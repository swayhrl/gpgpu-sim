# configs/hrl-repro/SM7_QV100_ccws_vta_probe_on/

Base config: `configs/tested-cfgs/SM7_QV100/`  
Branch: `hrl/paper/ccws-repro-v0`  
Purpose: CCWS Round V — VTA probe enabled (`gpgpu_ccws_enable_vta_probe=1`)  
Feature state: `vta_probe_on`

## Changed knobs vs. base SM7_QV100

| Knob | Base value | New value | Reason |
|------|-----------|-----------|--------|
| `-gpgpu_enable_ccws` | (default 0) | `1` | Enable CCWS instrumentation path |
| `-gpgpu_ccws_enable_vta_probe` | (default 0) | `1` | Enable VTA miss-side probe |

## Expected behavior

- `paper_ccws_enabled = 1`
- `paper_ccws_l1d_miss_seen > 0` for workloads with L1D misses
- `paper_ccws_vta_probe > 0` (every miss is probed)
- `paper_ccws_vta_hit > 0` for HCS-like workloads (repeated miss on same block)
- `paper_ccws_load_gate_block = 0` (no gating yet; Round V is instrumentation-only)
- `sim_cycle` ≈ baseline (no scheduling behavior change)
