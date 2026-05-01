# Round Y: CCWS Minimal Real Load-Only Gating

**日期**: 2026-05-01  
**分支**: `hrl/paper/ccws-repro-v0`  
**基础**: Round X would-gate telemetry（instrumentation-only）

---

## 本轮目标

将 Round X 的 would-gate telemetry 升级为真实 load-only gating。  
**最小实现原则**：只 gate LOAD_OP / TENSOR_CORE_LOAD_OP，不改变 cache replacement、VTA/LLS 更新逻辑、scheduler 结构。

---

## 实现方案

### 核心逻辑

在 `scheduler_unit::cycle()` 的 LOAD_OP issue 路径上，新增一个 gate 判断：

```cpp
// CCWS Round Y: real load-only gating
bool ccws_load_blocked = false;
if (m_shader->m_config->gpgpu_enable_ccws &&
    m_shader->m_config->gpgpu_ccws_enable_load_gating &&
    ((pI->op == LOAD_OP) || (pI->op == TENSOR_CORE_LOAD_OP))) {
  ccws_load_blocked = m_shader->m_ldst_unit->ccws_lg_gate_load(warp_id);
}
if (!ccws_load_blocked && m_mem_out->has_free(...) && ...) {
  m_shader->issue_warp(...);
}
```

`ccws_lg_gate_load(wid)` 查询 `m_ccws_would_can_issue[wid]`（由 Round X 的 sort+prefix-sum 每周期更新），若 `would_can_issue[wid] == false` 则返回 `true`（阻塞）。

### 关键约束

| 约束 | 状态 |
|------|------|
| 只 gate LOAD_OP / TENSOR_CORE_LOAD_OP | ✓ |
| STORE / MEMORY_BARRIER / compute 不受影响 | ✓ |
| `gpgpu_ccws_enable_load_gating=0` 时完全无行为变化 | ✓ |
| 不改 VTA / LLS 更新逻辑 | ✓ |
| 不改 cache replacement | ✓ |
| 不重构 scheduler | ✓ |

---

## 新增 Config Knobs

| Knob | Default | 说明 |
|------|---------|------|
| `-gpgpu_ccws_enable_load_gating` | `0` | 启用真实 load gating（依赖 would_gate 计算） |
| `-gpgpu_ccws_load_gate_debug` | `0` | per-gate debug trace（预留，暂未使用） |

**依赖关系**：`enable_load_gating=1` 需要同时开启 `enable_ccws=1`、`enable_would_gate=1`、`enable_lls_score=1`、`enable_vta_probe=1`，否则 `m_ccws_would_can_issue` 为空，`ccws_lg_gate_load()` 直接返回 false（不阻塞）。

---

## 新增 Stats

| Stat | 说明 |
|------|------|
| `paper_ccws_load_gate_attempt` | 真实 gate 检查次数（load issue 候选数） |
| `paper_ccws_load_gate_block` | 被真实 gate 阻塞的 load 数 |
| `paper_ccws_load_gate_allow` | 通过 gate 的 load 数 |

Round X 的 `paper_ccws_would_gate_*` 保留不变（telemetry 层）。

---

## 修改文件

| 文件 | 修改内容 |
|------|---------|
| `src/gpgpu-sim/shader.h` | `ldst_unit`：3 个 lg 计数器 + `ccws_lg_gate_load()` + `get_ccws_lg_stats()`；`shader_core_config`：2 个新 knob；`shader_core_ctx` / `simt_core_cluster`：声明 |
| `src/gpgpu-sim/shader.cc` | `init()`：lg 计数器初始化；`scheduler_unit::cycle()`：gate 判断插入；`ccws_lg_gate_load()` / `get_ccws_lg_stats()` 实现；`shader_core_ctx` / `simt_core_cluster` 聚合方法 |
| `src/gpgpu-sim/gpu-sim.cc` | 注册 2 个新 knob；`print_stats()`：`paper_ccws_load_gate_*` 从硬编码 0 改为真实聚合值 |
| `configs/hrl-repro/SM7_QV100_ccws_load_gate_on/` | 新建（基于 would_gate_on + `enable_load_gating=1`） |

---

## Quick Set 验证结果（7 workloads）

### feature_off（SM7_QV100_ccws_noop_off）

| Workload | sim_cycle | lg_attempt | lg_block | wg_block | 结论 |
|----------|-----------|------------|----------|----------|------|
| vecadd | 5569 | 0 | 0 | 0 | ✓ baseline |
| strided_access | 5825 | 0 | 0 | 0 | ✓ baseline |
| page_stride_access | 5851 | 0 | 0 | 0 | ✓ baseline |
| atomic_contention | 5414 | 0 | 0 | 0 | ✓ baseline |
| mutual_tiled | 7479 | 0 | 0 | 0 | ✓ baseline |
| polybench_2dconv | 6652 | 0 | 0 | 0 | ✓ baseline |
| rodinia_hotspot | 6931 | 0 | 0 | 0 | ✓ baseline |

**结论**：feature_off 7/7 pass，所有计数器 = 0，cycle 与 baseline 完全一致。

### load_gate_on（SM7_QV100_ccws_load_gate_on）

| Workload | sim_cycle | lg_attempt | lg_block | wg_block | 结论 |
|----------|-----------|------------|----------|----------|------|
| vecadd | 5569 | 105 | 0 | 0 | ✓ pass；LLS 信号不足 |
| strided_access | 5825 | 97 | 0 | 0 | ✓ pass；stride 破坏局部性，LLS 低 |
| page_stride_access | 5851 | 97 | 0 | 0 | ✓ pass；同上 |
| atomic_contention | 5414 | 50 | 0 | 0 | ✓ pass；atomic 不走 L1D |
| mutual_tiled | 7479 | 2552 | 0 | 0 | ✓ pass；LLS 信号存在但 decay 平衡 |
| polybench_2dconv | 6652 | 10232 | 0 | 0 | ✓ pass；LLS 信号存在但 decay 平衡 |
| **rodinia_hotspot** | **6931** | **16420** | **5** | **5** | ✓ **GATING ACTIVE** |

**关键结论**：
- `rodinia_hotspot`：`lg_block=5`，真实 gating 生效 ✓
- `lg_block = wg_block`：gate 判断与 telemetry 完全一致 ✓
- quick workload 太小（<1 warp/scheduler），大多数 workload LLS 分数不足以持续触发 gate；standard set 预期会有更多 block
- `sim_cycle` 在 `rodinia_hotspot` 未变（5 次 gate 对 6931 cycle 影响极小）

---

## 下一步

- **standard set 验证**：更大 workload 预期 `lg_block` 显著增加
- **LLS 参数调优**：`lls_hit_increment` / `lls_decay_amount` 比值影响 gate 频率
- **sim_cycle 对比**：standard set 中 HCS workload 的 cycle 变化是 CCWS 效果的核心指标
