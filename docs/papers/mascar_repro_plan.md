# Mascar Approximate Reproduction Plan

**论文**：Mascar: Speeding up GPU Warps by Reducing Memory Pitstops (HPCA 2015)
**分支**：hrl/paper/mascar-repro-v0
**最后更新**：2026-05-01 (Phase 0)

---

## 目标

在 GPGPU-Sim 上实现 Mascar 近似复现（approximate reproduction）：
- 不追求 100% faithful；
- 确认机制链路可工作：信号 → 判断 → 调度动作；
- 不实现 pipeline replay / re-execution；
- feature_off 全程不破坏 baseline。

---

## 论文机制概述

Mascar 检测 warp 的 memory subsystem 是否饱和（"memory pitstop"），并在 pitstop 状态时降低该 warp 的调度优先级，让 memory subsystem 有机会排空，同时调度其他 warp 执行。

核心链路：
```
memory stall probe
  → per-warp stall streak counter
  → pitstop 判定（stall_streak > threshold）
  → scheduler deprioritize（skip pitstop warp）
  → pitstop 恢复（stall_streak 重置 or max_skip_streak 保护）
```

---

## 阶段计划

### Phase 0：Reading / Plan（本阶段）

- [x] 阅读笔记
- [x] 复现计划
- [x] 实验目录结构
- [x] commit (5979161)

### Phase 1：No-op Config + Stats

- [x] 添加 config knobs（全部默认关闭）
- [x] 添加 paper_mascar_* stats 占位（行为不变）
- [x] 创建 feature_off / feature_on_noop 配置
- [x] 验证：vecadd, rodinia_hotspot, rodinia_bfs
- [x] 成功标准：编译通过；feature_off cycle = baseline；stats = 0
- [x] commit

### Phase 2：Memory Pressure / Pitstop Telemetry

- 添加 per-warp memory stall streak 计数（passive，不改行为）
- 添加 saturation event / pitstop event 计数
- 验证：vecadd, rodinia_hotspot, rodinia_srad_v2, rodinia_bfs, strided_access
- 成功标准：telemetry_on cycle 不变；memory-bound workloads 有 stall 信号

### Phase 3：Would-Change Telemetry

- 基于 stall_streak signal 计算 would_deprioritize decision
- 添加 would_prioritize / would_delay 计数（仍不改行为）
- 验证：vecadd, rodinia_hotspot, rodinia_srad_v2, rodinia_bfs, strided_access, polybench_fdtd2d
- 成功标准：would-change 有信号；行为不变

### Phase 4：Minimal Mechanism（高风险）

- **进入前必须输出 risk checkpoint**
- 在 `scheduler_unit::cycle()` 实现最小 skip 逻辑
- 不改 ldst_unit / MSHR / cache miss return
- 不实现 replay / re-execution
- max_skip_streak counter 防 deadlock
- 验证：vecadd, rodinia_hotspot, rodinia_srad_v2, rodinia_bfs, strided_access
- 成功标准：编译通过；feature_off 不变；policy_on 有 skip_count 信号；无 deadlock

### Phase 5：Focused Validation（仅 Phase 4 成功时）

- 不改 src
- focused workloads × configs
- 成功标准：结果可解释，不跑 standard

### Phase 6：Final Report

- 总结复现状态
- 明确 approximation 和 deviation
- 不改 src，不跑实验

---

## Config 计划

| Config dir | 功能 |
|-----------|------|
| SM7_QV100_mascar_noop_off | feature_off，所有 mascar knob = 0 |
| SM7_QV100_mascar_noop_on | feature_on_noop，enable_mascar=1 但无 telemetry |
| SM7_QV100_mascar_telemetry_on | enable_telemetry=1 |
| SM7_QV100_mascar_would_change_on | enable_would_deprioritize=1 |
| SM7_QV100_mascar_policy_on | enable_scheduling=1（最小 policy） |

---

## 关键约束

- `gpgpu_mascar_max_skip_streak >= 1`（防 deadlock）
- stall_threshold 默认值应保守（如 8），避免 over-skip
- Phase 4 停机条件：需要改 ldst_unit / MSHR / scoreboard / cache miss return / interconnect / DRAM
- 不实现 re-execution/replay

---

## Approximation 列表

| 原论文特性 | 本次近似 | 已知偏差 |
|----------|---------|---------|
| MSHR entry count 监测 | per-warp stall streak counter | 信号粒度粗 |
| 精确 pitstop 进入/退出 | threshold + streak counter | 简化逻辑 |
| Re-execution | 不实现 | 丢失主要 speedup |
| 原论文 GPU arch | GPGPU-Sim QV100 | 定量不可比 |

---

## 状态记录（各 Phase 更新）

| Phase | 状态 | commit | 备注 |
|-------|------|--------|------|
| Phase 0 | complete | 5979161 | reading + plan |
| Phase 1 | complete | — | noop config; 待 commit |
| Phase 2 | pending | — | |
| Phase 3 | pending | — | |
| Phase 4 | pending | — | 高风险 |
| Phase 5 | pending | — | 依赖 Phase 4 |
| Phase 6 | pending | — | |
