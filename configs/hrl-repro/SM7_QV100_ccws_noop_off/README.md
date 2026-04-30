# configs/hrl-repro/SM7_QV100_ccws_noop_off/

Base config: `configs/tested-cfgs/SM7_QV100/`  
Branch: `hrl/paper/ccws-repro-v0`  
Purpose: CCWS Round T feature_off — explicit `gpgpu_enable_ccws=0`  
Feature state: `feature_off` (no-op; identical behavior to baseline)

## Changed knobs vs. base

| Knob | Value | Reason |
|------|-------|--------|
| `-gpgpu_enable_ccws` | `0` | Explicit off; default is already 0 but made explicit for documentation |
| `-gpgpu_ccws_debug` | `0` | Debug trace off |

All other parameters are identical to `SM7_QV100`.  
Expected: `sim_cycle` ≈ baseline; `paper_ccws_enabled = 0`.
