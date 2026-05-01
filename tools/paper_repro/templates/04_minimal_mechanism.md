# Stage 04: Minimal Mechanism

## 目标
实现最小可用的论文机制（真实行为改变），feature_off 不变，feature_on 产生 main_signal > 0。

## 允许
- 修改 scheduler_unit::cycle() 的 issue 逻辑（添加 gate/block 条件）
- 添加 enable_mechanism knob（default 0）
- 创建 mechanism-on config
- 运行 quick set

## 禁止
- 修改 feature_off 路径的任何逻辑
- 在 feature_off 时改变任何行为
- 跑 standard set（focused 之前不允许）

## 必须产出
- feature_off 7/7 pass（sim_cycle = baseline，main_signal = 0）
- feature_on quick set：main_signal > 0 for target workloads
- experiments/paper-<key>/minimal_mechanism_check.csv
- docs/papers/<key>_round_minimal_mechanism.md

## round_state.yaml 必须字段
- source_changed: true
- feature_off_pass: true
- main_signal_present: true/false
- sim_cycle_delta: [列出各 workload cycle delta]

## 成功标准
feature_off 精确 = baseline；feature_on main_signal > 0 for at least one target workload。

## 10 分钟 checkpoint
如果执行超过 10 分钟，暂停并输出：feature_off 是否通过，main_signal 是否出现，未完成哪些。
