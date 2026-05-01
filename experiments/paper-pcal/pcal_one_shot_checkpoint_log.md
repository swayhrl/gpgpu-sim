# PCAL One-Shot Checkpoint Log

append-only — 每个 Phase 结束时追加，10 分钟触发时追加，Phase 4 前追加 risk checkpoint。

---

## Checkpoint: Phase 0 preflight

- Phase: Phase 0
- 当前状态: 完成
- 已完成: 分支/目录/reading 产物确认；round_state.yaml 更新为 preflight complete
- 正在做: N/A（Phase 0 只检查）
- 修改文件: experiments/paper-pcal/round_state.yaml（字段更新）
- 已运行验证: pwd / git branch / ls reading artifacts
- 当前风险: 无
- 是否建议继续: 是
- 下一步: Phase 1 no-op config + stats

---

## Checkpoint: Phase 1 no-op config + stats

- Phase: Phase 1
- 当前状态: 完成 ✓，commit d28a6ad
- 已完成: 7 knobs + 9 stats 添加（shader.h/cc/gpu-sim.cc）；2 个 config 目录创建；vecadd/hotspot/srad_v2 × noop_off/noop_on 验证通过
- 正在做: N/A
- 修改文件: src/gpgpu-sim/shader.h, shader.cc, gpu-sim.cc; configs/hrl-repro/SM7_QV100_pcal_noop_{off,on}/; 文档/实验文件
- 已运行验证: feature_off cycle=5569/6931/8236（与 baseline 一致）；noop_on cycle 不变；所有 paper_pcal_* 行为计数 = 0
- 当前风险: 无（Phase 1 是纯占位，无行为改动）
- 是否建议继续: 是
- 下一步: Phase 2 cache pressure telemetry

---

## Checkpoint: Phase 2 cache pressure telemetry

- Phase: Phase 2
- 当前状态: 完成 ✓，待 commit
- 已完成: pcal_probe_access() 方法（window-based miss rate）；两条 L1D 路径 hook；4 workload 信号验证
- 正在做: N/A
- 修改文件: src/gpgpu-sim/shader.h, shader.cc; configs/hrl-repro/SM7_QV100_pcal_telemetry_on/
- 已运行验证: vecadd/hotspot/srad_v2/fdtd2d × telemetry_on；sim_cycle 不变；miss_event/classified_low 有信号
- 当前风险: classified_high=0（所有 workload miss_rate>50%，符合预期）；window_size=8 适配小 workload
- 是否建议继续: 是
- 下一步: Phase 3 would-change bypass telemetry

---
