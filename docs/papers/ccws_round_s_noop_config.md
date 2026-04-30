# CCWS Round S — No-op Config Notes

_Created: 2026-04-30 (Round S, Stages S2–S3). Branch: `hrl/paper/ccws-repro-v0`._

---

## What was added

### Config knobs (all default off)

All knobs registered in `shader_core_config::reg_options()` in `gpu-sim.cc` (before the closing
`}` at line 665 in the pre-change file). Member variables added at the end of `shader_core_config`
in `shader.h` (after `m_specialized_unit_num`, line 1719 in the pre-change file).

| Knob | Type | Default | Notes |
|------|------|---------|-------|
| `-gpgpu_enable_ccws` | `int` | `0` | Master CCWS switch |
| `-gpgpu_ccws_enable_swl` | `int` | `0` | SWL comparison arm |
| `-gpgpu_ccws_swl_limit` | `unsigned` | `32` | SWL max warps per scheduler |
| `-gpgpu_ccws_base_locality_score` | `unsigned` | `100` | LLS base/floor value |
| `-gpgpu_ccws_k_throttle` | `float` | `8.0` | LLDS multiplier |
| `-gpgpu_ccws_vta_entries_per_warp` | `unsigned` | `16` | VTA size per warp |
| `-gpgpu_ccws_score_decay_interval` | `unsigned` | `1` | Cycles between LLS decay steps |
| `-gpgpu_ccws_gate_loads_only` | `int` | `1` | Gate loads only (1) or all mem ops (0) |
| `-gpgpu_ccws_debug` | `int` | `0` | Per-cycle trace |

### Stats output (no-op, all zero when feature_off)

Added in `gpgpu_sim::print_stats()` in `gpu-sim.cc`, immediately after
`print_cacheinst_stats(stdout, l2_stats, "L2")`.

```
paper_ccws_enabled = 0
paper_ccws_vta_probe = 0
paper_ccws_vta_hit = 0
paper_ccws_lost_locality_event = 0
paper_ccws_score_update = 0
paper_ccws_score_decay = 0
paper_ccws_load_gate_attempt = 0
paper_ccws_load_gate_block = 0
paper_ccws_load_gate_allow = 0
```

---

## Feature_off ≈ baseline verification (Stage S3)

| Workload | Baseline sim_cycle | Feature_off sim_cycle | Match |
|----------|-------------------|-----------------------|-------|
| vecadd | 5569 | **5569** | ✓ |
| strided_access | 5825 | **5825** | ✓ |
| page_stride_access | 5851 | **5851** | ✓ |
| mutual_tiled | 7479 | **7479** | ✓ |

`paper_ccws_load_gate_block = 0` for all runs. ✓

---

## Next stage

Stage S5: VTA-like prototype + LDD + LLS score update/decay.  
See `ccws_repro_plan.md` §9 for full stage breakdown.
