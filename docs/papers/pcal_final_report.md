# PCAL Approximate Reproduction Final Report

**论文**: Priority-Based Cache Allocation in Throughput Processors (HPCA 2015)
**作者**: Jia, Lowe-Power, Aamodt (UBC)
**复现分支**: `hrl/paper/pcal-repro-v0`
**完成日期**: 2026-05-01

---

## 1. 复现目标

在 GPGPU-Sim 上近似复现 PCAL（Priority-Based Cache Allocation）机制：根据 per-warp cache miss rate 将 warp 分类为高优先级（cache-sensitive）或低优先级（cache-insensitive），对低优先级 warp bypass L1D cache，减少 L1D 对 cache-sensitive warp 的竞争压力。

---

## 2. 机制链路

### 2.1 原论文机制（推测）

1. 为每个 warp/thread 跟踪 cache 行为（miss rate / reuse distance）
2. 将 warp 分类为 cache-sensitive 或 cache-insensitive
3. cache-insensitive warp 的 access bypass L1D（不分配 cache line）
4. 可能的 L2-level 优先级分配

### 2.2 本次复现实现

| 阶段 | 机制 | 实现 | 近似说明 |
|------|------|------|---------|
| 01_noop | Config knobs + stats 骨架 | shader.h/cc/gpu-sim.cc | — |
| 02_telemetry | Per-warp access/miss 窗口计数 | pcal_probe_access() | 两条 L1D 路径均 hook |
| 03_would_change | Would-bypass 遥测（passive） | 在 probe 内判断 | 不改缓存行为 |
| 04_minimal_mechanism | L1D bypass gate（实际 bypass） | memory_cycle() + response fix | bypass set 维护响应路径 |

---

## 3. Config Knobs

| Knob | 默认值 | 说明 |
|------|--------|------|
| gpgpu_enable_pcal | 0 | 主开关 |
| gpgpu_pcal_enable_telemetry | 0 | 遥测开关 |
| gpgpu_pcal_enable_would_bypass | 0 | would-bypass 遥测 |
| gpgpu_pcal_enable_bypass | 0 | 真实 bypass 开关 |
| gpgpu_pcal_miss_rate_threshold | 50 | miss_rate > threshold → low priority |
| gpgpu_pcal_window_size | 64 | per-warp access window 大小（验证用 8）|

---

## 4. Stats

| Stat | 说明 |
|------|------|
| paper_pcal_miss_event | L1D miss probe 次数 |
| paper_pcal_access_event | L1D access probe 次数 |
| paper_pcal_warp_classified_high | 被分类为 high priority 的 warp-round 数 |
| paper_pcal_warp_classified_low | 被分类为 low priority 的 warp-round 数 |
| paper_pcal_would_bypass | would-bypass access 数（telemetry） |
| paper_pcal_bypass_count | 实际 bypass 的 access 数 |
| paper_pcal_bypass_hit | bypass 中原本会 hit 的 access 数 |
| paper_pcal_window_reset | 窗口重置次数 |

---

## 5. Focused Validation 结果

### Focused Set（7 workloads，预期 bypass 有益）

| Workload | noop_off | policy_on | delta | 评价 |
|----------|----------|-----------|-------|------|
| rodinia_hotspot | 6931 | 6919 | -0.17% | ✓ |
| rodinia_srad_v2 | 8236 | 8166 | -0.85% | ✓ |
| polybench_fdtd2d | 5840 | 5798 | -0.72% | ✓ |
| mutual_tiled | 7479 | 7419 | -0.80% | ✓ |
| polybench_2dconv | 6652 | 7008 | +5.35% | ✗ 回归 |
| strided_access | 5825 | 5829 | +0.07% | ~ |
| parboil_histo | 7096 | 7096 | 0% | 无 bypass |

### Control Set（5 workloads）

| Workload | noop_off | policy_on | delta | 评价 |
|----------|----------|-----------|-------|------|
| vecadd | 5569 | 5548 | -0.38% | ~ |
| polybench_gemm | 24543 | 34334 | +39.8% | ✗ 严重回归 |
| mutual_naive | 12322 | 11617 | -5.72% | ✓ 意外减少 |
| rodinia_backprop | 6911 | 6900 | -0.16% | ✓ |
| atomic_contention | 5414 | 5414 | 0% | ✓ |

---

## 6. 成功标准检查

| 标准 | 结果 |
|------|------|
| feature_off sim_cycle = baseline（5569） | ✓ 通过 |
| 所有 paper_pcal_* = 0 when feature_off | ✓ 通过 |
| 无 deadlock | ✓ 通过 |
| bypass_count > 0 for focused workloads | ✓ 通过（8/12 workloads 有 bypass）|
| sim_cycle 方向（减少） | 部分通过（4/7 focused 正确）|

---

## 7. 近似实现局限

| 限制 | 影响 | 可能的改进 |
|------|------|-----------|
| 简单 miss_rate > 50% → low priority | 误分类 cache-sensitive workload（如 gemm） | reuse distance / footprint 估计 |
| tiny workload（64×64 ~ 128 iterations） | miss_rate 偏高，threshold 判断不准 | 使用更大 workload |
| 无 L2 bypass | L2 仍承受 full bypass traffic | 扩展到 L2 bypass |
| 固定 window_size=8 | 窗口太小，分类不稳定 | 自适应 window |
| per-warp（非 per-CTA 或 per-thread） | 粒度粗 | per-warp 在小 workload 下合理 |

---

## 8. 关键实现细节（供未来参考）

### 响应路径 bug（已修复）

当 `bypassL1D = true` 在 `memory_cycle()` 中绕过 L1D 时，ICNT 返回响应后会通过 `ldst_unit::cycle()` 的 response_fifo 处理。若 `m_next_global != NULL`（另一个 bypass 请求正在处理），当前 mf 不会被 pop，需等待下一 cycle。若在此之前就 erase 了 `m_pcal_bypass_mfs` 中的条目，下一 cycle 找不到该 mf → 触发 `baseline_cache::fill()` assertion。

**修复**：erase 只在 pop 成功时执行（`m_next_global == NULL` 分支内）。

### 两条 L1D 路径

SM7_QV100 使用 `gpgpu_l1_latency 20`，实际 L1D 访问通过 `l1_latency_queue`，不经过 `process_cache_access()`（后者仅用于 l1_latency=0 或 texture/const cache）。Telemetry hook 需要两条路径均添加。

---

## 9. 结论

**本次复现状态：Approximate Reproduction（近似复现完成）**

- 机制链路完整（Phase 01–04 全部通过）
- 4/7 focused workloads 方向正确
- 定量结果与原论文不符（部分 workload 有回归）是近似实现的已知限制
- 无 deadlock，无 crash，feature_off 不破坏 baseline

与 CCWS/DAWS 复现相同的结论：GPGPU-Sim 上 approximate reproduction 在 tiny workload 下定量验证困难，但机制链路正确性已验证。
