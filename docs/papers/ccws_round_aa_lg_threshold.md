# Round AA: Independent Load-Gating Score Threshold

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round Z 发现 `base_score` 不是独立 threshold

---

## 问题

Round Z 的 threshold sweep 用 `lls_base_score` 控制 cutoff：

```
cum_cutoff = nw * lls_base_score
```

但 `lls_base_score` 同时控制 LLS 初始值，两者等比例缩放，行为不变。

## 修复

新增独立 knob `gpgpu_ccws_lg_score_threshold`（默认 100），只用于 cutoff 计算：

```cpp
// shader.cc — Round X sort+prefix-sum
cum_cutoff = (unsigned long long)nw * m_config->gpgpu_ccws_lg_score_threshold;
```

`lls_base_score` 保持只控制 LLS 初始值和 decay floor，不再影响 gating cutoff。

## 修改文件

| 文件 | 修改 |
|------|------|
| `src/gpgpu-sim/shader.h` | 新增 `gpgpu_ccws_lg_score_threshold` 成员 |
| `src/gpgpu-sim/shader.cc` | `cum_cutoff` 改用 `lg_score_threshold` |
| `src/gpgpu-sim/gpu-sim.cc` | 注册新 knob，默认 100 |

## Threshold 语义

`cum_cutoff = nw * lg_score_threshold`（nw=64 for SM7_QV100）

| threshold | cutoff | 含义 |
|-----------|--------|------|
| 99 | 6336 | 略低于 base；有 VTA hit 的 warp 更易被 gate |
| 100 | 6400 | 默认；等价 Round Y 行为 |
| 101 | 6464 | 略高；prefix 不超 cutoff，0 blocks |
| 200 | 12800 | 高；需要 max_score>200 才触发，0 blocks |

## Tiny Validation 结果（3 workloads × 4 thresholds）

| workload | th | pass | sim_cycle | vta_hit | wg_block | lg_block |
|----------|----|------|-----------|---------|----------|----------|
| rodinia_hotspot | off | ✓ | 6931 | 0 | 0 | 0 |
| rodinia_hotspot | 99 | ✓ | 6931 | 1328 | 5 | 5 |
| rodinia_hotspot | 100 | ✓ | 6931 | 1328 | 5 | 5 |
| rodinia_hotspot | 101 | ✓ | 6931 | 1328 | 0 | 0 |
| rodinia_hotspot | 200 | ✓ | 6931 | 1328 | 0 | 0 |
| strided_access | all | ✓ | 5825 | 24 | 0 | 0 |
| page_stride_access | all | ✓ | 5851 | 24 | 0 | 0 |

**threshold 确实影响 load_gate_block**：th99/100 → 5 blocks；th101/200 → 0 blocks。✓

**注意**：th50（cutoff=3200）导致 deadlock（所有 warp 被 gate，调度器无法推进）。已删除该 config，说明 threshold 不能低于 `lls_base_score`（否则初始状态下所有 warp 都被 gate）。

## 关键发现

- `lg_score_threshold` 与 `lls_base_score` 解耦后，threshold sweep 有效。
- 有效范围：`lg_score_threshold >= lls_base_score`（否则 deadlock）。
- 当前 hotspot 的 max_score=102，threshold 在 100–101 之间有明显分界。
- 要在更多 workload 上看到 gating，需要更大 workload 或增大 `lls_hit_increment`。
