# Linux Kernel、库与应用接入 Trace，并在 Perfetto UI 中分析

> 适用范围：Linux 5.10/6.x、ARM64（包括嵌入式开发板）、用户态 C/C++ 应用、共享库/静态库，以及 Perfetto UI。
>
> 核心原则：**采集格式、事件来源和 UI 解析是三个不同层次**。事件能被采集，不代表它一定自动变成理想的时间线轨道；通常可以先在 `Ftrace Events` 中查看原始事件，再根据需求决定是否需要结构化解析。

## 1. 总体架构

```text
Kernel
  ├─ 内建 tracepoints / trace events ──┐
  ├─ 自定义 TRACE_EVENT()              │
  ├─ ftrace function/function_graph    ├─ tracefs / ftrace ──┐
  ├─ kprobe / uprobe（取决于内核配置） │                     │
  └─ perf events / PMU                 ┘                     │
                                                             ├─ tracebox / traced
Userspace                                                     │
  ├─ Perfetto SDK Track Events ──────────────────────────────┤
  ├─ atrace/ATRACE 类事件（平台支持时）──────────────────────┤
  ├─ 自定义数据源 / protobuf ────────────────────────────────┘
                                                             ↓
                                              .pftrace / .perfetto-trace
                                                             ↓
                                                    https://ui.perfetto.dev
                                                             ↓
                                              Timeline、SQL、原始事件分析
```

- **内核事件**：通常经 `linux.ftrace` 数据源采集。
- **应用/库事件**：优先考虑 Perfetto Tracing SDK 的 `Track Events`。
- **跨层关联**：在同一次 trace 中同时启用 `linux.ftrace` 和 `track_event`，并尽量让事件使用可关联的 PID/TID、时间戳和语义。
- **原始数据导入**：如果现有数据不是 trace event，而是自定义采样值/日志/二进制记录，可以写转换器生成 Perfetto Trace protobuf，或实现自定义数据源与 Trace Processor 解析。

## 2. 开始前：确认内核和 tracefs

### 2.1 检查配置

在目标 Linux 设备执行：

```bash
uname -a
grep -E 'CONFIG_(TRACING|TRACEPOINTS|EVENT_TRACING|FTRACE|FUNCTION_TRACER|FUNCTION_GRAPH_TRACER|KPROBE_EVENTS|UPROBE_EVENTS|PERF_EVENTS)=' \
    /boot/config-$(uname -r) 2>/dev/null
```

若系统不提供 `/boot/config-*`，尝试：

```bash
zcat /proc/config.gz 2>/dev/null | grep -E 'CONFIG_(TRACING|TRACEPOINTS|EVENT_TRACING|FTRACE|FUNCTION_TRACER|FUNCTION_GRAPH_TRACER|KPROBE_EVENTS|UPROBE_EVENTS|PERF_EVENTS)='
```

常见配置含义：

- `CONFIG_TRACING`、`CONFIG_TRACEPOINTS`、`CONFIG_EVENT_TRACING`：事件跟踪基础设施。
- `CONFIG_FTRACE`：ftrace 框架。
- `CONFIG_FUNCTION_TRACER`、`CONFIG_FUNCTION_GRAPH_TRACER`：函数/函数图跟踪。
- `CONFIG_KPROBE_EVENTS`、`CONFIG_UPROBE_EVENTS`：通过 kprobe/uprobe 动态插桩（具体能力与架构、内核配置有关）。
- `CONFIG_PERF_EVENTS`：perf 事件基础设施。

### 2.2 挂载 tracefs

```bash
sudo mount -t tracefs tracefs /sys/kernel/tracing 2>/dev/null || true
mount | grep tracefs
```

旧系统有时将 tracing 挂载在 `/sys/kernel/debug/tracing`：

```bash
mount | grep -E 'tracefs|debugfs'
```

优先使用 `/sys/kernel/tracing`。以下示例统一用变量：

```bash
TRACEFS=/sys/kernel/tracing
test -d "$TRACEFS/events" || TRACEFS=/sys/kernel/debug/tracing
echo "$TRACEFS"
```

### 2.3 列出当前内核实际支持的事件

```bash
cat "$TRACEFS/available_events"
find "$TRACEFS/events" -mindepth 2 -maxdepth 2 -name enable -print
```

**不要把其他内核版本的事件清单当作本机的权威清单。** 可用事件由内核版本、厂商补丁、配置选项和已加载模块共同决定。

## 3. 采集已有内核事件

### 3.1 用 tracefs 手动启用事件

例如查看调度与 IRQ：

```bash
# 先关闭旧配置，避免把之前的事件混进来
echo 0 | sudo tee "$TRACEFS/tracing_on"
echo | sudo tee "$TRACEFS/set_event"

# 只启用需要的事件
echo 'sched:sched_switch' | sudo tee "$TRACEFS/set_event"
echo 'sched:sched_wakeup' | sudo tee -a "$TRACEFS/set_event"
echo 'irq:irq_handler_entry' | sudo tee -a "$TRACEFS/set_event"
echo 'irq:irq_handler_exit' | sudo tee -a "$TRACEFS/set_event"

echo 1 | sudo tee "$TRACEFS/tracing_on"
sleep 5
echo 0 | sudo tee "$TRACEFS/tracing_on"

sudo cat "$TRACEFS/trace"
```

实时查看事件：

```bash
sudo cat "$TRACEFS/trace_pipe"
```

`trace_pipe` 是消费式读取接口，适合实时查看；保存正式记录时优先使用 Perfetto 或 `trace` 文件快照。

### 3.2 使用过滤器降低开销

查看事件字段：

```bash
cat "$TRACEFS/events/sched/sched_switch/format"
```

按字段过滤（字段必须存在于对应事件的 `format` 文件中）：

```bash
cat "$TRACEFS/events/sched/sched_switch/format"
echo 'prev_pid == 1234' | sudo tee "$TRACEFS/events/sched/sched_switch/filter"
```

清除过滤器：

```bash
echo 0 | sudo tee "$TRACEFS/events/sched/sched_switch/filter"
```

还可使用 `set_event_pid`（若本机提供）限制记录的任务 PID。注意：PID 过滤的语义可能受到事件上下文、内核版本和跟踪配置影响；应通过实际 trace 验证。

### 3.3 使用 function_graph 跟踪函数耗时

先检查支持的 tracer：

```bash
cat "$TRACEFS/available_tracers"
```

若包含 `function_graph`：

```bash
echo 0 | sudo tee "$TRACEFS/tracing_on"
echo nop | sudo tee "$TRACEFS/current_tracer"
echo function_graph | sudo tee "$TRACEFS/current_tracer"
# 可选：限制函数，具体格式请看本机 set_graph_function 文件
echo my_driver_function | sudo tee "$TRACEFS/set_graph_function"
echo 1 | sudo tee "$TRACEFS/tracing_on"
sleep 2
echo 0 | sudo tee "$TRACEFS/tracing_on"
sudo cat "$TRACEFS/trace"
```

函数图事件量可能很大，建议缩小函数范围并控制采集时间。Perfetto 的 ftrace 数据源支持在合适配置下记录函数图信息；具体字段随 Perfetto 版本变化，请参考官方 ftrace 配置文档。

## 4. 通过 tracebox 采集，并在 Perfetto UI 打开

### 4.1 安装/准备 tracebox

Perfetto 提供 `tracebox` 作为系统 trace 记录工具。准备与目标架构匹配的二进制，并确认：

```bash
./tracebox --version
./tracebox --help
```

不同发行包、构建选项和版本可能在参数上略有差异。下面采用常见的 `-c CONFIG --txt -o OUTPUT` 用法；若本机 `--help` 不同，以实际输出为准。

### 4.2 最小 ftrace 配置

创建 `kernel-trace.cfg`：

```text
buffers {
  size_kb: 32768
  fill_policy: DISCARD
}

duration_ms: 10000

data_sources {
  config {
    name: "linux.ftrace"
    target_buffer: 0
    ftrace_config {
      ftrace_events: "sched/sched_switch"
      ftrace_events: "sched/sched_wakeup"
      ftrace_events: "irq/irq_handler_entry"
      ftrace_events: "irq/irq_handler_exit"
    }
  }
}
```

运行：

```bash
sudo ./tracebox -c kernel-trace.cfg --txt -o kernel-trace.perfetto-trace
```

如果工具版本使用其他参数形式，请根据 `tracebox --help` 调整。记录成功后，检查文件大小和退出状态。

### 4.3 打开 Perfetto UI

1. 在电脑浏览器打开 <https://ui.perfetto.dev>。
2. 将 `kernel-trace.perfetto-trace` 拖入页面，或使用 `Ctrl+O` 打开文件。
3. 展开 `Ftrace Events`、CPU、进程/线程和调度相关轨道。
4. 选择一个事件查看时间戳、CPU、PID/TID 与字段。
5. 可通过 SQL 查询底层数据，例如：

```sql
SELECT
  ts,
  dur,
  name,
  cpu,
  utid,
  pid,
  tid
FROM sched_slice
ORDER BY ts
LIMIT 100;
```

不同版本的表名、列和解析结果可能不同；在 UI 的 Query 页面中先检查可用表：

```sql
SELECT name
FROM sqlite_master
WHERE type = 'table'
ORDER BY name;
```

若事件只显示在原始 `Ftrace Events` 轨道，并没有自动形成专用 slice/counter 轨道，这不一定代表采集失败。Perfetto 对部分标准事件有专门解析；自定义事件通常需要遵循约定字段，或扩展 Trace Processor。

## 5. 在内核模块/驱动中添加自定义 Trace Event

### 5.1 何时使用 TRACE_EVENT

适用于驱动、子系统或模块需要稳定、低开销、可过滤且字段明确的事件。优先使用静态 tracepoint，而不是在高频路径中随意增加 `printk()`。

推荐的文件布局：

```text
my_trace_module/
├── Makefile
├── my_trace_module.c
└── trace/
    └── events/
        └── my_driver.h
```

### 5.2 定义事件头文件

`trace/events/my_driver.h`：

```c
#undef TRACE_SYSTEM
#define TRACE_SYSTEM my_driver

#if !defined(_TRACE_MY_DRIVER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MY_DRIVER_H

#include <linux/tracepoint.h>

TRACE_EVENT(device_transfer,
    TP_PROTO(int device_id, size_t bytes, int result),
    TP_ARGS(device_id, bytes, result),

    TP_STRUCT__entry(
        __field(int, device_id)
        __field(size_t, bytes)
        __field(int, result)
    ),

    TP_fast_assign(
        __entry->device_id = device_id;
        __entry->bytes = bytes;
        __entry->result = result;
    ),

    TP_printk("device=%d bytes=%zu result=%d",
        __entry->device_id, __entry->bytes, __entry->result)
);

#endif /* _TRACE_MY_DRIVER_H */

#include <trace/define_trace.h>
```

### 5.3 在源文件中定义和触发事件

```c
#define CREATE_TRACE_POINTS
#include "trace/events/my_driver.h"

static int do_transfer(int device_id, size_t bytes)
{
    int ret;

    /* 实际传输逻辑 */
    ret = hardware_transfer(device_id, bytes);

    trace_device_transfer(device_id, bytes, ret);
    return ret;
}
```

说明：

- `CREATE_TRACE_POINTS` 只应在一个 `.c` 文件中定义。
- 头文件中的 include 路径和构建路径必须正确。
- 如果事件属于可独立构建的外部模块，要检查目标内核的 tracepoint 导出、符号可见性、内核版本与模块构建方式。某些场景更适合将 tracepoint 放进内核树内构建。
- `TP_printk()` 定义 tracefs 文本展示格式；Perfetto 读取的通常是结构化字段，不应假设 Perfetto 一定按 `TP_printk()` 字符串渲染。

### 5.4 验证新事件

模块加载后：

```bash
grep '^my_driver:' "$TRACEFS/available_events"
cat "$TRACEFS/events/my_driver/device_transfer/format"
echo 1 | sudo tee "$TRACEFS/events/my_driver/device_transfer/enable"
echo 1 | sudo tee "$TRACEFS/tracing_on"
sudo cat "$TRACEFS/trace_pipe"
```

Perfetto 配置中增加：

```text
ftrace_events: "my_driver/device_transfer"
```

### 5.5 让自定义事件自动形成 Perfetto 时间线 Slice

Perfetto 提供一套约定：在事件字段中使用指定名称和类型，可让 Trace Processor 自动将事件映射成 slice/instant。常见字段：

- `char track_event_type`：`'B'` 开始 slice，`'E'` 结束 slice，`'I'` instant。
- `__string(slice_name, slice_name)`：事件名称。
- 进程级分组可额外使用 `int scope_tgid`。

这是 Perfetto 的解析约定，并非 Linux 内核的通用 ABI。应以官方 `Kernel track events: format and conventions` 文档为准，并验证所用 Perfetto 版本的行为。若只需要原始事件，普通 `TRACE_EVENT` 已足够。

## 6. 在 C/C++ 应用和库中埋点

### 6.1 推荐方案：Perfetto Tracing SDK Track Events

适合新建或可修改源码的 C++ 应用/库。SDK 支持 slice、instant、counter、track 和自定义参数，能与同一份系统 trace 中的内核事件并存。

简化示例（初始化代码省略了项目构建和错误处理细节）：

```cpp
#include <perfetto.h>

PERFETTO_DEFINE_CATEGORIES(
    perfetto::Category("app")
        .SetDescription("Application trace events"),
    perfetto::Category("storage")
        .SetDescription("Storage operations")
);

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

void DoWork() {
  TRACE_EVENT("app", "DoWork");

  {
    TRACE_EVENT("storage", "ReadConfig");
    // read config ...
  }

  TRACE_EVENT_INSTANT("app", "WorkCheckpoint",
                      perfetto::Track::Current());
}
```

应用启动阶段需要初始化并注册 Track Event 数据源（示意）：

```cpp
perfetto::TracingInitArgs args;
args.backends |= perfetto::kInProcessBackend;
perfetto::Tracing::Initialize(args);
perfetto::TrackEvent::Register();
```

选择 `kInProcessBackend` 还是 `kSystemBackend` 取决于部署方式：

- **In-process**：应用内部独立记录，适合应用自有 trace；通常由应用控制会话。
- **System backend**：连接系统 tracing service，适合与系统级数据源合并记录；目标设备必须具备相应服务/后端和权限。
- 嵌入式 Linux 的可用 backend、SDK 版本、服务部署方式必须按实际构建产物确认，不能仅凭桌面示例推断目标设备支持。

SDK 的 CMake/Bazel/源码集成方式会随版本调整。请参考官方 Tracing SDK 与 Track Events 文档，并使用与应用集成的 SDK 版本配套的示例。

### 6.2 采集应用 Track Events

在系统 trace 配置中加入 `track_event`：

```text
buffers {
  size_kb: 32768
  fill_policy: DISCARD
}

duration_ms: 10000

data_sources {
  config {
    name: "linux.ftrace"
    ftrace_config {
      ftrace_events: "sched/sched_switch"
      ftrace_events: "sched/sched_wakeup"
    }
  }
}

data_sources {
  config {
    name: "track_event"
    track_event_config {
      enabled_categories: "app"
      enabled_categories: "storage"
      disabled_categories: "*"
    }
  }
}
```

运行系统记录器时，应用必须在记录窗口内运行，并且已连接到所配置的 backend。UI 中应用事件会出现在相应进程/线程轨道上；内核调度事件可用于分析应用线程何时运行、何时被唤醒或被抢占。

### 6.3 库如何埋点

共享库/静态库可以直接使用 Track Event API，但需统一管理：

1. **类别命名**：如 `mylib.io`、`mylib.parse`，避免与应用类别冲突。
2. **初始化所有权**：通常由主程序初始化 tracing；库不要重复初始化全局 tracing 系统。
3. **跨线程操作**：普通 slice 应在同一线程成对开始/结束。异步操作跨线程时，使用适合的 track/flow 设计，不能简单假定线程 slice 自动匹配。
4. **发布与开销**：为 release 构建控制类别和埋点频率；避免在极高频路径中记录大字符串或大量参数。
5. **静态库**：链接时确保 SDK 依赖、静态存储定义和必要注册只出现一次，遵循所用 SDK 版本的 ODR/链接要求。
6. **ABI**：尽量不要让公共 C ABI 暴露 Perfetto C++ 类型；可以在库内部用 C++ 封装，并向 C 接口提供可选 trace hook。

### 6.4 已有程序不方便改源码时

按优先级考虑：

- 使用 `perf`/硬件采样做调用栈性能分析（需内核与 PMU 支持）。
- 使用 uprobe 对用户态函数进行动态探测（需 `CONFIG_UPROBE_EVENTS` 等支持，并注意符号、优化、内联和 ABI 变化）。
- 用 `LD_PRELOAD` 包装目标动态库的导出函数，在包装层埋点；这对静态链接、setuid 程序、符号隐藏或内联代码无效。
- 对外部日志或自定义采样数据写转换器，将其转成 Perfetto protobuf。
- GDB/strace 等更适合调试或系统调用观察，不等同于低开销的 Perfetto 事件埋点。

## 7. 库或应用产生的自定义数据，如何导入 Perfetto

若采集的数据不是 ftrace 事件，也没有使用 Track Event SDK，通常有三条路：

### 路线 A：转换成 Perfetto Trace protobuf

将数据转换为 Perfetto 支持的 trace packet / track event 格式，再输出 `.pftrace`。优点是可以离线导入；缺点是需要正确处理时间戳、track、进程/线程元数据和 protobuf 格式。

### 路线 B：实现 Perfetto 自定义数据源

适用于持续采集、数据量大、需要强类型 schema 的场景。除了写入 trace packet，还要考虑 Trace Processor 如何解析这些数据，才能得到易查询的 SQL 表或专用 UI 轨道。

### 路线 C：使用 Track Event SDK

如果数据本质是函数耗时、阶段事件、计数器或流程关联，优先使用 Track Events，通常不需要修改 Perfetto 本身。

**重要区分**：把数据写进 trace 文件，只解决“数据进入 trace”；要形成专用轨道、slice 或 counter，还需要 Trace Processor 认识其 schema，或采用 Perfetto 已支持的事件约定。

## 8. 自定义事件的设计建议

| 需求 | 推荐表示方式 | 例子 |
|---|---|---|
| 函数/操作耗时 | Begin/End slice 或 SDK `TRACE_EVENT` | `ProbeDevice` |
| 单个时间点 | Instant event | `IRQ handled` |
| 随时间变化的数值 | Counter track | 队列深度、温度、内存用量 |
| 跨线程/异步任务关联 | Flow / async slice | 提交请求 → worker 完成 |
| 内核驱动事件 | `TRACE_EVENT` | `dma_submit`、`dma_done` |
| 函数级内核调用耗时 | ftrace `function_graph` | 驱动函数调用链 |
| CPU 性能采样 | `linux.perf` / perf | cycles、instructions、采样调用栈 |
| 任意结构化业务数据 | 自定义 data source + parser，或转换为 track event | 设备状态快照 |

设计字段时应包含必要的关联键（device ID、request ID、PID/TID 等），但避免记录敏感数据、巨型字符串或无必要的高频字段。

## 9. 常见故障排查

### 9.1 `available_events` 中没有事件

- 检查内核配置、当前内核版本和厂商补丁。
- 确认 tracefs 已挂载。
- 确认相关模块已加载；某些模块事件仅在模块加载后出现。
- 检查 `/sys/kernel/tracing/events/<subsystem>/<event>/format` 是否存在。

### 9.2 trace 文件生成了，但 UI 没有预期轨道

- 确认配置里数据源名称正确（如 `linux.ftrace`、`track_event`）。
- 确认事件在采集窗口内实际发生。
- 检查 trace 的 `Ftrace Events` 原始轨道。
- 自定义事件没有专用 parser 时，可能只显示为原始事件；按 Perfetto 约定设计字段或扩展 Trace Processor。
- 检查是否因权限、buffer 太小、DISCARD 策略或事件频率过高而丢失数据。

### 9.3 应用事件缺失

- 检查应用是否注册 Track Event 数据源、类别是否启用、backend 是否正确。
- 确认应用与 trace recorder 的连接方式匹配。
- 若使用 system backend，检查目标系统的 tracing service/权限；若使用 in-process backend，确认 trace session 的启动和停止逻辑。
- 确认事件并未被编译期配置、类别过滤或采集配置关闭。

### 9.4 trace 开销过大或丢事件

- 缩短采集窗口、减小事件范围、使用 filter。
- 不要一开始就启用 `*:*`。
- 增大 buffer 前先估算事件速率与持续时间。
- 高开销的 `function_graph`、高频 kprobe/uprobe 和大量字符串事件应谨慎使用。
- 采集时避免同时启用多个功能重叠的 tracing 工具，避免它们相互覆盖 tracefs 状态。

## 10. 建议的实战顺序

1. 先采集 `sched_switch`、`sched_wakeup`，验证 tracebox → Perfetto UI 全链路。
2. 增加 IRQ、workqueue、block、网络等与问题相关的标准事件。
3. 在驱动中增加一个低频 `TRACE_EVENT`，检查 `format` 和原始轨道。
4. 需要更清晰的自定义时间线时，按 Perfetto kernel track event 约定增加 slice/counter 字段。
5. 在 C++ 应用或库中加入 Track Event SDK，并与 `linux.ftrace` 同时采集。
6. 对自定义采样/日志数据，先明确其时间戳、track、字段和查询需求，再选择转换器或自定义 data source。

## 11. 官方文档

- Perfetto：Instrumenting the Linux kernel with ftrace  
  <https://perfetto.dev/docs/getting-started/ftrace>
- Perfetto：Kernel track events: format and conventions  
  <https://perfetto.dev/docs/reference/kernel-track-event>
- Perfetto：Tracing SDK  
  <https://perfetto.dev/docs/instrumentation/tracing-sdk>
- Perfetto：Track Events  
  <https://perfetto.dev/docs/instrumentation/track-events>
- Perfetto：Recording in-app traces  
  <https://perfetto.dev/docs/getting-started/in-app-tracing>
- Linux Kernel：Event Tracing  
  <https://docs.kernel.org/trace/events.html>
- Linux Kernel：ftrace  
  <https://docs.kernel.org/trace/ftrace.html>
