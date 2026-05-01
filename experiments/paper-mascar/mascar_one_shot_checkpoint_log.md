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

---

## Checkpoint: Phase 2 — Memory Pressure Telemetry

- Phase: 2 (Memory Pressure / Pitstop Telemetry)
- 当前状态: complete
- 已完成:
  - mascar_record_mem_stall / mascar_reset_stall_streak 方法添加（shader.h）
  - scheduler_unit::cycle() telemetry hook 添加（shader.cc）
  - telemetry_on config 创建（SM7_QV100_mascar_telemetry_on）
  - 编译通过
  - feature_off 回归：vecadd=5569, hotspot=6931 ✓
  - telemetry_on cycle 不变 ✓
  - mem_stall_event: hotspot=12150, srad_v2=3989 (有信号)
  - pitstop_event=0 (threshold=8 保守，Phase 3 改用 streak>=2)
- 修改文件:
  - src/gpgpu-sim/shader.h (mascar telemetry methods)
  - src/gpgpu-sim/shader.cc (scheduler hook)
  - configs/hrl-repro/SM7_QV100_mascar_telemetry_on/ (new)
- 已运行验证: vecadd×2, hotspot×2, srad_v2×1, bfs×1, strided_access×1
- 当前风险: 无
- 是否建议继续: 是，进入 Phase 3
- 下一步: Phase 3 would-change telemetry

---

## Checkpoint: Phase 3 — Would-Change Telemetry

- Phase: 3 (Would-Change Scheduling Telemetry)
- 当前状态: complete
- 已完成:
  - mascar_check_would_deprioritize 方法添加（shader.h）
  - 触发条件：stall_streak >= 2（比 Phase 4 threshold=8 更敏感）
  - scheduler hook 更新（shader.cc：mem stall else 分支追加 would-check）
  - would_change_on config 创建
  - 编译通过
  - 所有 6 workload cycle 不变 ✓
  - would_deprioritize: hotspot=10025, srad_v2=3237, fdtd2d=1936
  - skip_count=0（无真实 policy）✓
  - Phase 4 可行性确认：hook 点明确，仅 shader.h+cc，no memory pipeline change
- 修改文件:
  - src/gpgpu-sim/shader.h (mascar_check_would_deprioritize)
  - src/gpgpu-sim/shader.cc (call in scheduler)
  - configs/hrl-repro/SM7_QV100_mascar_would_change_on/ (new)
- 已运行验证: vecadd, hotspot, srad_v2, bfs, strided_access, fdtd2d × would_change_on
- 当前风险: 低（纯被动）
- 是否建议继续: 是，进入 Phase 4（附 risk checkpoint）
- 下一步: Phase 4 minimal scheduling policy

---

## Checkpoint: Phase 4 — Minimal Scheduling Policy

- Phase: 4 (Minimal Scheduling Policy)
- 当前状态: complete
- 已完成:
  - config bug 修复：policy_on 有重复行 `-gpgpu_mascar_enable_scheduling 0` 和 `-gpgpu_mascar_stall_threshold 8` 覆盖了正确值 → scheduling 实际=0 导致 skip_count=0
  - stall_threshold 调整：8→2（threshold=8 时 pitstop_event=0，streak 从未达到 8；threshold=2 立即有信号）
  - skip gate 从 pre-scoreboard 移到 post-scoreboard（memory block 内部，CCWS would-gate 之后，m_mem_out->has_free 之前）
  - 编译通过（warnings only）
  - noop_off regression：vecadd=5569 ✓
  - policy_on 验证：skip_count > 0，allow_count > 0，ratio = max_skip_streak = 4（deadlock prevention ✓）
- policy_on 结果:
  - vecadd: 5569→5560 (-0.2%), skip=44, allow=11
  - hotspot: 6931→6923 (-0.1%), skip=8300, allow=2075
  - srad_v2: 15926→15918 (-0.05%), skip=5516, allow=1379
- 修改文件:
  - src/gpgpu-sim/shader.cc (gate 移位：pre→post-scoreboard)
  - configs/hrl-repro/SM7_QV100_mascar_policy_on/gpgpusim.config (bug fix + threshold=2)
  - configs/hrl-repro/SM7_QV100_mascar_policy_on/README.md (更新)
  - experiments/paper-mascar/round_state.yaml (更新)
- 根本原因分析：config 有重复 option 行，GPGPU-Sim option_parser 后值覆盖前值 → enable_scheduling=0 → skip gate always false
- 是否建议继续: 是，进入 Phase 5 focused validation
- 下一步: Phase 5 focused validation
