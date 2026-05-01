# DAWS Round 05: Focused Validation

**Round**: AUTO-7
**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0

---

## 目标

验证 approximate DAWS throttling 在 divergence/cache-sensitive workloads 上的信号质量，判断是否可进入 final report。不改源码，不新增机制。

---

## 实验设计

**Workloads（7 个）**：vecadd（control）、rodinia_hotspot、rodinia_srad_v2、rodinia_bfs、rodinia_pathfinder、polybench_2dconv、polybench_fdtd2d

**Configs（3 类）**：
- `SM7_QV100_daws_noop_off`：feature_off baseline
- `SM7_QV100_daws_would_throttle_on`：telemetry only，不改调度
- `SM7_QV100_daws_throttle_on`：真实 throttle，threshold=32，max_streak=8

---

## 验证结果

### feature_off（noop_off）

| Workload | sim_cycle | pass |
|----------|-----------|------|
| vecadd | 5569 | ✓ |
| rodinia_hotspot | 6931 | ✓ |
| rodinia_srad_v2 | 8236 | ✓ |
| rodinia_bfs | 6665 | ✓ |
| rodinia_pathfinder | 6487 | ✓ |
| polybench_2dconv | 6652 | ✓ |
| polybench_fdtd2d | 5840 | ✓ |

全部与 baseline 一致 ✓

### would_throttle_on（telemetry only）

| Workload | sim_cycle | div_event | fp_sum_max | would_throttle | pass |
|----------|-----------|-----------|------------|----------------|------|
| vecadd | 5569 | 0 | 0 | 0 | ✓ 无 divergence |
| rodinia_hotspot | 6931 | 7655 | 144 | 6118 | ✓ 强信号，cycle 不变 |
| rodinia_srad_v2 | 8236 | 4229 | 242 | 6083 | ✓ 强信号，cycle 不变 |
| rodinia_bfs | 6665 | 78 | 31 | 0 | ✓ fp_max=31<threshold=32 |
| rodinia_pathfinder | 6487 | 68 | 32 | 0 | ✓ fp_max=32=threshold（不超过） |
| polybench_2dconv | 6652 | 7308 | 8 | 0 | ✓ 有 divergence 但 fp_max 低 |
| polybench_fdtd2d | 5840 | 0 | 0 | 0 | ✓ 无 divergence |

全部 cycle 不变 ✓，无 deadlock ✓

### throttle_on（真实 throttle）

| Workload | baseline | throttle_on | cycle_delta | throttle_block | throttle_allow | pass |
|----------|----------|-------------|-------------|----------------|----------------|------|
| vecadd | 5569 | 5569 | 0% | 0 | 0 | ✓ gate 不触发 |
| rodinia_hotspot | 6931 | 7072 | +2.0% | 79030 | 34606 | ✓ gate 有信号 |
| rodinia_srad_v2 | 8236 | 8561 | +3.9% | 38769 | 6765 | ✓ gate 有信号 |
| rodinia_bfs | 6665 | 6665 | 0% | 0 | 1256 | ✓ fp_sum 不超 threshold |
| rodinia_pathfinder | 6487 | 6487 | 0% | 0 | 1646 | ✓ fp_sum 不超 threshold |
| polybench_2dconv | 6652 | 6652 | 0% | 0 | 169443 | ✓ fp_max=8 低，不超 threshold |
| polybench_fdtd2d | 5840 | 5840 | 0% | 0 | 0 | ✓ 无 divergence |

全部无 deadlock ✓

---

## 关键观察

### 信号分层

| 类别 | Workload | 特征 |
|------|----------|------|
| 强 divergence | hotspot、srad_v2 | fp_sum_max >> threshold，would_throttle 高，throttle_block 高 |
| 弱 divergence | bfs、pathfinder | fp_sum_max ≈ threshold，gate 不触发 |
| 低 divergence | 2dconv | 有 divergence_event 但 fp_max 低（8），gate 不触发 |
| 无 divergence | vecadd、fdtd2d | 全程 fp=0，gate 不触发 |

### cycle 方向

hotspot/srad_v2 cycle 增加（+2–4%），与 CCWS 结论相同：tiny workload（64×64），每 SM warp 数少，throttle 阻止有效 warp 发射，增加 stall，而非减少 cache pressure。机制本身正确，需要更大 workload 才能看到 cycle 减少。

### polybench_2dconv 异常

throttle_allow=169443 但 throttle_block=0，说明 gate 被频繁评估但 fp_sum 始终不超 threshold（fp_max=8，远低于 threshold=32）。这是正常行为。

---

## 结论

1. **feature_off 不变** ✓
2. **would_throttle_on cycle 不变** ✓
3. **throttle_on 无 deadlock** ✓
4. **throttle_block 信号存在**（hotspot=79030，srad_v2=38769）✓
5. **cycle 方向可解释**（tiny workload 限制）✓
6. **approximate DAWS 机制链路完整**：divergence → footprint proxy → would-throttle → real gate ✓

**建议进入 AUTO-8 final report**。当前近似实现已足够记录为 approximate reproduction。

---

## 下一步

AUTO-8：DAWS final reproduction report。记录机制链路、近似说明、验证结果、与原论文的差距分析。
