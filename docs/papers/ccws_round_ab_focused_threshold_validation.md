# Round AB: Focused Threshold Validation

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round AA 独立 `gpgpu_ccws_lg_score_threshold` knob

---

## 实验设计

7 workloads × 4 configs（off / th99 / th100 / th101）= 28 runs。

---

## 结果

| workload | off | th99 | th100 | th101 | vta_hit | notes |
|----------|-----|------|-------|-------|---------|-------|
| rodinia_hotspot | 0 | **5** | **5** | 0 | 1328 | 唯一有 gating 的 workload |
| strided_access | 0 | 0 | 0 | 0 | 24 | hits 分散 |
| page_stride_access | 0 | 0 | 0 | 0 | 24 | hits 分散 |
| mutual_tiled | 0 | 0 | 0 | 0 | 384 | decay 平衡 hits |
| rodinia_srad_v2 | 0 | 0 | 0 | 0 | 3924 | hits 分散于多 warp |
| polybench_fdtd2d | 0 | 0 | 0 | 0 | 8338 | hits 分散于多 warp |
| rodinia_bfs | 0 | 0 | 0 | 0 | 124 | irregular graph |

所有 28 runs：pass=1，sim_cycle 与 baseline 完全一致。

---

## 分析

### threshold 趋势

th99 = th100（5 blocks）> th101（0 blocks）。趋势单调，方向正确。

分界点在 cutoff=6400（th100）vs 6464（th101）之间。hotspot 的 LLS prefix sum 在某些周期内恰好落在 6400–6464 区间，因此 th100 触发 gate 而 th101 不触发。

### 为何只有 hotspot 有 gating

hotspot 是 100% L1D miss 的 stencil workload，VTA hits 集中在少数 warp（max_score=102，nonzero_warps=34）。其他 workload 即使 vta_hit 更高（srad_v2=3924, fdtd2d=8338），hits 分散在更多 warp，任意时刻没有 warp 的 LLS 分数高到足以推动 prefix sum 超过 cutoff。

### sim_cycle 未变化

5 次 gate block 对 6931 cycle 影响不可见（<0.1%）。需要更大 workload 才能观察 cycle 变化。

---

## 是否进入 standard validation

**建议进入 standard validation（Round AC）**，但需要先解决以下问题：

1. **当前 tiny workload 太小**：每 SM 活跃 warp 数 <1，LLS 分数分化不足。standard set 中更大的 kernel 预期产生更多 gate blocks。
2. **lls_hit_increment 偏小**（当前=1）：VTA hit 只让分数 +1，decay 很快抵消。建议在 standard validation 前尝试 `lls_hit_increment=10` 或 `lls_hit_increment=50`，使分数分化更明显。
3. **threshold 区分度窄**：th99/100 vs th101 的差异只有 64（nw×1），说明当前 LLS 分数分布非常集中在 base_score 附近。

**结论**：当前机制功能正确，但信号太弱。进入 standard validation 前建议先调整 `lls_hit_increment`，否则 standard set 结果可能与 tiny set 相同（只有 hotspot 有少量 blocks）。
