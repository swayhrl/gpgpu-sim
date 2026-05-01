# Round AH: Active-Warp-Only Can-Issue Calculation

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round AG 确认 cutoff 使用 max_warps(64) 而非 active_warps(~8)，放大 8×

---

## 实现内容

### 修改文件

**`src/gpgpu-sim/shader.h`**：在 `shader_core_ctx` 新增两个 accessor：
```cpp
bool warp_done_exit(unsigned wid) const { return m_warp[wid]->done_exit(); }
unsigned get_max_warps() const { return m_config->max_warps_per_shader; }
```

**`src/gpgpu-sim/shader.cc`**：`ldst_unit::cycle()` 中 would_can_issue 计算块：
- `nw` 改为 `get_not_completed() / warp_size`（active thread 数 / warp size）
- sort 时跳过 `done_exit()=true` 的 warp slot
- `cum_cutoff = nw * lg_score_threshold`（nw 现在是 active warp 数）
- exited warp 的 `would_can_issue` 设为 true（不 gate）

---

## 验证结果

### feature_off 7/7 pass

所有 7 workload sim_cycle = baseline，load_gate_block = 0。✓

### gating_on focused set

| workload | inc | lg_block | cycle | cycle_delta | 评估 |
|----------|-----|---------|-------|-------------|------|
| rodinia_hotspot | 5 | 80397 | 12133 | +75% | 过度 gating |
| rodinia_srad_v2 | 5 | 268324 | 29850 | +88% | 过度 gating |
| polybench_fdtd2d | 5 | 458907 | 58371 | +64% | 过度 gating |
| strided_access | 5 | 0 | 5825 | 0 | 无 gating |
| page_stride_access | 5 | 0 | 5851 | 0 | 无 gating |
| rodinia_hotspot | 20 | 841386 | 36833 | +432% | 严重过度 gating |
| rodinia_srad_v2 | 20 | 2549282 | 90050 | +465% | 严重过度 gating |
| polybench_fdtd2d | 20 | 2555614 | 137371 | +285% | 严重过度 gating |

---

## 关键发现：tiny workload 的 cutoff 极小

### 根本原因

`get_not_completed() / warp_size` 在 tiny workload 中极小：

- srad_v2 64×64 = 4096 threads，分布在 80 SMs → per-SM ≈ 51 threads = 2 warps
- `nw = 2`，`cutoff = 2 × 100 = 200`
- 2 个 active warp 的 LLS 合计 ≈ 200-300 > 200 → 几乎所有 warp 被 gate

这导致 `lg_block/attempt ≈ 91%`，几乎所有 load 都被阻塞。

### 与 Round AG 预期的差异

Round AG 预期 cutoff 从 6400 降到 ~800（8 warps/SM）。实际 cutoff 降到 ~200（2 warps/SM），比预期更小。原因：occupancy 12% 是全局平均，但 tiny workload 的 threads 分布不均，部分 SM 只有 1-2 个 warp。

### strided_access / page_stride_access 无 gating

这两个 workload 的 VTA hit 极少（24 次），LLS 分数几乎不超过 base_score，cutoff 再小也不会触发 gate。

---

## 新的问题层次

| 问题 | 状态 |
|------|------|
| cutoff 使用 max_warps（Round AG bug） | ✓ 已修复 |
| tiny workload per-SM thread 数极少 → cutoff 极小 | **新发现** |
| 过度 gating（+64–88% cycle for inc=5） | **当前状态** |

---

## 结论：Revert active-warp 修复，接受近似实现

### 决策

**Revert active-warp cutoff 修复**（`shader.cc` 已 revert，`shader.h` accessor 已移除）。

当前实现恢复为 `nw = m_ccws_lls.size() = max_warps_per_shader = 64`，并在代码中加入注释说明此近似的原因（见 `shader.cc` Round X/AF 注释块）。

### 理由

| 方案 | 结果 | 结论 |
|------|------|------|
| `nw = max_warps(64)` | 0 blocks for inc=5/20/30 | cutoff 过大，gate 不触发 |
| `nw = not_completed/warp_size` | +64–767% cycle | cutoff 过小，过度 gating |

两端都不理想。tiny workload（per-SM 2–8 warps）本身不适合 CCWS 验证——论文的 HCS workload 有更高 occupancy。继续追求精确 active-warp cutoff 在 tiny workload 上没有意义。

### 当前实现的近似说明

| 组件 | 论文语义 | 当前实现 | 近似程度 |
|------|---------|---------|---------|
| VTA | eviction-based（inter-warp 驱逐检测） | miss-side（intra-warp 重复 miss） | 近似，方向正确 |
| NumActiveWarps | 当前 SM 上 active warp 数 | max_warps_per_shader = 64 | 高估 8×，cutoff 偏大 |
| CumLLSCutoff | NumActiveWarps × BaseScore | 64 × threshold | 偏大，gate 阈值偏高 |
| inc=50 | 不适用（论文用 VTA hit rate 动态计算 LLDS） | 静态 increment，触发 gating 但过度 | 可用于趋势验证 |

### 分支验证就绪状态

| 验证类型 | 状态 | 说明 |
|---------|------|------|
| feature_off 7/7 | ✓ 就绪 | 所有 round 均通过 |
| feature_on（inc=50）gating 信号 | ✓ 可用 | srad_v2/fdtd2d 有 gating，但 +45–61% cycle |
| feature_on（inc=1, th=99）弱信号 | ✓ 可用 | hotspot lg_block=5，cycle 不变 |
| standard validation | ⚠ 有限 | tiny workload 不能充分展示 CCWS 效果 |

**结论**：当前分支可以进入 focused validation（7 workload quick set），验证机制趋势正确性。standard validation 需要更大 workload（srad_v2 256×256 等）才有意义。

### 最终 git diff（Round AH 完成后）

```
experiments/paper-ccws/config_matrix.csv  +3 行（Round AH 条目）
experiments/paper-ccws/active_can_issue_validation.csv  新增（22 行）
src/gpgpu-sim/shader.cc  +3 行注释（nw = max_warps 说明）
docs/papers/ccws_round_ah_active_can_issue.md  本文件
```

`source_changed: false`（shader.cc 只加注释，无行为变化；shader.h 已 revert）。
