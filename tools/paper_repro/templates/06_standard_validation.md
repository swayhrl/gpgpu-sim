# Stage 06: Standard Validation

## 目标
在 standard workload set（13 个）上做完整回归，验证机制在更大范围 workload 上的行为。只在 focused validation 通过后执行。

## 前置条件（必须满足才能执行本 stage）
- focused validation 已完成
- conservative config cycle 方向正确（或已明确解释为何不正确）
- feature_off 7/7 pass

## 允许
- 运行 standard workload set（13 个）
- 比较 feature_off / best_config 两类 config

## 禁止
- 修改 src/
- 新增机制
- 在 cycle 方向错误时跑 standard（会产生误导性结果）

## 必须产出
- experiments/paper-<key>/standard_validation.csv
- docs/papers/<key>_round_standard_validation.md
- 明确结论：HCS workload 是否有 cycle 改善，CI workload 是否无回归

## round_state.yaml 必须字段
- source_changed: false
- workloads_run: [列出 13 个]
- hcs_workloads_improved: [列出]
- ci_workloads_no_regression: [列出]
- recommend_next: final_report 或 mechanism_fix

## 成功标准
- HCS workload：cycle 减少（或 main_signal > 0 且 cycle 不增加）
- CI workload：cycle delta < 1%

## 10 分钟 checkpoint
如果执行超过 10 分钟，暂停并输出：已跑哪些 workload，HCS 结果，CI 结果，未完成哪些。
