# Mascar Round 01：No-op Config + Stats

**阶段**：Phase 1
**日期**：2026-05-01
**分支**：hrl/paper/mascar-repro-v0

---

## 目标

添加 Mascar config knobs（全部默认关闭）和 paper_mascar_* stats 占位。
验证 feature_off 和 feature_on_noop 不改变 sim_cycle，stats 均为 0。

---

## 改动文件

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| src/gpgpu-sim/shader.h | 修改 | 添加 Mascar config knobs 和 stats 变量 |
| src/gpgpu-sim/shader.cc | 修改 | 添加 Mascar 计数器初始化和 cluster 聚合方法 |
| src/gpgpu-sim/gpu-sim.cc | 修改 | 添加 option_parser_register 和 stats 打印 |
| configs/hrl-repro/SM7_QV100_mascar_noop_off/ | 新建 | feature_off config |
| configs/hrl-repro/SM7_QV100_mascar_noop_on/ | 新建 | feature_on_noop config |

---

## Config Knobs（全部 default 0）

| Knob | 默认值 | 说明 |
|------|-------|------|
| gpgpu_enable_mascar | 0 | master enable |
| gpgpu_mascar_enable_telemetry | 0 | Phase 2: memory stall probe |
| gpgpu_mascar_enable_would_deprioritize | 0 | Phase 3: would-deprioritize telemetry |
| gpgpu_mascar_enable_scheduling | 0 | Phase 4: actual scheduler skip |
| gpgpu_mascar_stall_threshold | 8 | consecutive stall cycles to trigger |
| gpgpu_mascar_max_skip_streak | 4 | max consecutive skips before force-allow |
| gpgpu_mascar_debug | 0 | debug trace |

---

## Stats（全部 default 0）

| Stat | 说明 |
|------|------|
| paper_mascar_enabled | = gpgpu_enable_mascar |
| paper_mascar_mem_stall_event | per-warp stall 累积次数 |
| paper_mascar_saturation_event | 内存饱和事件计数 |
| paper_mascar_pitstop_event | pitstop 进入事件计数 |
| paper_mascar_would_deprioritize | would-deprioritize 计数 |
| paper_mascar_skip_count | 实际 skip 计数 |
| paper_mascar_allow_count | force-allow 计数（防 deadlock） |

---

## 验证结果

| Workload | Config | sim_cycle | baseline_cycle | cycle_delta | mascar_* stats | PASS |
|----------|--------|-----------|----------------|-------------|----------------|------|
| vecadd | noop_off | 5569 | 5569 | 0 | 全部 = 0 | PASS |
| vecadd | noop_on | 5569 | 5569 | 0 | 全部 = 0 | PASS |
| rodinia_hotspot | noop_off | 6931 | 6931 | 0 | 全部 = 0 | PASS |
| rodinia_hotspot | noop_on | 6931 | 6931 | 0 | 全部 = 0 | PASS |
| rodinia_bfs | noop_off | 6665 | 6665 | 0 | 全部 = 0 | PASS |

---

## 成功标准评估

- [x] 编译通过（warnings only）
- [x] feature_off cycle = baseline
- [x] feature_on_noop cycle = baseline
- [x] paper_mascar_* 行为计数全部为 0
- [x] 没有真实 Mascar 行为改动

Phase 1 通过，可以进入 Phase 2。
