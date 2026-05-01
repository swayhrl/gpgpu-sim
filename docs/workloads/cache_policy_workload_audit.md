# Cache Policy Workload Coverage Audit

**Round**: WORKLOAD-AUDIT
**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0

---

## 1. 审计目标

在进入 `hrl/idea/cache-policy-experiments-v0` 自研 cache policy 之前，系统审计当前 `/workspace/repos/gpgpu-workloads` 中已有的 23 个 ready workload，判断其是否足够支撑第一轮 cache policy 实验。

**不做**：不下载新 benchmark，不跑大规模实验，不改 GPGPU-Sim 源码。

---

## 2. 当前 Workload 全集（23 个 ready）

### 按 sim_cost 分组

| 速度类 | Workload | sim_cycles | IPC |
|--------|----------|------------|-----|
| very fast (<8K) | vecadd | 5,569 | 0.97 |
| | atomic_contention | 5,414 | 0.66 |
| | strided_access | 5,825 | 0.83 |
| | page_stride_access | 5,851 | 0.83 |
| | polybench_2dconv | 6,652 | 123.5 |
| | rodinia_hotspot | 6,931 | 133.4 |
| | mutual_tiled | 7,479 | 25.7 |
| | parboil_stencil | 8,221 | 7.8 |
| medium (8K–40K) | mutual_naive | 12,322 | 17.2 |
| | rodinia_backprop | 13,883 | 37.9 |
| | rodinia_srad_v2 | 15,926 | 69.3 |
| | parboil_mri_q | 21,269 | 25.5 |
| | parboil_sgemm | 21,460 | 4.4 |
| | polybench_gemm | 24,543 | 80.3 |
| | rodinia_kmeans | 31,592 | 2.0 |
| | parboil_histo | 35,472 | 170.1 |
| | polybench_fdtd2d | 35,681 | 24.6 |
| slow (40K–200K) | polybench_atax | 77,442 | 19.4 |
| | rodinia_lud | 97,518 | 1.1 |
| | rodinia_nw | 97,915 | 1.1 |
| | rodinia_bfs | 136,110 | 0.23 |
| | rodinia_pathfinder | 207,186 | 3.7 |
| very slow (>200K) | parboil_spmv | 1,090,650 | 0.97 |

---

## 3. Workload 特征标注

| Workload | cache_sensitive | divergence | irregular | compute | L1D_miss | L2_miss | L1D_rfail |
|----------|----------------|------------|-----------|---------|----------|---------|-----------|
| vecadd | high | none | no | low | 1.000 | 0.333 | 79 |
| strided_access | medium | none | no | low | 0.222 | 0.500 | 6 |
| page_stride_access | medium | none | no | low | 0.333 | 0.333 | 0 |
| atomic_contention | low | none | no | low | NA | 0.000 | NA |
| mutual_naive | low | none | no | high | 0.152 | 0.200 | 4 |
| mutual_tiled | high | none | no | high | 1.000 | 0.200 | 12 |
| polybench_2dconv | medium | none | no | high | 0.243 | 0.367 | 284 |
| polybench_fdtd2d | medium | none | no | medium | 0.546 | 0.000 | 4,098 |
| polybench_gemm | low | none | no | high | 0.075 | 0.000 | 6,253 |
| polybench_atax | low | none | no | medium | 0.014 | 0.000 | — |
| rodinia_hotspot | high | medium | no | high | 1.000 | 0.199 | 519 |
| rodinia_srad_v2 | high | medium | no | high | 0.792 | 0.342 | 2,258 |
| rodinia_backprop | medium | none | no | high | 0.408 | 0.008 | 1,438 |
| rodinia_bfs | low | high | yes | low | 0.308 | 0.000 | — |
| rodinia_pathfinder | medium | medium | no | low | 0.800 | 0.007 | — |
| rodinia_kmeans | medium | medium | yes | low | 0.667 | 0.125 | — |
| rodinia_lud | medium | none | no | low | 0.516 | 0.000 | — |
| rodinia_nw | medium | none | no | low | 0.866 | 0.000 | — |
| parboil_histo | medium | none | no | high | 0.217 | 0.212 | 42,603 |
| parboil_spmv | low | none | yes | low | NA | 0.000 | — |
| parboil_stencil | high | none | no | medium | 1.000 | 0.321 | — |
| parboil_sgemm | medium | none | no | medium | 0.680 | 0.320 | — |
| parboil_mri_q | high | none | no | medium | 1.000 | 0.443 | — |

*divergence 标注基于 DAWS AUTO-4/5 实验数据（hotspot=7655 events，srad_v2=4229，bfs=78，pathfinder=68）*

---

## 4. Cache Policy 实验分组

### 4.1 cache_policy_focused（7 个，主实验集）

| Workload | 角色 | 核心信号 | sim_cost |
|----------|------|---------|---------|
| rodinia_hotspot | primary_signal | L1D=1.0，divergence=7655 | tiny |
| rodinia_srad_v2 | primary_signal | L1D=0.79 L2=0.34，divergence=4229 | medium |
| polybench_fdtd2d | l1d_l2_contrast | L1D=0.55 L2=0.00（L1D 压力无 L2 溢出） | medium |
| mutual_tiled | reuse_signal | L1D=1.0，tile reuse pattern | tiny |
| polybench_2dconv | stencil_l2 | L1D=0.24 L2=0.37，L2 压力 | tiny |
| strided_access | coalescing_control | L1D=0.22 L2=0.50，受控 coalescing 退化 | tiny |
| parboil_histo | reservation_fail | L1D_rfail=42,603，最高 reservation fail | medium |

**选择理由**：覆盖 L1D 高 miss（hotspot/srad_v2/mutual_tiled）、L2 压力（strided/2dconv）、L1D-without-L2（fdtd2d）、reservation fail（histo）四种不同 cache 行为模式。

### 4.2 cache_policy_controls（5 个，负控制集）

| Workload | 角色 | 为什么是 control |
|----------|------|----------------|
| vecadd | negative_control | 无 divergence，最简单；任何 policy 改变这里都是 red flag |
| polybench_gemm | compute_bound_control | L2=0.00，完全 compute-bound；cache policy 不应影响 |
| mutual_naive | streaming_reference | global-mem streaming；与 mutual_tiled 对比 |
| rodinia_backprop | dense_nn_control | L2=0.008，低 L2 miss；应不受 cache policy 影响 |
| atomic_contention | atomic_control | L1D 绕过；atomic path；explicit PASS |

### 4.3 cache_policy_standard_candidate（13 个，完整验证候选）

focused(7) + controls(5) + 额外 1 个：
- **rodinia_bfs**：irregular graph，divergence signal，但 slow（136K cycles）

parboil_spmv 列为候选但建议仅在最终验证时使用（1M cycles）。

---

## 5. 主要缺口分析

### 缺口 1：无大规模 stencil（高优先级）

**现状**：所有 stencil 都是 64×64 或 128×128。
**影响**：CCWS/DAWS 复现中 cycle 方向相反的根本原因——tiny workload 下每 SM warp 数少，throttle 阻止有效 warp 而非减少 cache pressure。
**修复**：重新编译 hotspot/srad_v2 使用 256×256 输入。**低工作量，高价值。**

```bash
# hotspot 256x256
./hotspot 256 1 1 temp_256 power_256 output.out
# srad_v2 256x256
./srad 256 256 0 255 0 255 0.5 1
```

### 缺口 2：无大规模 BFS（高优先级）

**现状**：rodinia_bfs 使用 64-node graph，divergence_event=78，fp_sum_max=31（刚好低于 threshold=32）。
**影响**：BFS 是 DAWS 论文的核心 workload，但当前 tiny graph 信号太弱。
**修复**：重新编译 bfs 使用 1024-node graph。**低工作量，高价值。**

### 缺口 3：无 HCS-like workload（低优先级）

**现状**：无 LBM、LULESH、Graph500 等真实 HCS workload。
**影响**：无法复现原论文的 HCS 场景。
**修复**：需要添加新 benchmark suite。**高工作量，暂不需要。**

### 缺口 4：部分 workload 缺 cacheinst 字段（中优先级）

**现状**：8 个 workload（bfs、pathfinder、kmeans、lud、nw、spmv、sgemm、mri_q）的 `cacheinst_L1D_miss_rate=NA`，因为这些 workload 在 Round P 之前跑的，或者使用了不同的 config。
**影响**：无法用 cacheinst instrumentation 分析这些 workload。
**修复**：用当前 build 重跑一次。**中等工作量，按需执行。**

---

## 6. 是否足够开始 Cache Policy 自研

**结论：足够。**

7 个 focused workload 覆盖了 cache policy 实验所需的核心行为模式：
- ✓ 高 L1D miss（hotspot、srad_v2、mutual_tiled）
- ✓ L2 压力（strided_access、2dconv）
- ✓ L1D-without-L2（fdtd2d）
- ✓ Reservation fail（histo）
- ✓ 5 个 negative control

**主要限制**：tiny workload scale。建议在第一轮实验后，如果 cycle 方向相反，立即补 256×256 stencil 输入，而不是等到发现问题再补。

---

## 7. 建议

1. **立即开始**：用现有 7 focused + 5 control workload 开始第一轮 cache policy 实验。
2. **可选预处理**：在开始前，重新编译 hotspot/srad_v2 使用 256×256 输入（预计 30 分钟）。
3. **不要先补 benchmark**：现有覆盖已足够，补 benchmark 是第二轮的事。
4. **BFS 大图**：如果第一轮实验需要 irregular workload，再补 1024-node BFS。
5. **parboil_spmv**：仅在最终验证时使用，不放入 focused 集。
