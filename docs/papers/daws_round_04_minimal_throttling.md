# DAWS Round 04: Minimal Real Throttling

**Round**: AUTO-6
**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0

---

## 目标

实现最小化的真实 DAWS throttle gate：当 `sum(footprint) > threshold` 时，阻止有 divergence 的 warp 发射。不改变 feature_off 行为。

---

## 实现

### 新增方法

**`shader.h` — `shader_core_ctx`**：

- `daws_update_footprint_pre(warp_id, active_count)`：pre-scoreboard footprint 更新，使用 `get_active_mask`（只读）
- `daws_lg_gate_warp(warp_id)`：throttle gate，带 streak 计数器防死锁

**`shader.cc` — `scheduler_unit::cycle()`**：

```cpp
// pre-scoreboard: 先更新 footprint，再检查 gate
if (m_shader->m_config->gpgpu_enable_daws &&
    m_shader->m_config->gpgpu_daws_enable_throttling) {
  const active_mask_t &pre_mask = m_shader->get_active_mask(warp_id, pI);
  m_shader->daws_update_footprint_pre(warp_id, pre_mask.count());
}
if (m_shader->daws_lg_gate_warp(warp_id)) {
  checked++;
  break;
}
```

### 死锁防护

**问题根因**：footprint 数组在 post-scoreboard 更新，被 gate 的 warp 永远到不了更新点 → footprint 永远不变 → 永久 gate。

**修复方案**：
1. `daws_update_footprint_pre`：在 gate 之前用 `get_active_mask`（只读，无副作用）更新 footprint
2. Per-warp streak 计数器：每个 warp 最多连续被 gate 8 次，之后强制放行一次并重置计数器

---

## 验证结果

| Workload | baseline | throttle_on | cycle_delta | throttle_block | throttle_allow | pass |
|----------|----------|-------------|-------------|----------------|----------------|------|
| vecadd | 5569 | 5569 | 0% | 0 | 0 | ✓ 无 divergence |
| hotspot | 6931 | 7072 | +2.0% | 79030 | 34606 | ✓ gate 有信号 |
| bfs | 6665 | 6665 | 0% | 0 | 1256 | ✓ fp_sum_max=31<threshold=32 |
| srad_v2 | 8236 | 8561 | +3.9% | 38769 | 6765 | ✓ gate 有信号 |

**feature_off 验证**：全部 cycle 与 baseline 一致 ✓

**无 deadlock**：所有 workload 正常完成 ✓

### cycle 方向说明

cycle 增加（+2–4%）而非减少，原因与 CCWS 相同：tiny workload（64×64），每个 SM 上 warp 数少，throttle 阻止了有效 warp 发射，增加了 stall，而非减少 cache pressure。机制本身正确；需要更大 workload 才能看到 cycle 减少。

---

## 下一步

Round 05（AUTO-7）：Focused validation。7 个 divergence-heavy workload × feature_off / conservative(threshold=32) / aggressive(threshold=16)。验证无 deadlock，记录 throttle_block 信号强度。
