# DAWS Round 03: Footprint and Would-Throttle Telemetry

**Round**: AUTO-5
**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0

---

## 目标

基于 Round 02 的 divergence telemetry，添加 approximate footprint estimate 和 would-throttle 信号统计。不改变调度行为。

---

## Footprint 近似公式

```
footprint[wid] = warp_size - active_count(wid)   (diverged thread slots)
```

- 当 `active_count = warp_size`（无 divergence）→ footprint = 0
- 当 `active_count = 1`（极端 divergence）→ footprint = 31
- `sum(footprint)` = 所有 warp 的 diverged thread slots 之和

**Would-throttle 触发条件**：`sum(footprint) > gpgpu_daws_footprint_threshold`（默认 32）

这是对 DAWS 原论文 cache footprint prediction 的近似：跳过 loop detection 和 load classification，直接用 diverged thread count 作为 footprint proxy。

---

## 实现位置

**`src/gpgpu-sim/shader.h`**：
- `shader_core_ctx` 新增方法 `daws_update_footprint_and_would_throttle(warp_id, active_count)`
- `shader_core_ctx` 新增方法 `get_daws_would_throttle_stats(...)`
- `shader_core_ctx` 新增成员变量：
  - `m_daws_footprint[]`（per-warp vector）
  - `m_daws_footprint_update`
  - `m_daws_footprint_sum_total`
  - `m_daws_footprint_sum_max`
  - `m_daws_would_throttle`
- `simt_core_cluster` 新增 `get_daws_would_throttle_stats(...)` 声明

**`src/gpgpu-sim/shader.cc`**：
- `shader_core_ctx` 构造函数：初始化 5 个 Round 03 计数器
- `scheduler_unit::cycle()`：在 Round 02 hook 后追加 Round 03 调用
- `simt_core_cluster::get_daws_would_throttle_stats()`：跨 SM 聚合

**`src/gpgpu-sim/gpu-sim.cc`**：
- 替换 placeholder 为实际 footprint / would_throttle 计数器

---

## 验证结果

| Workload | Config | sim_cycle | divergence_event | fp_sum_max | would_throttle | throttle_block | pass |
|----------|--------|-----------|-----------------|------------|----------------|----------------|------|
| vecadd | noop_off | 5569 | 0 | 0 | 0 | 0 | ✓ |
| hotspot | noop_off | 6931 | 0 | 0 | 0 | 0 | ✓ |
| bfs | noop_off | 6665 | 0 | 0 | 0 | 0 | ✓ |
| vecadd | would_throttle_on | 5569 | 0 | 0 | 0 | 0 | ✓ 无 divergence |
| hotspot | would_throttle_on | 6931 | 7655 | 144 | 6118 | 0 | ✓ 信号存在 |
| bfs | would_throttle_on | 6665 | 78 | 31 | 0 | 0 | ✓ fp_max=31<threshold=32 |
| srad_v2 | would_throttle_on | 8236 | 4229 | 242 | 6083 | 0 | ✓ 信号存在 |

**关键观察**：
- hotspot / srad_v2：would_throttle 有信号（footprint_sum_max 远超 threshold=32）
- bfs：fp_max=31，刚好低于 threshold=32（tiny workload 限制，SM 上 warp 数少）
- vecadd：全程无 divergence，footprint_sum=0
- 所有 workload sim_cycle 不变 ✓，throttle_block=0 ✓

**bfs 无信号解释**：64×64 workload 在 SM 上只有少量 warp，sum(footprint) 最大只有 31。threshold 降至 16 可触发信号，但这属于参数调优，留给 AUTO-6。

---

## 下一步

Round 04（AUTO-6）：实现 minimal real throttling。当 `sum(footprint) > threshold` 时，实际阻止高 footprint warp 发射。验证 hotspot/srad_v2 cycle 是否减少。
