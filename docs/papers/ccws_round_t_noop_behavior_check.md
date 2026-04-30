# CCWS Round T — No-op Feature Behavior Check

_Created: 2026-04-30 (Round T). Branch: `hrl/paper/ccws-repro-v0`._

---

## 1. Purpose

Verify that the CCWS no-op config knobs and `paper_ccws_*` stats (added in Round S) do not alter
simulation behavior regardless of `gpgpu_enable_ccws` value. Specifically:

- `feature_off` (`gpgpu_enable_ccws=0`) ≈ `cache-inst-v0` baseline  
- `feature_on_noop` (`gpgpu_enable_ccws=1`) ≈ `feature_off` (no VTA/LLS/gating implemented)  
- `paper_ccws_enabled` correctly reflects the flag value  
- All behavior counters (vta_hit, load_gate_block, etc.) remain 0  

This is the safety gate before Stage S5 (VTA/LLD prototype).

---

## 2. Current Branch and Commit

| Field | Value |
|-------|-------|
| Branch | `hrl/paper/ccws-repro-v0` |
| HEAD commit | `d632fdd` (Add CCWS no-op config knobs and SWL audit) |
| Tag | `ccws-config-noop` |

---

## 3. Config State

### Created config directories (Round T)

| Dir | Base | `gpgpu_enable_ccws` | Purpose |
|-----|------|---------------------|---------|
| `configs/hrl-repro/SM7_QV100_ccws_noop_off/` | SM7_QV100 | `0` (explicit) | feature_off baseline |
| `configs/hrl-repro/SM7_QV100_ccws_noop_on/` | SM7_QV100 | `1` | feature_on_noop check |

Both configs are identical to `configs/tested-cfgs/SM7_QV100/` except for the appended CCWS lines.
The original `tested-cfgs/` was not modified.

### Base config note

`gpgpu_enable_ccws` does not appear in the SM7_QV100 base config; the registered default is `0`.
The `_noop_off` config makes this explicit; the `_noop_on` config changes it to `1`.

---

## 4. Workload Script Config Override Support

| Script | Native config override | After Round T change |
|--------|----------------------|---------------------|
| `scripts/run_one.sh` | No (reads manifest `config_local`) | Yes — `GPGPUSIM_CONFIG_OVERRIDE=<dir>` env var |
| `scripts/run_workload_set.sh` | No | Passes env through to `run_one.sh` ✓ |

**Change made**: Added `GPGPUSIM_CONFIG_OVERRIDE` env var support to `run_one.sh` (6 lines).
When set, it overrides `CONFIG_LOCAL` **and** copies config into the binary's execution directory
(extracted from the `cd <dir>` prefix in `RUN_COMMAND`). This is necessary because GPGPU-Sim
reads `gpgpusim.config` from the process working directory, not from the run output dir.

---

## 5. Build Status

| Check | Result |
|-------|--------|
| Incremental `make -j` | **Pass** (no recompilation needed; already clean from Round S) |
| Build commit | `d632fdd` |
| `libcudart.so` version | `4.2.0 (d632fdd-modified_0.0)` |

---

## 6. Quick Set Results

### feature_off (`gpgpu_enable_ccws=0`)

| Workload | sim_cycle | Result | paper_ccws_enabled | paper_ccws_load_gate_block |
|----------|-----------|--------|--------------------|---------------------------|
| vecadd | 5569 | PASS | 0 | 0 |
| strided_access | 5825 | PASS | 0 | 0 |
| page_stride_access | 5851 | PASS | 0 | 0 |
| atomic_contention | 5414 | PASS | 0 | 0 |
| mutual_tiled | 7479 | PASS | 0 | 0 |
| polybench_2dconv | 6652 | OK (no PASS string) | 0 | 0 |
| rodinia_hotspot | 6931 | OK (no PASS string) | 0 | 0 |

**Summary**: 5 explicit PASS + 2 completed, **failed = 0**

### feature_on_noop (`gpgpu_enable_ccws=1`, no VTA/LLS/gating)

| Workload | sim_cycle | Result | paper_ccws_enabled | paper_ccws_load_gate_block |
|----------|-----------|--------|--------------------|---------------------------|
| vecadd | 5569 | PASS | **1** | 0 |
| strided_access | 5825 | PASS | **1** | 0 |
| page_stride_access | 5851 | PASS | **1** | 0 |
| atomic_contention | 5414 | PASS | **1** | 0 |
| mutual_tiled | 7479 | PASS | **1** | 0 |
| polybench_2dconv | 6652 | OK | **1** | 0 |
| rodinia_hotspot | 6931 | OK | **1** | 0 |

**Summary**: 5 explicit PASS + 2 completed, **failed = 0**

---

## 7. Full `paper_ccws_*` Field Check (vecadd, feature_on_noop)

```
paper_ccws_enabled           = 1
paper_ccws_vta_probe         = 0
paper_ccws_vta_hit           = 0
paper_ccws_lost_locality_event = 0
paper_ccws_score_update      = 0
paper_ccws_score_decay       = 0
paper_ccws_load_gate_attempt = 0
paper_ccws_load_gate_block   = 0
paper_ccws_load_gate_allow   = 0
```

All behavior counters = 0, as expected for the no-op stage.

---

## 8. Behavior Invariance Judgment

| Check | Result |
|-------|--------|
| `feature_off sim_cycle` = baseline | ✓ All 7 workloads match exactly |
| `feature_on_noop sim_cycle` = `feature_off` | ✓ All 7 workloads match exactly |
| `paper_ccws_enabled` reflects config | ✓ 0 for off, 1 for on |
| `paper_ccws_load_gate_block` = 0 | ✓ All 7 workloads both groups |
| `paper_ccws_vta_hit` = 0 | ✓ All 7 workloads both groups |
| `cacheinst_*` stats still present | ✓ Verified in all runs |
| Any sign of behavior change | **None observed** |

**Verdict**: No-op stage is safe. The CCWS feature flag can be set to 0 or 1 without any effect on
simulation results. All behavior-altering logic paths (VTA, LLS, load gating) remain unimplemented.

---

## 9. Detailed Results File

`experiments/paper-ccws/noop_behavior_check.csv`

---

## 10. Next Steps

**Do NOT proceed to full VTA/LLS/gating in Round U.**

Suggested Round U options:
1. **SWL controllable baseline**: Wire `gpgpu_ccws_enable_swl` to dynamically invoke `swl_scheduler`
   so we have a reproducible SWL comparison arm before implementing CCWS.
2. **Config automation**: Update workload manifests to point to `hrl-repro` configs for CCWS
   experiments (so `GPGPUSIM_CONFIG_OVERRIDE` is not needed manually).

When ready for Stage S5 (VTA prototype), the approach will be:
- Add miss-side VTA in `ldst_unit` / `shader.cc` using `mf->get_wid()`
- Add per-scheduler LLS array (`unsigned ccws_lls[MAX_WARPS_PER_CTA]`)
- Score decay in `scheduler_unit::cycle()`
- At that point `paper_ccws_vta_hit > 0` should appear on `page_stride_access`

**Reminder**: Any self-developed cache policy must go to `hrl/idea/*`, never into this branch.
