# Round AD: LLS Hit-Increment Calibration (inc=5/20/30)

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round AC 发现 inc=10 无 gating（timing 问题），inc=50 过度 gating（+45–61% cycle）

---

## 实验设计

7 workloads × 3 configs（inc=5/20/30，th=100）= 21 runs。目标：找到 inc=10 和 inc=50 之间的中间值，使 gating 适度（cycle 变化 <10%）。

---

## 结果摘要

| workload | inc5_th100 | inc20_th100 | inc30_th100 |
|----------|-----------|------------|------------|
| rodinia_hotspot | lg=0 | lg=0 | lg=0 |
| strided_access | 0 | 0 | 0 |
| page_stride_access | 0 | 0 | 0 |
| mutual_tiled | 0 | 0 | 0 |
| rodinia_srad_v2 | 0 | 0 | 0 |
| polybench_fdtd2d | 0 | 0 | 0 |
| rodinia_bfs | 0 | 0 | 0 |

**所有 21 runs：lg_block=0，sim_cycle = baseline。**

---

## 关键发现：gating 的根本限制

### 诊断数据

| workload/config | max_score | wg_attempt | wg_block | lg_block |
|----------------|-----------|------------|----------|---------|
| hotspot inc5 | — | — | 0 | 0 |
| hotspot inc20 | 204 | 16415 | 0 | 0 |
| srad_v2 inc20 | 413 | — | 0 | 0 |
| fdtd2d inc30 | 863 | 16580 | 0 | 0 |
| fdtd2d inc50 | — | 470559 | 453979 | 453979 |

### 根本原因

**Gate 只在 warp 有 LOAD_OP 准备发射时触发。高 LLS 分数的 warp 已经被 memory system（scoreboard stall）阻塞，无法到达 issue 阶段。**

具体机制：
1. 高 VTA hit 率的 warp（如 srad_v2 的 stencil warp）频繁 L1D miss
2. Miss 后 warp 进入 scoreboard stall，等待 memory 返回
3. 在 stall 期间，该 warp 的 LLS 分数通过 VTA hit 积累
4. 但 gate 检查发生在 `scheduler_unit::cycle()` 的 issue 阶段
5. 已经 stall 的 warp 不会到达 issue 阶段，gate 永远不会触发

### inc=50 为何有效

inc=50 通过正反馈循环绕过了这个限制：
- 分数积累极快，使 **非 stall 的 warp** 也超过 cutoff
- wg_attempt 从 16580（inc=30）跳到 470559（inc=50）——增加 28×
- 这说明 inc=50 开始 gate 那些本来可以发射的 warp，形成反馈：gate → 更多 miss → 更高分数 → 更多 gate
- 结果：+45–61% cycle（过度 gating）

### inc=5/20/30 为何无效

这三个值使分数积累足够快，使高 miss 率 warp 的 LLS 超过 cutoff，但这些 warp 已经在 stall，gate 无法触发。wg_attempt 保持在 ~16000–17000（与 inc=1 相同），说明 gate 检查的 warp 集合没有变化。

---

## 参数选择总结

| inc | lg_block | cycle 变化 | 评估 |
|-----|---------|-----------|------|
| 1 | hotspot=5 | 0% | 信号极弱 |
| 5 | 0 | 0% | 无效（同 inc=10） |
| 10 | 0 | 0% | 无效（timing 问题） |
| 20 | 0 | 0% | 无效（同根因） |
| 30 | 0 | 0% | 无效（同根因） |
| 50 | srad_v2=34557, fdtd2d=453979 | +45–61% | 过度 gating |

---

## 下一步建议

当前 gating 实现有根本性限制：gate 插入点在 issue 阶段，而高 LLS warp 已在 scoreboard stall，无法到达 issue 阶段。

可选方向：

**方向 A（接受 inc=50，调整 threshold）**：提高 `lg_score_threshold`（如 th=150/200）以减少 inc=50 的过度 gating。风险：threshold 调整可能无法精确控制 cycle 增幅。

**方向 B（重新考虑 gating 插入点）**：在更早的 pipeline 阶段（如 warp 选择阶段，而非 issue 阶段）插入 gate，使高 LLS warp 在 stall 恢复后也被 gate。这更接近论文原意（"Can Issue bit"影响 warp 是否进入 issue 候选集）。

**方向 C（接受当前行为，进入 standard validation）**：inc=1/th=99 在 hotspot 上有 5 blocks，虽然信号弱，但机制 confirmed working。可以用 inc=50 的 srad_v2/fdtd2d 结果作为"gating active"证明，进入 standard set 验证更多 workload。
