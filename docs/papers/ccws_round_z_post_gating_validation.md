# Round Z: Post-Gating Validation

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round Y minimal real load-only gating

---

## 目标

验证 Round Y gating 在 cache/irregular workload 上的信号合理性。不新增机制。

---

## 实验设计

**Workloads（7 个）**：strided_access, page_stride_access, mutual_tiled, rodinia_hotspot, rodinia_srad_v2, polybench_fdtd2d, rodinia_bfs

**Configs（4 组）**：
| Config | threshold | base_score |
|--------|-----------|------------|
| SM7_QV100_ccws_noop_off | off | — |
| SM7_QV100_ccws_load_gate_on | default | 100 |
| SM7_QV100_ccws_load_gate_conservative | conservative | 200 |
| SM7_QV100_ccws_load_gate_aggressive | aggressive | 50 |

---

## 结果摘要

### feature_off（7/7 pass）

所有 workload：cycle = baseline，所有计数器 = 0。✓

### load_gate_on — default threshold（7/7 pass）

| Workload | sim_cycle | vta_hit | lls_update | wg_block | lg_block |
|----------|-----------|---------|------------|----------|----------|
| strided_access | 5825 | 24 | 24 | 0 | 0 |
| page_stride_access | 5851 | 24 | 24 | 0 | 0 |
| mutual_tiled | 7479 | 384 | 384 | 0 | 0 |
| **rodinia_hotspot** | **6931** | **1328** | **1328** | **5** | **5** |
| rodinia_srad_v2 | 15926 | 3924 | 3924 | 0 | 0 |
| polybench_fdtd2d | 35681 | 8338 | 8338 | 0 | 0 |
| rodinia_bfs | 136110 | 124 | 124 | 0 | 0 |

### threshold sweep（conservative / aggressive）

**三组结果完全相同**。原因见下方分析。

---

## 关键发现

### 1. 只有 rodinia_hotspot 出现 gating（lg_block=5）

`rodinia_hotspot` 是 stencil workload，L1D miss 率 100%，VTA hit 集中在少数 warp。这些 warp 的 LLS 分数在某些周期内显著高于其他 warp，触发 sort+prefix-sum cutoff。

其他 workload 即使 vta_hit 更高（srad_v2=3924, fdtd2d=8338），也没有 gating，原因是 hits 分散在更多 warp 和更多周期，任意时刻没有单个 warp 的 LLS 分数高到足以推动 prefix sum 超过 cutoff。

### 2. sim_cycle 未变化

5 次 gate block 对 6931 cycle 的影响极小（<0.1%），在统计误差范围内。需要更大 workload（更多 kernel、更多 warp）才能观察到 cycle 变化。

### 3. threshold sweep 无效（base_score 不是独立 threshold）

`base_score` 同时控制：
- LLS 初始值（`m_ccws_lls.assign(nw, base_score)`）
- cutoff（`cum_cutoff = nw * base_score`）

两者等比例缩放，相对行为不变。要做有效 threshold sweep，需要引入独立的 `lls_gate_threshold` 参数（与初始值解耦）。这是 Round Z 的重要发现，留给后续 round 处理。

### 4. 信号合理性判断

| 检查项 | 结果 |
|--------|------|
| feature_off 全部 lg_block=0 | ✓ |
| load_gate_on 至少一个 workload lg_block>0 | ✓（rodinia_hotspot） |
| lg_block = wg_block（gate 与 telemetry 一致） | ✓ |
| STORE / compute 不受影响 | ✓ |
| 高 vta_hit workload 不一定有 gating（分散 vs 集中） | ✓ 符合预期 |

---

## 后续建议

1. **解耦 threshold**：新增 `gpgpu_ccws_lls_gate_threshold`（独立于 base_score），使 threshold sweep 有意义。
2. **更大 workload**：standard set 中的 larger kernel 预期产生更多 lg_block 和可观测的 cycle 变化。
3. **LLS 参数调优**：增大 `lls_hit_increment`（如 10→50）或减小 `lls_decay_amount` 可加速 LLS 分数分化，使更多 workload 出现 gating。
