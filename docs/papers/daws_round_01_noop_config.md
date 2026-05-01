# DAWS Round 01: No-op Config and Stats

**Round**: AUTO-3
**Date**: 2026-05-01
**Branch**: hrl/paper/daws-repro-v0

---

## 目标

在 GPGPU-Sim 中添加 DAWS no-op config knobs 和 `paper_daws_*` stats 占位符，验证 feature_off 不改变 baseline 行为。

---

## 新增 Config Knobs

在 `src/gpgpu-sim/shader.h` 的 `shader_core_config` 中新增（shader.h:1801–1810）：

| Knob | 默认值 | 说明 |
|------|--------|------|
| `gpgpu_enable_daws` | 0 | master switch |
| `gpgpu_daws_enable_telemetry` | 0 | Round 02: divergence event probe |
| `gpgpu_daws_enable_would_throttle` | 0 | Round 03: would-throttle telemetry |
| `gpgpu_daws_enable_throttling` | 0 | Round 04: actual warp throttling |
| `gpgpu_daws_footprint_threshold` | 50 | L1 capacity fraction (0-100) |
| `gpgpu_daws_min_active_threads` | 1 | min active threads to trigger |
| `gpgpu_daws_debug` | 0 | per-cycle debug trace |

在 `src/gpgpu-sim/gpu-sim.cc` 中注册（gpu-sim.cc:742–769）。

---

## 新增 Stats

在 `gpu-sim.cc` print_stats() 中新增（gpu-sim.cc:1733–1739）：

```
paper_daws_enabled = <gpgpu_enable_daws>
paper_daws_divergence_event = 0   (placeholder)
paper_daws_footprint_update = 0   (placeholder)
paper_daws_would_throttle = 0     (placeholder)
paper_daws_throttle_block = 0     (placeholder)
```

---

## 新增 Configs

| Config | 路径 | 说明 |
|--------|------|------|
| `SM7_QV100_daws_noop_off` | configs/hrl-repro/ | gpgpu_enable_daws=0 |
| `SM7_QV100_daws_noop_on` | configs/hrl-repro/ | gpgpu_enable_daws=1, 子功能全 0 |

---

## 验证结果

| Workload | Config | sim_cycle | baseline | delta | paper_daws_throttle_block | pass |
|----------|--------|-----------|----------|-------|--------------------------|------|
| vecadd | daws_noop_off | 5569 | 5569 | 0 | 0 | ✓ |
| vecadd | daws_noop_on | 5569 | 5569 | 0 | 0 | ✓ |
| rodinia_hotspot | daws_noop_off | 6931 | 6931 | 0 | 0 | ✓ |
| rodinia_bfs | daws_noop_off | 136110 | 136110 | 0 | 0 | ✓ |

**结论**：feature_off 和 noop_on 均不改变 sim_cycle，paper_daws_* 行为计数全为 0。

---

## 下一步

Round 02（AUTO-4）：在 `scheduler_unit::cycle()` 中添加 divergence event probe，当 `active_count() < warp_size` 时递增 `paper_daws_divergence_event` 计数器。不改变调度行为。
