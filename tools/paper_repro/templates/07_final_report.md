# Stage 07: Final Report

## 目标
撰写论文复现 final report，总结机制理解、实现状态、近似说明、实验结论。不跑新实验，不改 src。

## 允许
- 写 docs/papers/<key>_final_reproduction_report.md
- 写 experiments/paper-<key>/final_summary.csv
- 更新 ccws_repro_plan.md / CLAUDE.md / round_state.yaml

## 禁止
- 修改 src/
- 跑新实验
- 宣称 exact reproduction（如果有近似实现）

## 必须产出
- docs/papers/<key>_final_reproduction_report.md（按下方结构）
- experiments/paper-<key>/final_summary.csv
- round_state.yaml 更新

## final report 结构
1. Scope and Goal
2. Paper Mechanism Recap
3. GPGPU-Sim Mapping
4. Implemented Stages（按 round 总结）
5. What Was Successfully Reproduced
6. Approximation and Deviations（必须明确写）
7. Key Experimental Findings
8. Interpretation（为什么结果与原论文不同）
9. Reproduction Status（表格：faithful/partial/no）
10. Recommended Next Steps

## round_state.yaml 必须字段
- stage: final_report
- source_changed: false
- experiments_run: false
- final_report_created: true
- mechanism_chain_reproduced: true/false
- faithful_reproduction_status
- recommend_next

## 10 分钟 checkpoint
如果执行超过 10 分钟，暂停并输出：已完成哪些章节，未完成哪些。
