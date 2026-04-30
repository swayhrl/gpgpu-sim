# CCWS Round U — SWL Static Baseline

_Created: 2026-05-01 (Round U). Branch: `hrl/paper/ccws-repro-v0`._

---

## 1. Purpose

Incorporate the existing `swl_scheduler` / `warp_limiting` mechanism into the CCWS reproduction
workflow as a static baseline. SWL (Static Wavefront Limiting) is the paper's non-CCWS comparison
arm: it limits the number of warps eligible for issue per cycle to a static constant k.

This round does **not** implement CCWS dynamic gating. Its goals are:
- Confirm `swl_scheduler` is configurable via `hrl-repro` config copies
- Establish SWL as a controllable comparison baseline before CCWS implementation
- Identify that tiny quick-set workloads are insufficient to show SWL limiting effects

---

## 2. Branch and Commit

| Field | Value |
|-------|-------|
| Branch | `hrl/paper/ccws-repro-v0` |
| HEAD commit | `d960c52` (docs: add CCWS no-op behavior check and hrl-repro configs) |
| Tag | `ccws-noop-behavior-check` |

---

## 3. Existing SWL Summary

| Aspect | Detail |
|--------|--------|
| Class | `swl_scheduler : public scheduler_unit` (`shader.h:619`, `shader.cc:1678`) |
| Config key | `-gpgpu_scheduler warp_limiting:<prio>:<limit>` |
| `<prio>` | Must be `2` = `SCHEDULER_PRIORITIZATION_GTO` (only GTO currently supported; assertion) |
| `<limit>` | Max warps included in `m_next_cycle_prioritized_warps` per scheduler per cycle |
| Mechanism | `order_by_priority(..., MIN(m_num_warps_to_limit, m_supervised_warps.size()), ...)` |
| Scope | Per-scheduler; SM7_QV100 has 4 schedulers, ~16 supervised warps each |
| Assertion | `assert(m_num_warps_to_limit <= max_warps_per_shader)` (64 for SM7_QV100) |

**Source code NOT modified.** Config-only change.

---

## 4. Configs Created

| Dir | `gpgpu_scheduler` | Effective limit per scheduler | Purpose |
|-----|------------------|------------------------------|---------|
| `configs/hrl-repro/SM7_QV100_ccws_swl_limit_4/` | `warp_limiting:2:4` | 4 of ~16 | Aggressive SWL (paper's aggressive arm) |
| `configs/hrl-repro/SM7_QV100_ccws_swl_limit_8/` | `warp_limiting:2:8` | 8 of ~16 | Moderate SWL (paper's sweet-spot k=8) |
| `configs/hrl-repro/SM7_QV100_ccws_swl_limit_16/` | `warp_limiting:2:16` | 16 of ~16 | No-op SWL / GTO reference |

All configs also set `-gpgpu_enable_ccws 0` (CCWS dynamic gating off).

---

## 5. Validation Runs

- **Scope**: Full quick set (7 workloads) × 3 SWL limits
- **Method**: `GPGPUSIM_CONFIG_OVERRIDE` env var (added in Round T)
- **Source compiled**: No recompilation needed (no `src/` changes)
- **Sets run**: `quick` (7 workloads)

Workloads: `vecadd`, `strided_access`, `page_stride_access`, `atomic_contention`,
`mutual_tiled`, `polybench_2dconv`, `rodinia_hotspot`

---

## 6. Results

### Pass/fail summary

| Limit | Explicit PASS | Completed | Failed |
|-------|--------------|-----------|--------|
| 4 | 5 | 2 | **0** |
| 8 | 5 | 2 | **0** |
| 16 | 5 | 2 | **0** |

### `paper_ccws_*` — all zero

All `paper_ccws_*` counters (vta_hit, load_gate_block, lost_locality, etc.) = 0 for all runs.
`paper_ccws_enabled = 0` for all runs. `cacheinst_*` stats still present. ✓

### sim_cycle comparison

All three SWL limits produce **identical results** to each other.

| Workload | LRR baseline | SWL (all limits) | Δ |
|----------|-------------|-------------------|---|
| vecadd | 5569 | 5558 | −0.2% |
| strided_access | 5825 | 5824 | ~0% |
| page_stride_access | 5851 | 5850 | ~0% |
| atomic_contention | 5414 | 5407 | −0.1% |
| mutual_tiled | 7479 | 7451 | −0.4% |
| polybench_2dconv | 6652 | 6660 | +0.1% |
| rodinia_hotspot | 6931 | 6917 | −0.2% |

---

## 7. Interpretation

### Why all three limits produce identical results

SM7_QV100 has 4 schedulers per SM, each supervising ~16 warps. The quick-set workloads are
**tiny** (e.g., `vecadd` runs 1 CTA, `strided_access` runs 4 CTAs or fewer). With only 1–4
active warps per scheduler, even `warp_limiting:2:4` has **no constraining effect** — there are
fewer supervised warps than the limit.

The small differences vs. the LRR baseline (~0.1–0.4%) come entirely from the scheduler policy
change (LRR → GTO), not from warp limiting.

This is the **"tiny workloads" limitation** already documented in `ccws_repro_plan.md` §13.

### SWL vs. CCWS

SWL is a **static, all-instruction** limit. It always limits all warps' access to the issue queue,
regardless of L1D locality. CCWS is a **dynamic, load-only** gate — it selectively blocks warps
that have shown lost intra-warp L1D locality (high LLS). On large HCS workloads, CCWS provides
adaptive limiting that outperforms fixed SWL by targeting only the right warps at the right time.

---

## 8. Limitations

| Limitation | Detail |
|-----------|--------|
| Tiny workloads | Quick-set workloads have too few active warps for SWL limiting to have any effect |
| No `cache_focus` | Workloads like `rodinia_srad_v2`, `parboil_spmv` need larger inputs to show SWL effects |
| GTO-only SWL | `swl_scheduler` only supports GTO prioritization; LRR SWL is not available |
| No `irregular_focus` | The paper's HCS benchmark suite is not directly replicated here |
| No CCWS | Dynamic LLS/VTA/load-gating not yet implemented |

---

## 9. Next Steps

**Recommended Round V: extend workload set before Stage S5 VTA**

Option A (preferred): Run SWL against `cache_focus` set (larger workloads) to verify that
SWL limiting has a visible effect. This establishes the SWL baseline before CCWS is compared.

Option B: Proceed directly to Stage S5 (VTA prototype + LLS array + score decay).

**Reminder**: Any self-developed cache policy must go to `hrl/idea/*`, never into this branch.
