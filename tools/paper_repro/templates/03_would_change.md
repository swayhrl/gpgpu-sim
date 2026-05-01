# Stage 03: Would-Change Telemetry

## 目标
实现"如果机制生效，会发生什么"的 telemetry，不实际改变行为。would_change_block > 0 for target workloads，sim_cycle 不变。

## 允许
- 在 scheduler_unit::cycle() 中添加 telemetry 计数（不阻塞 issue_warp）
- 添加 would_change_attempt / would_change_block / would_change_allow 计数器
- 创建 would-change-on config
- 运行 quick set

## 禁止
- 修改 issue_warp 调用（不阻塞任何指令）
- 改变 cache 行为

## 必须产出
- feature_off 7/7 pass
- would-change-on 7/7 pass（sim_cycle 不变，would_change_block > 0 for target workloads）
- experiments/paper-<key>/would_change_check.csv
- docs/papers/<key>_round_would_change.md

## round_state.yaml 必须字段
- would_change_signal: true/false
- sim_cycle_unchanged: true/false
- load_gate_block: 0（必须为 0）

## 成功标准
sim_cycle 不变；would_change_block > 0 for at least one target workload。

## 10 分钟 checkpoint
如果执行超过 10 分钟，暂停并输出：would_change 信号是否出现，sim_cycle 是否不变，未完成哪些。
