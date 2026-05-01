# Round AC: LLS Hit-Increment Sensitivity Validation

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round AB 建议增大 `lls_hit_increment` 以增强 gating 信号

---

## 实验设计

7 workloads × 4 configs（inc1/10/50 th100 + inc10 th101）= 35 runs（含 inc1 参考值）。

---

## 结果摘要

| workload | inc1_th100 | inc10_th100 | inc50_th100 | inc10_th101 |
|----------|-----------|------------|------------|------------|
| rodinia_hotspot | lg=5 | lg=0 | lg=0 | lg=0 |
| strided_access | 0 | 0 | 0 | 0 |
| page_stride_access | 0 | 0 | 0 | 0 |
| mutual_tiled | 0 | 0 | 0 | 0 |
| **rodinia_srad_v2** | 0 | 0 | **34557** | 0 |
| **polybench_fdtd2d** | 0 | 0 | **453979** | 0 |
| rodinia_bfs | 0 | 0 | 0 | 0 |

---

## 关键发现

### inc50 触发大量 gating（srad_v2 / fdtd2d）

`inc50_th100` 在两个 stencil workload 上产生显著 gating：
- `rodinia_srad_v2`：lg_block=34557，sim_cycle 从 15926 → 23150（**+45%**）
- `polybench_fdtd2d`：lg_block=453979，sim_cycle 从 35681 → 57411（**+61%**）

这两个 workload 有大量 VTA hits（3924 / 8338），inc=50 使 LLS 分数快速积累，导致高分 warp 被频繁 gate。

### inc10 无 gating（包括 hotspot）

`inc10_th100` 所有 workload lg_block=0，包括 inc1 时有 5 blocks 的 hotspot。原因：inc=10 使分数增长更快，但 hotspot 是极小 kernel（6931 cycles），被 gate 的 warp 在 gate-check 时恰好没有 load 准备好发射（timing 问题）。inc10 的 wg_attempt=16415 但 wg_block=0，说明 `would_can_issue=false` 的 warp 在每次 gate-check 时都没有 load 指令在 issue 窗口。

### sim_cycle 变化

inc50 造成 +45%/+61% cycle 增加，说明 gating 确实影响了调度行为。但增幅过大，可能是过度 gating（论文 CCWS 的目标是减少 cache miss，不是大幅增加 cycle）。

### inc10_th101 无 gating

与 inc10_th100 相同，threshold 差异（6400 vs 6464）在 inc10 下无影响（因为 wg_block 本身就是 0）。

---

## 参数选择建议

| inc | 效果 | 风险 |
|-----|------|------|
| 1 | 信号极弱（只 hotspot 5 blocks） | 安全 |
| 10 | 无 gating（timing 问题） | 安全但无效 |
| 50 | 强信号（srad_v2/fdtd2d 大量 blocks，cycle +45–61%） | 过度 gating |

建议尝试 **inc=5** 或 **inc=20** 作为中间值，在 standard validation 前先做 tiny check。

---

## 是否进入 standard validation

**暂不建议直接进入**。inc=50 的 cycle 增幅过大（+45–61%），说明 gating 过于激进。需要先找到合适的 `lls_hit_increment` 值（预期 inc=5 或 inc=20），使 gating 适度（cycle 变化 <10%）再进入 standard validation。
