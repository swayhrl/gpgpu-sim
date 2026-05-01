# Stage 01: No-op Config + Stats

## 目标
添加 config knobs 和 paper_* stats 计数器，feature flag 默认 off，编译通过，feature_off quick set pass。

## 允许
- 修改 shader.h（添加 config 成员变量）
- 修改 gpu-sim.cc（注册 option_parser，添加 print_stats 输出）
- 创建 configs/hrl-repro/<key>_noop_off/ 和 <key>_noop_on/
- 运行 quick set（7 workloads）

## 禁止
- 修改任何调度逻辑
- 修改 cache 逻辑
- 添加任何行为改变代码

## 必须产出
- 编译通过（make -j$(nproc)）
- feature_off quick set 7/7 pass（sim_cycle = baseline，所有 paper_* = 0）
- experiments/paper-<key>/noop_behavior_check.csv
- docs/papers/<key>_round_noop.md

## round_state.yaml 必须字段
- source_changed: true（shader.h + gpu-sim.cc）
- knobs_added: [列出所有新 knob]
- stats_added: [列出所有新 stat]
- feature_off_pass: true/false
- compile_status: pass/fail

## 成功标准
feature_off sim_cycle = baseline ±0（精确相等）；所有 paper_* = 0。

## 10 分钟 checkpoint
如果执行超过 10 分钟，暂停并输出：已添加哪些 knob，编译状态，未完成哪些。
