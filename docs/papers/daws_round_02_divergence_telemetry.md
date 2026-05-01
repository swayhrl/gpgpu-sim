# DAWS Round 02: Divergence Telemetry Instrumentation

**Round**: AUTO-4
**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0

---

## 目标

在 `scheduler_unit::cycle()` 中添加 passive divergence telemetry，当 `active_count() < warp_size` 时记录 divergence event。不改变调度行为。

---

## 实现位置

**`src/gpgpu-sim/shader.h`**：
- `shader_core_ctx` 新增方法 `daws_record_divergence(unsigned active_count)`（inline）
- `shader_core_ctx` 新增方法 `get_daws_telemetry_stats(...)` 用于 cluster 聚合
- `shader_core_ctx` 新增成员变量：
  - `m_daws_divergence_event`
  - `m_daws_active_thread_sum`
  - `m_daws_active_thread_samples`
  - `m_daws_min_active_seen`
- `simt_core_cluster` 新增 `get_daws_telemetry_stats(...)` 声明

**`src/gpgpu-sim/shader.cc`**：
- `shader_core_ctx` 构造函数：初始化 4 个 DAWS 计数器
- `scheduler_unit::cycle()`：在 `active_mask` 计算后插入 telemetry hook（约 line 1355）
- `simt_core_cluster::get_daws_telemetry_stats()`：跨 SM 聚合

**`src/gpgpu-sim/gpu-sim.cc`**：
- `print_stats()` 中替换 placeholder 为实际计数器读取

---

## Telemetry Hook 逻辑

```cpp
// 在 scheduler_unit::cycle() 中，active_mask 计算后：
unsigned ac = active_mask.count();
if (ac < (unsigned)m_shader->m_config->warp_size)
    m_shader->daws_record_divergence(ac);
```

`daws_record_divergence()` 仅在 `gpgpu_enable_daws=1 && gpgpu_daws_enable_telemetry=1` 时执行，否则立即返回。

---

## 验证结果

| Workload | Config | sim_cycle | divergence_event | min_active | throttle_block | pass |
|----------|--------|-----------|-----------------|------------|----------------|------|
| vecadd | noop_off | 5569 | 0 | 0 | 0 | ✓ |
| hotspot | noop_off | 6931 | 0 | 0 | 0 | ✓ |
| bfs | noop_off | 6665 | 0 | 0 | 0 | ✓ |
| vecadd | telemetry_on | 5569 | 0 | 0 | 0 | ✓ 无分支，预期 |
| hotspot | telemetry_on | 6931 | 7655 | 8 | 0 | ✓ 信号存在 |
| bfs | telemetry_on | 6665 | 78 | 1 | 0 | ✓ 极端 divergence |

**关键观察**：
- hotspot：7655 次 divergence event，min_active=8（32 线程中 8 个活跃 → 75% divergence）
- bfs：78 次 divergence event，min_active=1（极端 divergence，符合 BFS 不规则访问特性）
- vecadd：0 divergence（预期，无分支）
- 所有 workload sim_cycle 不变 ✓
- paper_daws_throttle_block = 0 ✓

---

## 下一步

Round 03（AUTO-5）：实现 would-throttle telemetry。基于 `active_count` 计算 predicted footprint，判断是否 would throttle，但不实际阻止 warp 发射。
