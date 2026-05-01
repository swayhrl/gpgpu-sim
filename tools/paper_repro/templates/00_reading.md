# Stage 00: Paper Reading

## 目标
深度阅读论文，理解机制，映射到 GPGPU-Sim 源码。不修改任何文件。

## 允许
- 阅读论文 PDF
- 阅读 GPGPU-Sim 源码
- 写阅读笔记（docs/papers/<key>_reading_notes.md）
- 写 repro plan（docs/papers/<key>_repro_plan.md）

## 禁止
- 修改 src/
- 运行 workload
- 创建 config

## 必须产出
- docs/papers/<key>_reading_notes.md
- docs/papers/<key>_repro_plan.md
- experiments/paper-<key>/config_matrix.csv（表头）
- experiments/paper-<key>/result_manifest.csv（表头）

## round_state.yaml 必须字段
- round, stage: reading, status: complete
- source_changed: false
- experiments_run: false
- mechanism_chain: [列出论文机制层]
- gpgpu_sim_mapping: [列出映射点]
- highest_risk_change: [最高风险修改点]
- recommend_next: Stage 01 no-op config

## 10 分钟 checkpoint
如果执行超过 10 分钟，暂停并输出：已完成哪些章节阅读，未完成哪些，下一步。
