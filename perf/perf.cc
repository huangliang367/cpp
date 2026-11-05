
#include "trace_categories.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

class Observer : public perfetto::TrackEventSessionObserver {
public:
  Observer() { perfetto::TrackEvent::AddSessionObserver(this); }
  ~Observer() override { perfetto::TrackEvent::RemoveSessionObserver(this); }

  void OnStart(const perfetto::DataSourceBase::StartArgs &) override {
    std::unique_lock<std::mutex> lock(mutex);
    cv.notify_one();
  }

  void WaitForTracingStart() {
    PERFETTO_LOG("Waiting for tracing to start...");
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [] { return perfetto::TrackEvent::IsEnabled(); });
    PERFETTO_LOG("Tracing started");
  }

  std::mutex mutex;
  std::condition_variable cv;
};

void InitializePerfetto() {
  perfetto::TracingInitArgs args;
  // The backends determine where trace events are recorded. For this example we
  // are going to use the system-wide tracing service, so that we can see our
  // app's events in context with system profiling information.
  args.backends = perfetto::kSystemBackend;
  args.enable_system_consumer = false;

  perfetto::Tracing::Initialize(args);
  perfetto::TrackEvent::Register();
}

} // namespace

using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;

static int64_t now_ns() {
  return std::chrono::duration_cast<Nanoseconds>(
             Clock::now().time_since_epoch())
      .count();
}

struct Statistics {
  double min = 0;
  double max = 0;
  double avg = 0;
  double p50 = 0;
  double p90 = 0;
  double p95 = 0;
  double p99 = 0;
};

static double percentile(const std::vector<double> &sorted, double p) {
  if (sorted.empty())
    return 0;

  size_t index = static_cast<size_t>(p * (sorted.size() - 1));

  return sorted[index];
}

static Statistics calculate_statistics(std::vector<double> samples) {
  Statistics s;

  if (samples.empty())
    return s;

  std::sort(samples.begin(), samples.end());

  s.min = samples.front();
  s.max = samples.back();

  s.avg = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();

  s.p50 = percentile(samples, 0.50);
  s.p90 = percentile(samples, 0.90);
  s.p95 = percentile(samples, 0.95);
  s.p99 = percentile(samples, 0.99);

  return s;
}

class NotifyPair {
public:
  NotifyPair(unsigned int iterations, unsigned int warmup, uint64_t request_id)
      : iterations_(iterations), warmup_(warmup), request_id(request_id) {
    samples_.reserve(iterations);
  }

  void run() {
    waiter_ = std::thread(&NotifyPair::waiter_thread, this);

    try {
      notifier_ = std::thread(&NotifyPair::notifier_thread, this);
    } catch (...) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
      }
      notify_cv_.notify_one();
      waiter_.join();
      throw;
    }
  }

  void join() {
    if (notifier_.joinable())
      notifier_.join();
    if (waiter_.joinable())
      waiter_.join();

    if (thread_error_)
      std::rethrow_exception(thread_error_);
  }

  const std::vector<double> &samples() const { return samples_; }

private:
  unsigned int iterations_;
  unsigned int warmup_;
  uint64_t request_id;

  std::thread waiter_;
  std::thread notifier_;
  std::mutex mutex_;

  std::condition_variable notify_cv_;
  std::condition_variable ack_cv_;
  bool stopping_ = false;
  std::exception_ptr thread_error_;

  // 当前通知序号
  uint64_t sequence_ = 0;

  // Waiter 已经完成的序号
  uint64_t ack_sequence_ = 0;

  // Notifier 发出通知前记录的时间戳
  std::atomic<int64_t> notify_timestamp_{0};

  std::vector<double> samples_;

  void notifier_thread() {
    try {
      for (uint64_t i = 1; i <= iterations_ + warmup_; ++i) {

        {
          std::unique_lock<std::mutex> lock(mutex_);

          // 等待 Waiter 完成上一轮测量
          ack_cv_.wait(lock,
                       [&] { return ack_sequence_ == i - 1 || stopping_; });

          if (stopping_)
            return;

          sequence_ = i;
        }

        // 在 notify_one() 之前记录时间戳
        notify_timestamp_.store(now_ns(), std::memory_order_release);

        // TRACE_EVENT_INSTANT("app", "start_notify", perfetto::Flow::ProcessScoped(request_id));
        // 唤醒对应的 Waiter
        notify_cv_.notify_one();
      }
    } catch (...) {
      report_thread_error();
    }
  }

  void waiter_thread() {
    try {
      uint64_t last_sequence = 0;

      for (uint64_t i = 1; i <= iterations_ + warmup_; ++i) {

        int64_t latency_ns = 0;

        {
          std::unique_lock<std::mutex> lock(mutex_);

          // 等待 Notifier 发出通知
          notify_cv_.wait(
              lock, [&] { return sequence_ != last_sequence || stopping_; });

          if (stopping_)
            return;

          // Waiter 已经被唤醒并重新获得 mutex
          const int64_t wakeup_time = now_ns();

          const int64_t notify_time =
              notify_timestamp_.load(std::memory_order_acquire);

        //   TRACE_EVENT("app", "waked", perfetto::TerminatingFlow::ProcessScoped(request_id));
        //   TRACE_EVENT_INSTANT("app", "waked", perfetto::TerminatingFlow::ProcessScoped(request_id));
          TRACE_EVENT_INSTANT("app", "waked time", "timestamp_ns",
                              wakeup_time);
          TRACE_EVENT_INSTANT("app", "notify time", "timestamp_ns",
                              notify_time);
          latency_ns = wakeup_time - notify_time;

          last_sequence = sequence_;

          // 告诉 Notifier，本轮已经完成
          ack_sequence_ = last_sequence;
        }

        // 唤醒 Notifier，允许下一轮通知
        ack_cv_.notify_one();

        if (i > warmup_) {
          samples_.push_back(static_cast<double>(latency_ns));
        }
      }
    } catch (...) {
      report_thread_error();
    }
  }

  void report_thread_error() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!thread_error_)
        thread_error_ = std::current_exception();
      stopping_ = true;
    }
    notify_cv_.notify_one();
    ack_cv_.notify_one();
  }
};

static unsigned int parse_arg(const char *value, const char *name) {
  char *end = nullptr;

  unsigned long result = std::strtoul(value, &end, 10);

  if (value == end || *end != '\0' || result == 0 || result > 100000000UL) {
    throw std::invalid_argument(std::string("Invalid argument: ") + name);
  }

  return static_cast<unsigned int>(result);
}

static void print_statistics(const std::string &name, const Statistics &s) {
  std::cout << std::left << std::setw(12) << name << std::right << std::fixed
            << std::setprecision(2) << std::setw(12) << s.min << std::setw(12)
            << s.avg << std::setw(12) << s.p50 << std::setw(12) << s.p90
            << std::setw(12) << s.p95 << std::setw(12) << s.p99 << std::setw(12)
            << s.max << '\n';
}

int main(int argc, char *argv[]) {

  InitializePerfetto();

  Observer observer;
  observer.WaitForTracingStart();

  try {
    // 默认创建 4 对线程
    unsigned int pairs = argc > 1 ? parse_arg(argv[1], "pairs") : 4;

    // 每对线程测试次数
    unsigned int iterations =
        argc > 2 ? parse_arg(argv[2], "iterations") : 100000;

    // 预热次数
    unsigned int warmup = argc > 3 ? parse_arg(argv[3], "warmup") : 1000;

    std::cout << "Notify Benchmark\n";
    std::cout << "==============================\n";
    std::cout << "Thread pairs : " << pairs << '\n';
    std::cout << "Iterations   : " << iterations << '\n';
    std::cout << "Warmup       : " << warmup << '\n';
    std::cout << "CPU affinity : disabled\n\n";

    std::deque<NotifyPair> benchmarks;

    for (unsigned int i = 0; i < pairs; ++i) {
      benchmarks.emplace_back(iterations, warmup, i + 1);
    }

    // Start all pairs before joining so they run concurrently.
    try {
      for (auto &benchmark : benchmarks)
        benchmark.run();
    } catch (...) {
      for (auto &benchmark : benchmarks)
        benchmark.join();
      throw;
    }

    for (auto &benchmark : benchmarks)
      benchmark.join();

    // 汇总所有线程对的测量结果
    std::vector<double> all_samples;

    for (unsigned int i = 0; i < pairs; ++i) {
      const auto &samples = benchmarks[i].samples();

      all_samples.insert(all_samples.end(), samples.begin(), samples.end());

      std::cout << "\nPair " << i << '\n';

      print_statistics("Latency(ns)", calculate_statistics(samples));
    }

    std::cout << "\n========== Overall ==========\n";

    std::cout << std::left << std::setw(12) << "Metric" << std::right
              << std::setw(12) << "Min" << std::setw(12) << "Avg"
              << std::setw(12) << "P50" << std::setw(12) << "P90"
              << std::setw(12) << "P95" << std::setw(12) << "P99"
              << std::setw(12) << "Max" << '\n';

    print_statistics("Latency(ns)", calculate_statistics(all_samples));

    std::cout << "\nTotal samples: " << all_samples.size() << '\n';

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    std::cerr << "Usage: notify_benchmark [pairs] "
              << "[iterations] [warmup]\n";
    return 1;
  }

  perfetto::TrackEvent::Flush();

  return 0;
}
