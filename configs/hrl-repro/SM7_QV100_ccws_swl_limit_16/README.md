# configs/hrl-repro/SM7_QV100_ccws_swl_limit_16/

Base config: `configs/tested-cfgs/SM7_QV100/`  
Branch: `hrl/paper/ccws-repro-v0`  
Purpose: CCWS Round U — SWL static baseline, warp limit = 16 per scheduler  
Feature state: `swl_static`

## Changed knobs vs. base SM7_QV100

| Knob | Base value | New value | Reason |
|------|-----------|-----------|--------|
| `-gpgpu_scheduler` | `lrr` | `warp_limiting:2:16` | Enable swl_scheduler; GTO(2) prio; limit 16 warps per scheduler |
| `-gpgpu_enable_ccws` | (default 0) | `0` | Explicit; CCWS dynamic gating not enabled |

## Notes

SM7_QV100: ~16 supervised warps per scheduler.  
`warp_limiting:2:16` sets the limit equal to the supervised warp pool — **no effective limiting**.  
This config functions as a **GTO scheduler reference** (same set as LRR baseline but GTO-ordered).  
Useful for separating scheduler policy effect (LRR→GTO) from warp limiting effect.  
**Does NOT activate CCWS dynamic gating**; that requires VTA/LLS (Stage S5+).

## Expected behavior

- `paper_ccws_enabled = 0`; all `paper_ccws_*` counters = 0
- `sim_cycle` may differ from LRR baseline due to GTO vs LRR ordering difference
- Should be closer to baseline than limit_4/limit_8
