# Round AI: Focused CCWS Validation

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round AH 完成，接受近似实现（nw=max_warps, miss-side VTA），不再修改机制

---

## 实验设计

| 维度 | 内容 |
|------|------|
| Workloads | 7 个（hotspot, srad_v2, fdtd2d, strided_access, page_stride_access, mutual_tiled, bfs） |
| Configs | feature_off / conservative（inc=1, th=100）/ aggressive（inc=50, th=100） |
| 目标 | 验证机制链路趋势，判断是否足够进入 final report |

**注意**：th=99 全部 deadlock（threshold < lls_base_score=100 导致初始化时所有 warp 被 gate）。conservative 改用 th=100（Round AA/AB 已验证安全范围）。

---

## 结果汇总

### feature_off baseline

| Workload | sim_cycle | lg_block |
|----------|-----------|----------|
| rodinia_hotspot | 6,931 | 0 |
| rodinia_srad_v2 | 15,926 | 0 |
| polybench_fdtd2d | 35,681 | 0 |
| strided_access | 5,825 | 0 |
| page_stride_access | 5,851 | 0 |
| mutual_tiled | 7,479 | 0 |
| rodinia_bfs | 136,110 | 0 |

### conservative（inc=1, th=100）

| Workload | sim_cycle | cycle_delta | vta_hit | lg_block | 评估 |
|----------|-----------|-------------|---------|----------|------|
| rodinia_hotspot | 7,343 | **+6%** | 1,328 | 4,595 | gating active |
| rodinia_srad_v2 | 16,739 | **+5%** | 3,924 | 17,092 | gating active |
| polybench_fdtd2d | 39,539 | **+11%** | 8,345 | 54,907 | gating active |
| strided_access | 5,825 | 0% | 24 | 0 | no gating |
| page_stride_access | 5,851 | 0% | 24 | 0 | no gating |
| mutual_tiled | 7,921 | **+6%** | 384 | 2,014 | gating active |
| rodinia_bfs | 139,060 | **+2%** | 128 | 4,341 | weak gating |

### aggressive（inc=50, th=100）

| Workload | sim_cycle | cycle_delta | vta_hit | lg_block | 评估 |
|----------|-----------|-------------|---------|----------|------|
| rodinia_hotspot | 98,133 | **+1316%** | 1,328 | 3,391,951 | 严重过度 gating |
| rodinia_srad_v2 | 249,050 | **+1464%** | 3,754 | 10,312,132 | 严重过度 gating |
| polybench_fdtd2d | 319,747 | **+796%** | 8,262 | 13,985,078 | 严重过度 gating |
| strided_access | 5,825 | 0% | 24 | 0 | no gating |
| page_stride_access | 5,851 | 0% | 24 | 0 | no gating |
| mutual_tiled | 96,271 | **+1187%** | 384 | 969,673 | 严重过度 gating |
| rodinia_bfs | 666,655 | **+390%** | 135 | 760,007 | 严重过度 gating |

---

## 关键发现

### 1. 机制链路完整，方向正确

- VTA hit → LLS update → load_gate_block 链路在所有 L1D-miss workload 上均触发
- `lls_update = vta_hit`（精确相等），证明 LLS update path 正确
- strided_access / page_stride_access：vta_hit=24（极少），lg_block=0，符合预期（stride 破坏 intra-warp 局部性，VTA hit 率极低）

### 2. conservative 产生适度 gating，但仍有 cycle 增加

conservative（inc=1, th=100）在 stencil workload 上产生明显 gating（lg_block 数千至数万），但 cycle 增加 +2–11%。这与论文预期相反（CCWS 应减少 cycle）。

**根本原因**：cutoff 使用 `nw=max_warps=64`，cutoff=6400。实际 active warps ≈ 8，正确 cutoff 应为 800。当前 cutoff 过大，导致 gate 触发时机不准确——gate 在 LLS 分数已经很高时才触发，此时 warp 已经造成了 cache thrashing，gate 反而增加了 stall 时间而非减少 miss。

### 3. aggressive 严重过度 gating

inc=50 使 LLS 分数快速积累，触发正反馈：gate → 更多 stall → 更多 miss → 更多 VTA hit → 更高 LLS → 更多 gate。cycle 增加 390–1464%，完全不可用。

### 4. 对 gating 最敏感的 workload

| Workload | 敏感度 | 原因 |
|----------|--------|------|
| polybench_fdtd2d | 最高 | 最多 vta_hit（8345），lg_block 最多（54907 conservative） |
| rodinia_srad_v2 | 高 | vta_hit=3924，lg_block=17092 conservative |
| rodinia_hotspot | 中 | vta_hit=1328，lg_block=4595 conservative |
| mutual_tiled | 中 | vta_hit=384，lg_block=2014 conservative |
| rodinia_bfs | 低 | vta_hit=128，lg_block=4341 conservative（+2% cycle） |
| strided_access / page_stride_access | 无 | vta_hit=24，lg_block=0 |

### 5. 近似实现的核心限制

| 限制 | 影响 |
|------|------|
| VTA miss-side 近似（intra-warp 重复 miss） | 可能高估 lost locality；strided workload 的 vta_hit 极低符合预期 |
| cutoff 使用 max_warps=64（应为 active ~8） | cutoff 偏大 8×；gate 触发时机偏晚；conservative 仍有 +5–11% cycle |
| inc=1 时 LLS 积累慢 | gate 触发时 warp 已经 stall 很久，gate 增加 stall 而非减少 miss |
| tiny workload（64×64）| 每 SM 只有 2–8 warps，不代表论文的 HCS workload 场景 |

---

## 结论：是否足够进入 final report

**结论：机制链路复现成功，但近似实现导致 cycle 方向相反，不适合直接进入 standard validation。**

### 支持 final report 的证据

1. VTA → LLS → gate 完整链路在 7/7 workload 上正确触发
2. feature_off 7/7 pass（所有 round 均通过）
3. gating 对 L1D-miss workload 敏感，对 stride/page_stride 不敏感（方向正确）
4. conservative vs aggressive 行为差异明显（可量化）

### final report 中需要说明的限制

1. **VTA 近似**：miss-side 而非 eviction-based，无法检测 inter-warp 驱逐
2. **cutoff 近似**：nw=max_warps 高估 8×，gate 触发时机不准确
3. **tiny workload**：64×64 workload 每 SM 只有 2–8 warps，不代表论文 HCS 场景
4. **cycle 方向**：当前实现在 conservative 下 cycle 增加 +2–11%（论文应减少）

### 建议

**不建议进入 standard validation**（当前实现 cycle 方向相反，standard validation 会产生误导性结果）。

**建议直接进入 final report**，以"机制链路复现成功，近似实现限制明确"为结论。如需进一步改进，需要：
1. 修复 cutoff（需要解决 tiny workload over-gating 问题，可能需要更大 workload）
2. 或实现 faithful eviction-based VTA（需修改 `cache_block_t`）

这两项改进超出当前 focused validation 范围，属于后续 idea branch 工作。
