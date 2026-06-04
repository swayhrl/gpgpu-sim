# Mascar M2A Notes

M2A completed before M2B active scheduling was added.

- Passive knobs registered with default-off scheduling:
  `src/gpgpu-sim/gpu-sim.cc:783` through `src/gpgpu-sim/gpu-sim.cc:794`.
- Recent L1 saturation flag implemented in ldst_unit:
  `src/gpgpu-sim/shader.h:1633`,
  `src/gpgpu-sim/shader.cc:2204`,
  `src/gpgpu-sim/shader.cc:2246`.
- Shared per-SM MP/owner/WST state placed in `shader_core_ctx`:
  `src/gpgpu-sim/shader.h:3086` through `src/gpgpu-sim/shader.h:3094`.
- Passive cycle update and owner release guards:
  `src/gpgpu-sim/shader.cc:2325`.
- Passive WST and candidate telemetry:
  `src/gpgpu-sim/shader.cc:2370`.
- M2A config keeps new active scheduling off:
  `configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/gpgpusim.config:248`.

Validation at the M2A checkpoint:

- `git diff --check`: pass.
- `source setup_environment release && make -j2`: pass after fixing a
  const-correctness issue in `mascar_m2_current_cycle()`.
- No benchmark was run at M2A.
