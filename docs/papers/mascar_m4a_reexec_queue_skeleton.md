# Mascar M4A Re-Execution Queue Skeleton

M4A adds the load re-execution queue control plane, passive would-enqueue
telemetry, and stats. It does not require active behavior: the active queue is
off unless `gpgpu_mascar_enable_reexec_queue=1`.

## Implemented

- New M4 knobs are declared in `src/gpgpu-sim/shader.h:2086` through
  `src/gpgpu-sim/shader.h:2099` and registered with default-off active behavior
  in `src/gpgpu-sim/gpu-sim.cc:809` through `src/gpgpu-sim/gpu-sim.cc:861`.
- M4 stats and the queue entry type are defined in
  `src/gpgpu-sim/shader.h:491` through `src/gpgpu-sim/shader.h:558`.
- `ldst_unit` owns the queue, per-warp in-queue bit, and stats at
  `src/gpgpu-sim/shader.h:1742` through `src/gpgpu-sim/shader.h:1746`.
- Passive would-enqueue accounting is implemented at
  `src/gpgpu-sim/shader.cc:2364` through `src/gpgpu-sim/shader.cc:2388`.
- M4 stats are printed as `paper_mascar_m4_*` at
  `src/gpgpu-sim/gpu-sim.cc:2066` through `src/gpgpu-sim/gpu-sim.cc:2121`.

## Config

`configs/hrl-repro/SM7_QV100_mascar_m4_reexec_probe_on/` enables:

- M1 L1 saturation probe.
- M2 owner scheduling.
- M3 hit-only/NACK.
- M4 re-exec probe.
- Active M4 re-exec queue off.
- Old proxy scheduling off.

## Scope

M4A is passive for the re-execution queue. It records would-enqueue outcomes for
M3 NACKs and real L1D `RESERVATION_FAIL`, while preserving the existing cache
return behavior when `gpgpu_mascar_enable_reexec_queue=0`.

