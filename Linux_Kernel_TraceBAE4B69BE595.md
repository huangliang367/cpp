# Linux Kernel Trace 事件目录与查询方法

> 本文是常见事件子系统的分类参考，不是某一台机器上“全部事件”的固定清单。Linux trace event 的实际集合由内核版本、配置、架构、厂商补丁、模块和驱动决定。**以目标设备的 `/sys/kernel/tracing/available_events` 为准。**

## 1. 获取当前内核支持的完整事件列表

```bash
TRACEFS=/sys/kernel/tracing
test -d "$TRACEFS/events" || TRACEFS=/sys/kernel/debug/tracing

# 全部事件，格式通常为 subsystem:event
cat "$TRACEFS/available_events"

# 保存清单，方便版本间比较
cat "$TRACEFS/available_events" | sort > available_events.txt

# 事件所属 subsystem
find "$TRACEFS/events" -mindepth 1 -maxdepth 1 -type d \
  -printf '%f\n' | sort

# 输出每个事件的字段格式
find "$TRACEFS/events" -mindepth 2 -maxdepth 2 -type d | sort
```

建议把该文件与内核版本一起归档：

```bash
{
  uname -a
  echo "---- config ----"
  (zcat /proc/config.gz 2>/dev/null || cat "/boot/config-$(uname -r)" 2>/dev/null) |
    grep -E 'CONFIG_(TRACING|TRACEPOINTS|EVENT_TRACING|FTRACE|PERF_EVENTS|KPROBE_EVENTS|UPROBE_EVENTS)='
  echo "---- available_events ----"
  cat "$TRACEFS/available_events"
} > "kernel-trace-inventory-$(uname -r).txt"
```

> `find -printf` 不是所有 BusyBox 版本都支持；若命令不兼容，可用 `find ... -print`。

## 2. 常见 trace event 子系统分类

以下列举常见名称和用途。它们是**候选类别/常见事件名**，不是对所有 Linux 版本的保证；启用前请检查 `available_events` 和相应 `format` 文件。

### 2.1 `sched`：调度器

常见事件：

- `sched:sched_switch`：任务切换，分析谁让出 CPU、切换到哪个任务。
- `sched:sched_wakeup`：任务被唤醒。
- `sched:sched_wakeup_new`：新任务唤醒路径。
- `sched:sched_migrate_task`：任务在 CPU 之间迁移。
- `sched:sched_process_fork`：创建进程/线程。
- `sched:sched_process_exec`：执行新程序映像。
- `sched:sched_process_exit`：任务退出。
- `sched:sched_process_free`：任务结构释放（是否存在取决于版本）。
- `sched:sched_stat_runtime`：任务运行时间统计（需要相关配置/启用支持）。

典型用途：CPU 利用率、线程阻塞/唤醒、调度延迟、迁核、线程生命周期。

### 2.2 `irq` 与 `ipi`：中断

常见事件：

- `irq:irq_handler_entry`：进入硬中断处理函数。
- `irq:irq_handler_exit`：退出硬中断处理函数。
- `irq:softirq_entry`：进入 softirq。
- `irq:softirq_exit`：退出 softirq。
- `irq:softirq_raise`：触发/挂起 softirq。
- `ipi:*`：部分架构提供的处理器间中断事件，具体名称随架构和内核版本变化。

典型用途：中断频率、处理耗时、IRQ 风暴、softirq 占用、跨 CPU 通知。检查是否存在：

```bash
grep -E '^(irq|ipi):' "$TRACEFS/available_events"
```

### 2.3 `workqueue`：工作队列

可能包含：

- `workqueue:workqueue_queue_work`
- `workqueue:workqueue_activate_work`
- `workqueue:workqueue_execute_start`
- `workqueue:workqueue_execute_end`
- `workqueue:workqueue_cancel_work`

典型用途：分析工作何时入队、何时执行、执行耗时以及 worker 线程行为。具体事件名以本机清单为准。

### 2.4 `timer`、`hrtimer`：定时器

可能包含：

- `timer:timer_start`
- `timer:timer_expire_entry`
- `timer:timer_expire_exit`
- `timer:timer_cancel`
- `timer:hrtimer_start`
- `timer:hrtimer_expire_entry`
- `timer:hrtimer_expire_exit`

典型用途：定时器高频唤醒、延迟任务、定时器回调耗时、系统功耗线索。子系统与事件名可能随版本变化。

### 2.5 `syscalls`：系统调用

常见事件形态：

- `syscalls:sys_enter_<name>`
- `syscalls:sys_exit_<name>`

例如目标内核可能提供 `sys_enter_openat`、`sys_exit_openat`、`sys_enter_read`、`sys_exit_read` 等。不同架构、系统调用表和配置会影响实际清单。

典型用途：应用进入内核的系统调用行为、系统调用耗时、文件/网络调用频率。全量开启通常会产生大量数据，应只选择需要的调用。

### 2.6 `exceptions`：异常/系统调用入口相关事件

部分架构和版本会提供 `exceptions:*` 事件，例如系统调用或异常入口/退出事件。该类别并非所有架构都有统一的完整集合。应直接检查：

```bash
grep '^exceptions:' "$TRACEFS/available_events"
```

### 2.7 `block`：块设备 I/O

常见事件名可能包括：

- `block:block_rq_insert`
- `block:block_rq_issue`
- `block:block_rq_complete`
- `block:block_bio_queue`
- `block:block_bio_complete`
- `block:block_rq_requeue`
- `block:block_rq_merge`

典型用途：请求排队、设备下发、完成时间、I/O 延迟、合并与重排队。事件语义与字段会因块层实现和版本而变化。

### 2.8 `writeback`：页缓存回写

可能包含：

- `writeback:writeback_dirty_page`
- `writeback:writeback_write_inode`
- `writeback:writeback_written`
- `writeback:balance_dirty_pages`

典型用途：脏页增长、回写节奏、写入节流和存储延迟关联。

### 2.9 `kmem`、`kmalloc`/内存分配相关事件

常见 `kmem` 事件可能包括：

- `kmem:kmalloc`
- `kmem:kfree`
- `kmem:mm_page_alloc`
- `kmem:mm_page_free`
- `kmem:mm_page_alloc_zone_locked`
- `kmem:mm_page_pcpu_drain`

实际命名可能不同，有些分配器事件包含 `kmem` 前缀但不是所有内核都提供同样集合。

典型用途：分配热点、页面分配/释放、内存压力与分配失败分析。高频分配事件可能造成较大 trace 体积。

### 2.10 `vmscan`、`compaction`：回收与内存整理

可能包含：

- `vmscan:mm_vmscan_direct_reclaim_begin`
- `vmscan:mm_vmscan_direct_reclaim_end`
- `vmscan:mm_vmscan_kswapd_wake`
- `vmscan:mm_vmscan_kswapd_sleep`
- `compaction:mm_compaction_begin`
- `compaction:mm_compaction_end`

典型用途：直接回收、kswapd 活动、内存碎片、分配延迟与内存压力。

### 2.11 `ext4`、`f2fs`、`xfs` 等文件系统事件

文件系统 trace event 按文件系统各自命名，常见前缀例如 `ext4:*`、`f2fs:*`、`xfs:*`。具体事件常与 inode、extent、日志提交、数据写入和回写有关。

查询：

```bash
grep -E '^(ext4|f2fs|xfs|btrfs):' "$TRACEFS/available_events"
```

典型用途：文件系统操作路径、日志/事务、extent 分配、数据写入与 block 层延迟关联。

### 2.12 `net`、`skb`、`sock`、`tcp` 等网络事件

可能出现的类别包括：

- `net:*`：网络设备/协议栈相关事件。
- `skb:*`：socket buffer 生命周期或处理事件（若内核提供）。
- `sock:*`：socket 层事件。
- `tcp:*`、`udp:*`：协议相关事件（是否存在取决于版本与配置）。
- `napi:*`：NAPI 相关事件（依版本而异）。

典型用途：网络包处理、丢包线索、软中断、协议栈延迟。不是所有内核都包含这些子系统，需查询本机清单。

### 2.13 `power`、`cpu_frequency`、`cpu_idle`：电源与频率

常见事件可能包括：

- `power:cpu_frequency`
- `power:cpu_idle`
- `power:cpu_frequency_limits`
- `power:suspend_resume`
- `power:device_pm_callback_start`
- `power:device_pm_callback_end`

典型用途：CPU DVFS、idle 状态、系统 suspend/resume、设备电源管理回调耗时。事件名称和支持情况依平台而异。

### 2.14 `regulator`、`clk`、`rpm`：时钟与电源域

某些平台提供 `regulator:*`、`clk:*`、`rpm:*` 或厂商自定义事件。它们不是通用内核都具备的统一清单。

典型用途：时钟 enable/disable、频率变化、电源 rail、运行时电源管理。嵌入式 SoC 的厂商内核尤其需要核对其补丁。

### 2.15 `dma_fence`、`gpu`、`drm`：图形与同步

可能包含 `dma_fence:*`、`drm:*`，以及特定 GPU 驱动的自定义事件。具体取决于驱动与内核版本。

典型用途：fence 生命周期、GPU/显示提交与完成、渲染同步。GPU 驱动内部事件往往是平台专有的。

### 2.16 `rcu`：RCU

常见事件可能包括：

- `rcu:rcu_utilization`
- `rcu:grace_period`
- `rcu:rcu_callback`
- `rcu:rcu_dyntick`

实际事件名会随版本改变。典型用途：宽限期、回调积压、RCU stall 相关性能问题。

### 2.17 `lock`：锁与争用

可能包含：

- `lock:lock_acquire`
- `lock:lock_release`
- `lock:lock_contended`
- `lock:lock_acquired`

不是所有配置都提供全部事件。锁跟踪可能引入较高开销；优先针对疑似锁和短时间窗口采集。

### 2.18 `printk`、`ftrace` 与跟踪元事件

某些内核提供 `ftrace:*`、`printk:*` 等事件，用于跟踪框架或日志相关路径。事件清单和用途需按内核版本核实。

### 2.19 驱动/模块自定义子系统

驱动可通过 `TRACE_EVENT()` 定义自有子系统，例如：

```text
my_driver:device_transfer
my_driver:dma_submit
my_driver:dma_complete
```

也可能由厂商内核添加 `rockchip:*` 等平台专用事件；不要假设某个厂商前缀在所有版本中存在。

## 3. 不同事件类别的查询命令

### 3.1 按子系统查看

```bash
grep '^sched:' "$TRACEFS/available_events"
grep '^irq:' "$TRACEFS/available_events"
grep '^workqueue:' "$TRACEFS/available_events"
grep '^block:' "$TRACEFS/available_events"
grep '^power:' "$TRACEFS/available_events"
grep '^kmem:' "$TRACEFS/available_events"
```

### 3.2 查找某个事件的字段和过滤条件

```bash
cat "$TRACEFS/events/sched/sched_switch/format"
cat "$TRACEFS/events/sched/sched_switch/filter"
```

`format` 文件通常描述事件 ID、字段类型、字段名、偏移、长度，以及文本格式。过滤器中可用的字段应以 `format` 为准。

### 3.3 检查是否可启用

```bash
test -e "$TRACEFS/events/sched/sched_switch/enable" && echo "event exists"
cat "$TRACEFS/events/sched/sched_switch/enable"
```

启用一个事件：

```bash
echo 1 | sudo tee "$TRACEFS/events/sched/sched_switch/enable"
```

启用整个子系统：

```bash
echo 1 | sudo tee "$TRACEFS/events/sched/enable"
```

关闭所有事件：

```bash
echo 0 | sudo tee "$TRACEFS/events/enable"
```

也可以通过 `set_event` 管理：

```bash
echo 'sched:sched_switch' | sudo tee "$TRACEFS/set_event"
echo 'sched:sched_wakeup' | sudo tee -a "$TRACEFS/set_event"
# 清空事件选择
echo | sudo tee "$TRACEFS/set_event"
```

注意：对 `set_event` 使用 `>` 会替换此前事件列表；需要追加时使用 `>>` 或 `tee -a`。

## 4. 常用 Perfetto 采集配置片段

### 4.1 调度与中断

```text
data_sources {
  config {
    name: "linux.ftrace"
    ftrace_config {
      ftrace_events: "sched/sched_switch"
      ftrace_events: "sched/sched_wakeup"
      ftrace_events: "irq/irq_handler_entry"
      ftrace_events: "irq/irq_handler_exit"
      ftrace_events: "irq/softirq_entry"
      ftrace_events: "irq/softirq_exit"
    }
  }
}
```

某个事件若不在目标机 `available_events` 中，先从配置移除或换成本机实际存在的事件。

### 4.2 workqueue 与 timer

```text
data_sources {
  config {
    name: "linux.ftrace"
    ftrace_config {
      ftrace_events: "workqueue/*"
      ftrace_events: "timer/*"
    }
  }
}
```

通配符支持情况取决于 Perfetto/记录器版本；最稳妥的方式是明确列出本机存在且需要的事件。不要把示例中的通配符当作所有版本均支持的保证。

### 4.3 块 I/O

```text
data_sources {
  config {
    name: "linux.ftrace"
    ftrace_config {
      ftrace_events: "block/block_rq_insert"
      ftrace_events: "block/block_rq_issue"
      ftrace_events: "block/block_rq_complete"
    }
  }
}
```

先确认各事件存在。不同内核版本可能使用不同事件名或字段。

## 5. 内核事件不等于 Perfetto 专用轨道

可分为三种情况：

1. **标准事件 + Perfetto 内置解析**：例如部分调度事件会形成 scheduler tracks/slices。
2. **原始 ftrace 事件**：事件能被记录，在 `Ftrace Events` 中看到字段，但没有专用轨道。
3. **自定义事件 + 专用解析**：需遵循 Perfetto kernel track event conventions，或修改 Trace Processor 解析逻辑。

Perfetto SQL 查询时，可先检查数据库表：

```sql
SELECT name
FROM sqlite_master
WHERE type = 'table'
ORDER BY name;
```

再查询可用表。常见的 `sched_slice` 表仅在相关数据成功采集和解析后才会出现。

## 6. 事件选择建议

| 排查目标 | 优先考虑 |
|---|---|
| 线程卡顿、调度延迟 | `sched_switch`、`sched_wakeup`、必要时 `sched_migrate_task` |
| IRQ 过多/耗时 | `irq_handler_entry/exit`、`softirq_entry/exit` |
| workqueue 延迟 | `workqueue:*` 中实际存在的 queue/execute 事件 + `sched` |
| I/O 慢 | `block:*` + 文件系统子系统 + `sched` |
| 内存回收卡顿 | `vmscan:*`、`kmem:*`、`compaction:*` |
| CPU 频率/idle | `power:cpu_frequency`、`power:cpu_idle`（若存在） |
| 驱动操作耗时 | 自定义 `TRACE_EVENT`，必要时使用 Perfetto slice 字段约定 |
| 应用函数耗时 | Perfetto SDK Track Events |
| CPU 热点 | `linux.perf`/`perf` 采样；与 ftrace trace 合并时需检查时钟与配置 |
| 函数调用链 | `function_graph`，严格控制范围和持续时间 |

## 7. 重要限制与注意事项

- 事件名和字段不是跨所有 Linux 版本、架构、厂商内核都稳定一致的清单。
- tracepoint 的存在不代表它一定在当前内核启用或可供 Perfetto 读取。
- 全量启用 `*:*` 通常会造成数据量过大、丢事件和明显的性能扰动。
- 事件字段可能包含 PID、路径、设备标识等信息；分享 trace 前应检查隐私和敏感信息。
- 内核事件采集、Perfetto trace 格式和 UI 专用轨道解析是不同问题，需要分别验证。

## 8. 官方参考

- Linux Kernel Event Tracing：<https://docs.kernel.org/trace/events.html>
- Linux Kernel ftrace：<https://docs.kernel.org/trace/ftrace.html>
- Perfetto ftrace instrumentation：<https://perfetto.dev/docs/getting-started/ftrace>
- Perfetto kernel track event conventions：<https://perfetto.dev/docs/reference/kernel-track-event>
- Perfetto tracing SDK：<https://perfetto.dev/docs/instrumentation/tracing-sdk>
