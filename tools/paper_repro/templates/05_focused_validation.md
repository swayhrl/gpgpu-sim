# Stage 05: Focused Validation

## 目标
在 focused workload set（7 个）上验证机制趋势，比较 conservative / aggressive 两类 config，判断是否足够进入 final report 或 standard validation。

## 允许
- 运行 focused workload set（7 个）
- 比较 feature_off / conservative / aggressive 三类 config
- 记录 cycle delta、main_signal、gate_block

## 禁止
- 修改 src/
- 新增机制
- 跑 standard set（本 stage 不允许）
- 自动 commit/tag/push

## 必须产出
- experiments/paper-<key>/focused_validation.csv
- docs/papers/<key>_round_focused_validation.md
- 明确结论：是否建议进入 standard validation，或直接进入 final report

## round_state.yaml 必须字段
- source_changed: false
- workloads_run: [列出 7 个]
- configs_run: [feature_off, conservative, aggressive]
- conservative_signal_summary
- aggressive_signal_summary
- workloads_sensitive_to_mechanism
- recommend_next: standard_validation 或 final_report

## 成功标准
- feature_off 7/7 pass
- conservative config：至少 1 个 workload 有 main_signal > 0
- aggressive config：证明机制 active（即使 over-gating）
- 明确说明 cycle 方向是否正确

## 注意事项（来自 CCWS 经验）
- threshold < base_score 会 deadlock，检查 lg_score_threshold >= lls_base_score
- tiny workload 可能不代表论文 HCS 场景
- cycle 方向错误不等于机制错误，需要分析根本原因

## 10 分钟 checkpoint
如果执行超过 10 分钟，暂停并输出：已跑哪些 workload，conservative 结果，未完成哪些。
