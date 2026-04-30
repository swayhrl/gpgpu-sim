# configs/hrl-repro/SM7_QV100_ccws_swl_limit_8/

Base config: `configs/tested-cfgs/SM7_QV100/`  
Branch: `hrl/paper/ccws-repro-v0`  
Purpose: CCWS Round U — SWL static baseline, warp limit = 8 per scheduler  
Feature state: `swl_static`

## Changed knobs vs. base SM7_QV100

| Knob | Base value | New value | Reason |
|------|-----------|-----------|--------|
| `-gpgpu_scheduler` | `lrr` | `warp_limiting:2:8` | Enable swl_scheduler; GTO(2) prio; limit 8 warps per scheduler |
| `-gpgpu_enable_ccws` | (default 0) | `0` | Explicit; CCWS dynamic gating not enabled |

## Notes

SM7_QV100: ~16 supervised warps per scheduler.  
`warp_limiting:2:8` limits each scheduler to 8 warps — moderate SWL.  
This is the paper's sweet-spot SWL value for HCS workloads (Section 4).  
**Does NOT activate CCWS dynamic gating**; that requires VTA/LLS (Stage S5+).

## Expected behavior

- `paper_ccws_enabled = 0`; all `paper_ccws_*` counters = 0
- Most representative SWL limit for paper comparison
- Primary quick-set sweep target for Round U
