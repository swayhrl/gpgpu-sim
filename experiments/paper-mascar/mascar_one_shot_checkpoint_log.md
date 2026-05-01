# Mascar One-Shot Checkpoint Log

append-only log — 每个 Phase 结束时追加一次。

---

## Checkpoint: Phase 0 — Reading / Plan

- Phase: 0 (Reading / Plan / Preflight)
- 当前状态: complete
- 已完成:
  - preflight checks 通过（正确分支、干净工作树）
  - reading notes 创建：docs/papers/mascar_reading_notes.md
  - repro plan 创建：docs/papers/mascar_repro_plan.md
  - 实验目录创建：experiments/paper-mascar/（README, config_matrix.csv, round_state.yaml）
  - mascar.yaml 更新（reproduction_status）
  - CLAUDE.md 更新（Mascar 状态记录）
  - checkpoint log 初始化
- 正在做: 准备 Phase 0 commit
- 修改文件:
  - docs/papers/mascar_reading_notes.md (new)
  - docs/papers/mascar_repro_plan.md (new)
  - experiments/paper-mascar/README.md (new)
  - experiments/paper-mascar/config_matrix.csv (new)
  - experiments/paper-mascar/round_state.yaml (new)
  - tools/paper_repro/papers/mascar.yaml (updated)
  - CLAUDE.md (updated)
  - experiments/paper-mascar/mascar_one_shot_checkpoint_log.md (this file)
- 已运行验证: 无（Phase 0 禁止跑 workload）
- 当前风险: 无，纯文档阶段
- 是否建议继续: 是，进入 Phase 1
- 下一步: Phase 1 no-op config + stats

---

## Checkpoint: Phase 1 — No-op Config + Stats

- Phase: 1 (No-op Config + Stats)
- 当前状态: complete
- 已完成:
  - Mascar config knobs 添加（7 个，全部 default 0）
  - Mascar stats 变量添加（7 个，全部初始化为 0）
  - option_parser_register 添加（gpu-sim.cc）
  - stats 打印添加（gpu-sim.cc paper_mascar_* section）
  - cluster/core 聚合方法添加（shader.h + shader.cc）
  - config 目录创建：mascar_noop_off / mascar_noop_on
  - 编译通过（warnings only，无 error）
  - vecadd noop_off: 5569 = baseline ✓
  - vecadd noop_on: 5569 = baseline ✓
  - hotspot noop_off: 6931 = baseline ✓
  - hotspot noop_on: 6931 = baseline ✓
  - bfs noop_off: 6665 ✓
- 修改文件:
  - src/gpgpu-sim/shader.h (Mascar knobs + stats vars + inline methods)
  - src/gpgpu-sim/shader.cc (init + cluster aggregation)
  - src/gpgpu-sim/gpu-sim.cc (option registration + stats printing)
  - configs/hrl-repro/SM7_QV100_mascar_noop_off/ (new)
  - configs/hrl-repro/SM7_QV100_mascar_noop_on/ (new)
- 已运行验证: vecadd×2, hotspot×2, bfs×1
- 当前风险: 无
- 是否建议继续: 是，进入 Phase 2
- 下一步: Phase 2 memory pressure telemetry
