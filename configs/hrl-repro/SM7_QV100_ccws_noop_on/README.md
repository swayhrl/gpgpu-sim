# configs/hrl-repro/SM7_QV100_ccws_noop_on/

Base config: `configs/tested-cfgs/SM7_QV100/`  
Branch: `hrl/paper/ccws-repro-v0`  
Purpose: CCWS Round T feature_on_noop — `gpgpu_enable_ccws=1`, but no VTA/LLS/gating implemented yet  
Feature state: `feature_on_noop` (no behavior change; only `paper_ccws_enabled=1` in output)

## Changed knobs vs. base

| Knob | Value | Reason |
|------|-------|--------|
| `-gpgpu_enable_ccws` | `1` | Enable flag set; verifies flag is parsed and reflected in stats |
| `-gpgpu_ccws_debug` | `0` | Debug trace off |

All other parameters are identical to `SM7_QV100`.  
Expected: `sim_cycle` ≈ baseline; `paper_ccws_enabled = 1`; all behavior counters still 0.
