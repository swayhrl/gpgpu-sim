# configs/hrl-repro/SM7_QV100_ccws_swl_limit_4/

Base config: `configs/tested-cfgs/SM7_QV100/`  
Branch: `hrl/paper/ccws-repro-v0`  
Purpose: CCWS Round U — SWL static baseline, warp limit = 4 per scheduler  
Feature state: `swl_static`

## Changed knobs vs. base SM7_QV100

| Knob | Base value | New value | Reason |
|------|-----------|-----------|--------|
| `-gpgpu_scheduler` | `lrr` | `warp_limiting:2:4` | Enable swl_scheduler; GTO(2) prio; limit 4 warps per scheduler |
| `-gpgpu_enable_ccws` | (default 0) | `0` | Explicit; CCWS dynamic gating not enabled |

## Notes

SM7_QV100: 64 warps/SM ÷ 4 schedulers = ~16 supervised warps per scheduler.  
`warp_limiting:2:4` limits each scheduler to its 4 oldest-dynamic-id warps.  
This is a very aggressive limit — analogous to paper's SWL with k=4.  
**Does NOT activate CCWS dynamic gating**; that requires VTA/LLS (Stage S5+).

## Expected behavior

- `paper_ccws_enabled = 0`; all `paper_ccws_*` counters = 0
- `sim_cycle` may change significantly vs. baseline (SWL effect is real)
- Compute-bound workloads (mutual_tiled, polybench_gemm) likely degrade
- Memory-bound stalls (HCS-like) may improve or degrade depending on footprint
