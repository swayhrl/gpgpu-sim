# PCAL Phase 3：Would-Change Bypass Telemetry

**日期**：2026-05-01  
**分支**：hrl/paper/pcal-repro-v0  
**状态**：完成 ✓

---

## 目标

基于 Phase 2 telemetry signal，在 `pcal_probe_access()` 中添加 would_bypass 决策记录。不改变 cache 行为。

## 修改文件

| 文件 | 改动 |
|------|------|
| `src/gpgpu-sim/shader.h` | `pcal_probe_access()` 中添加 would_bypass 判断（3 行） |
| `configs/hrl-repro/SM7_QV100_pcal_would_change_on/` | 新建：telemetry=1, would_bypass=1 |

## 实现机制

在 `pcal_probe_access()` 中，每次 L1D 访问后：
- 如果 `gpgpu_pcal_enable_would_bypass=1` 且 `m_pcal_warp_low_priority[warp_id]=true`
- 递增 `m_pcal_would_bypass`

低 priority warp 在第一个 window 分类后，后续每次访问都计入 would_bypass。

## 验证结果

| Workload | sim_cycle | access_event | would_bypass | bypass_rate | bypass_count |
|----------|-----------|-------------|-------------|-------------|--------------|
| vecadd | 5569 | 96 | 32 | 33% | 0 |
| rodinia_hotspot | 6931 | 2576 | 1106 | 43% | 0 |
| rodinia_srad_v2 K1 | 8236 | 4352 | 3328 | 76% | 0 |
| polybench_fdtd2d | 35681 | 14450 | 9352 | 65% | 0 |
| mutual_tiled | 7479 | 640 | 384 | 60% | 0 |

**信号解释**：
- would_bypass > 0 for all workloads ✓（低 priority 分类后持续触发）
- would_bypass < access_event（初始 window 未满时 warp 为高 priority）
- bypass_count = 0（Phase 3 无真实 bypass）✓
- sim_cycle 不变 ✓

## 成功标准检查

- [x] would_bypass 有合理信号（非零，可解释）
- [x] sim_cycle 不变
- [x] policy_bypass = 0（无真实 bypass）
- [x] 可判断是否进入 Phase 4：**是**，Phase 4 hook 点已确认（memory_cycle bypassL1D 标志）
