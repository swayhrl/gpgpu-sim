# configs/hrl-repro/SM7_QV100_ccws_vta_probe_off/

Base config: `configs/tested-cfgs/SM7_QV100/`  
Branch: `hrl/paper/ccws-repro-v0`  
Purpose: CCWS Round V — VTA probe baseline (probe disabled, feature_off)  
Feature state: `feature_off`

## Changed knobs vs. base SM7_QV100

| Knob | Base value | New value | Reason |
|------|-----------|-----------|--------|
| `-gpgpu_enable_ccws` | (default 0) | `0` | Explicit feature_off |
| `-gpgpu_ccws_enable_vta_probe` | (default 0) | `0` | VTA probe disabled |

## Expected behavior

- `paper_ccws_enabled = 0`; all `paper_ccws_*` counters = 0
- `sim_cycle` ≈ LRR baseline (no behavior change)
- Used as Round V feature_off reference
