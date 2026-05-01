# Stage 02: Instrumentation-Only Telemetry

## 目标
实现论文的 signal 采集（VTA-like probe、score 计算等），纯计数，不改变调度行为。sim_cycle 必须与 feature_off 完全一致。

## 允许
- 修改 shader.h / shader.cc（添加 per-warp 数据结构和计数器）
- 修改 gpu-sim.cc（聚合 stats）
- 创建 telemetry-on config
- 运行 quick set

## 禁止
- 修改 scheduler_unit::cycle() 的 issue_warp 调用
- 阻塞任何指令
- 改变 cache replacement 逻辑

## 必须产出
- feature_off 7/7 pass（sim_cycle 不变）
- feature_on 7/7 pass（sim_cycle 不变，signal > 0 for target workloads）
- experiments/paper-<key>/telemetry_check.csv
- docs/papers/<key>_round_telemetry.md

## round_state.yaml 必须字段
- source_changed: true
- signal_present: true/false（target workload 上 main signal > 0）
- sim_cycle_unchanged: true/false
- load_gate_block: 0（必须为 0）

## 成功标准
sim_cycle 与 feature_off 完全相同；main signal（如 vta_hit）> 0 for L1D-miss workloads。

## 10 分钟 checkpoint
如果执行超过 10 分钟，暂停并输出：已实现哪些 signal，sim_cycle 是否不变，未完成哪些。
