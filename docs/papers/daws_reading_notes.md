# DAWS Reading Notes

**Round**: AUTO-2
**Date**: 2026-05-01
**Paper source**: Web search summary (no PDF available)

---

## 重要更正：论文信息

**实际论文**：Rogers, O'Connor, Aamodt — "Divergence-Aware Warp Scheduling" — **MICRO 2013**（不是 Fung 等人的 2011 论文）。

daws.yaml 中的 authors/venue 需要更正：
- authors: "Rogers, O'Connor, Aamodt"（与 CCWS 同一组）
- venue: "MICRO 2013"

---

## 1. 研究对象

GPU L1 data cache 在 **irregular / divergent workloads** 下的性能问题。

目标应用：sparse matrix-vector multiply (SpMV)、graph traversal (BFS) 等具有 **memory divergence** 的 workload。

---

## 2. 解决的问题

普通 warp scheduler（包括 CCWS）在 divergent workload 下的局限：

- **CCWS** 关注 intra-warp cache locality（LLS score），但不感知 branch divergence。
- 当 warp 内线程因分支走不同路径时，各线程访问的内存地址不同（memory divergence），导致 cache footprint 膨胀。
- 多个 divergent warp 同时活跃 → L1 cache thrashing → 性能下降。
- 传统 reactive 方案（检测到 interference 后再 throttle）响应太晚。

---

## 3. DAWS 核心思想

**Proactive cache footprint prediction based on divergence.**

核心流程：
1. 检测 warp 内的 branch divergence（active thread count 下降）。
2. 预测每个 warp 的 L1 cache footprint（基于 active thread count + loop 内 load 指令分类）。
3. 在调度时，限制同时活跃的 warp 数量，使总 footprint ≤ L1 cache capacity。
4. 优先调度 "接近 reconverge" 的 warp（active thread count 低 → footprint 小 → 更安全）。

与 CCWS 的关键区别：
- CCWS：LLS score 反映 cache miss 历史 → 事后 gating
- DAWS：footprint prediction 基于 divergence 状态 → 事前 throttling

---

## 4. 需要观测的信号

| 信号 | GPGPU-Sim 对应 | 可用性 |
|------|---------------|--------|
| active thread count per warp | `warp_inst_t::active_count()` / `m_warp_active_mask.count()` | ✓ 已有 |
| branch divergence event | `simt_stack` pdom reconvergence PC | ✓ 已有 |
| loop detection | 需要 PC 范围检测或 back-edge 检测 | ✗ 需新增 |
| load instruction classification | 需要 per-static-PC 分类表 | ✗ 需新增 |
| L1 cache capacity | `gpgpu_cache_config` | ✓ 已有 |

---

## 5. 对 warp scheduling 的影响

- 在 `scheduler_unit::cycle()` 中，根据 footprint prediction 决定是否允许某 warp 发射。
- 类似 CCWS 的 can-issue gating，但判断条件是 predicted footprint sum 而非 LLS score sum。
- 需要新增 per-warp footprint predictor 数据结构（类似 CCWS 的 `m_ccws_lls_score[]`）。

---

## 6. 与 CCWS 的比较

| 维度 | CCWS | DAWS |
|------|------|------|
| 信号来源 | cache miss history (VTA) | branch divergence (active count) |
| 预测方式 | reactive (LLS score decay) | proactive (footprint prediction) |
| 调度影响 | load-only gating | warp throttling (all ops) |
| 目标 workload | cache-sensitive (hotspot, srad) | divergent (SpMV, BFS) |
| 论文年份 | MICRO 2012 | MICRO 2013 |
| 同一作者组 | Rogers/O'Connor/Aamodt | Rogers/O'Connor/Aamodt |

---

## 7. 复现难度评估

| 机制 | 难度 | 说明 |
|------|------|------|
| active thread count probe | 低 | `active_count()` 已有 |
| divergence event detection | 低 | `simt_stack` 已有 pdom 信息 |
| loop detection | 高 | 需要 back-edge / PC range 检测，无现成基础设施 |
| load instruction classification | 高 | 需要 per-static-PC 表，需要 decode 阶段 hook |
| footprint prediction table | 中 | 新增 per-warp 数据结构，类似 LLS score array |
| scheduler throttling | 中 | 类似 CCWS can-issue gating，但条件不同 |

**结论**：DAWS 比 CCWS 复杂，loop detection 和 load classification 是最高风险点。近似方案：用 active_count 直接作为 footprint proxy，跳过 loop detection。

---

## 8. 近似方案

**Approximate DAWS**（推荐复现路径）：

1. **跳过 loop detection**：用 active_count < warp_size 作为 divergence 信号（不区分 loop 内外）。
2. **跳过 load classification**：所有 load 指令均视为 divergence-sensitive。
3. **Footprint proxy**：`predicted_footprint[wid] = (warp_size - active_count[wid]) * cache_line_size`（或直接用 `warp_size / active_count` 作为 throttle factor）。
4. **Throttling**：当 sum(predicted_footprint) > L1_capacity 时，gate 低 active_count warp（与 CCWS 方向相反：CCWS gate 高 LLS score，DAWS gate 高 footprint）。

这个近似保留了 DAWS 的核心思想（divergence → footprint → throttle），但大幅降低实现复杂度。
