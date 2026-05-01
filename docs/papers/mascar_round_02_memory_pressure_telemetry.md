# Mascar Round 02：Memory Pressure Telemetry

**阶段**：Phase 2
**日期**：2026-05-01
**分支**：hrl/paper/mascar-repro-v0

---

## 目标

实现 passive telemetry，用于观测 memory pressure / pitstop 信号。
不改变 scheduler 行为。

---

## 改动文件

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| src/gpgpu-sim/shader.h | 修改 | 添加 mascar_record_mem_stall / mascar_reset_stall_streak 方法 |
| src/gpgpu-sim/shader.cc | 修改 | scheduler_unit::cycle() 添加 telemetry hook |
| configs/hrl-repro/SM7_QV100_mascar_telemetry_on/ | 新建 | telemetry_on config |

---

## Telemetry Hook 位置

`scheduler_unit::cycle()` 中 `m_mem_out->has_free()` 检查处：
- 成功 issue：调用 `mascar_reset_stall_streak(warp_id)` 重置 streak
- 失败（m_mem_out 满）：调用 `mascar_record_mem_stall(warp_id)` 记录 stall 事件

---

## 验证结果

| Workload | Config | sim_cycle | mem_stall_event | saturation | pitstop | PASS |
|----------|--------|-----------|-----------------|------------|---------|------|
| vecadd | noop_off | 5569 | 0 | 0 | 0 | PASS (feature_off 不变) |
| hotspot | noop_off | 6931 | 0 | 0 | 0 | PASS (feature_off 不变) |
| vecadd | telemetry_on | 5569 | 57 | 0 | 0 | PASS (cycle 不变) |
| hotspot | telemetry_on | 6931 | 12150 | 0 | 0 | PASS (cycle 不变；stall 信号有) |
| srad_v2 | telemetry_on | 8236 | 3989 | 0 | 0 | PASS (cycle 不变；stall 信号有) |
| bfs | telemetry_on | 6665 | 1 | 0 | 0 | PASS (cycle 不变；bfs 低 mem pressure) |
| strided_access | telemetry_on | 5825 | 57 | 0 | 0 | PASS (cycle 不变) |

---

## 信号解读

- `mem_stall_event`：每次 warp 内存指令因 m_mem_out 满而无法 issue，递增
  - hotspot（12150）和 srad_v2（3989）有明显信号 → 内存密集 workload
  - vecadd/strided_access（57）低信号 → 内存压力轻
  - bfs（1）最低 → 几乎无 structural stall
- `pitstop_event = 0` for all：per-warp stall_streak 未能连续达到 threshold=8
  - 原因：GPGPU-Sim 的 round-robin 调度使 per-warp streak 难以连续积累到 8
  - 不影响 Phase 3：would_deprioritize 将使用更低的触发阈值（streak >= 2）
- `saturation_event = 0`：是 pitstop=0 的衍生结果

---

## 已知近似

pitstop_event 阈值 threshold=8 对于 tiny workloads 过于保守。Phase 3 将使用
streak >= 2 作为 would_deprioritize 触发条件，Phase 4 保持 threshold=8 用于
实际 skip（防 deadlock 更保守）。

---

## 成功标准评估

- [x] 编译通过
- [x] feature_off cycle 不变
- [x] telemetry_on cycle 不变
- [x] telemetry signal 合理（mem_stall_event 对内存密集 workload 有信号）
- [x] 无真实 policy 行为

Phase 2 通过，可以进入 Phase 3。
