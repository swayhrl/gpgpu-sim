# CCWS Round V — VTA Probe Instrumentation

_Created: 2026-05-01 (Round V). Branch: `hrl/paper/ccws-repro-v0`._

---

## 1. Purpose

Implement the VTA-like miss-side probe instrumentation as passive counters.
No scheduling behavior changes; no load gating. Goals:

- Confirm L1D miss path is reachable from `ldst_unit` with warp_id and block address
- Implement miss-side VTA approximation (per-warp circular buffer of recently-missed blocks)
- Show `paper_ccws_vta_probe > 0` and `vta_hit > 0` for L1D-miss workloads
- Show `paper_ccws_load_gate_block = 0` throughout (no gating yet)
- Show `sim_cycle` unchanged vs. baseline (pure instrumentation)

---

## 2. Audit Summary (Task B)

| Question | Answer |
|----------|--------|
| Best probe location | `L1_latency_queue_cycle()` at `shader.cc` MISS branch — primary path for SM7_QV100 (l1_latency > 0) |
| Can get mem_fetch? | Yes — `mf_next` in scope at MISS branch |
| Can get wid? | Yes — `mf_next->get_wid()` |
| Can get miss status? | Yes — `status == MISS \|\| HIT_RESERVED` |
| Can get evicted tag? | No — `evicted_block_info` has no warp_id field |
| Fallback needed? | Yes — miss-side approximation (Plan B) |
| VTA table location | `ldst_unit` (per-SM, has `m_L1D`, `m_config`, `l1_latency_queue`) |

Secondary probe location: `process_cache_access()` for no-latency L1D path (fallback, not used by SM7_QV100).

---

## 3. VTA Miss-Side Approximation Design

**Semantics**: Track per-warp set of recently-missed block addresses. On L1D MISS by warp W for block B:
1. Probe VTA[W] for B → if found, `vta_hit++` (approximates "W missed B before → lost locality")
2. Insert B into VTA[W] at circular pointer position (overwrite oldest if full)

This approximates the paper's eviction-based VTA. The paper's VTA records the owning warp of an evicted line. Our approximation records the warp that missed the block. A VTA hit here means "warp W has missed this block at least twice" — a proxy for lost intra-warp locality.

**VTA table structure** (in `ldst_unit`):
- `m_ccws_vta[nwarps][vta_entries_per_warp]` — circular buffer of block addresses, init to `-1`
- `m_ccws_vta_ptr[nwarps]` — per-warp circular pointer

**Table is only allocated when**: `gpgpu_enable_ccws=1 && gpgpu_ccws_enable_vta_probe=1`.
When `gpgpu_ccws_enable_vta_probe=0`, `m_ccws_vta.empty()` → probe exits immediately.

---

## 4. New Config Knob

| Knob | Default | Meaning |
|------|---------|---------|
| `-gpgpu_ccws_enable_vta_probe` | `0` | Enable VTA miss-side probe (Round V) |

When `gpgpu_enable_ccws=0`, VTA probe does nothing regardless of `gpgpu_ccws_enable_vta_probe`.

---

## 5. Source Changes

| File | Change | Risk |
|------|--------|------|
| `src/gpgpu-sim/shader.h` | `ldst_unit`: add VTA state + counters + methods; `shader_core_config`: add `gpgpu_ccws_enable_vta_probe`; `shader_core_ctx`, `simt_core_cluster`: add `get_ccws_vta_stats()` declaration | Low |
| `src/gpgpu-sim/shader.cc` | `ldst_unit::init()`: initialize VTA; `ccws_vta_probe_miss()`: probe logic; `L1_latency_queue_cycle()`: probe at MISS; `process_cache_access()`: probe at MISS (fallback); aggregation implementations | Low |
| `src/gpgpu-sim/gpu-sim.cc` | Register `gpgpu_ccws_enable_vta_probe`; replace hardcoded-zero stats with aggregated real counters | Low |

No changes to cache replacement logic, scheduler ordering, or issue gating.

---

## 6. New Stats Output

| Stat | Meaning |
|------|---------|
| `paper_ccws_l1d_miss_seen` | Total L1D misses observed (always incremented when `gpgpu_enable_ccws=1`) |
| `paper_ccws_vta_probe` | Misses with VTA probe performed (= `l1d_miss_seen` when `vta_probe=1`) |
| `paper_ccws_vta_hit` | VTA hits (block seen before by this warp) |
| `paper_ccws_vta_insert` | VTA insertions |
| `paper_ccws_vta_overwrite` | Insertions that overwrote a valid entry (VTA full for this warp) |

---

## 7. Validation Results

### probe_off (feature_off baseline check)

All 7 quick-set workloads: `sim_cycle` = baseline, all `paper_ccws_*` = 0. ✓

### probe_on (VTA probe enabled)

| Workload | sim_cycle | l1d_miss_seen | vta_probe | vta_hit | gate_block | Δ vs baseline |
|----------|-----------|---------------|-----------|---------|------------|--------------|
| vecadd | 5569 | 96 | 96 | 72 | 0 | 0% |
| strided_access | 5825 | 224 | 224 | 24 | 0 | 0% |
| page_stride_access | 5851 | 256 | 256 | 24 | 0 | 0% |
| atomic_contention | 5414 | 0 | 0 | 0 | 0 | 0% |
| mutual_tiled | 7479 | 640 | 640 | 384 | 0 | 0% |
| polybench_2dconv | 6652 | 7860 | 7860 | 5616 | 0 | 0% |
| rodinia_hotspot | 6931 | 2576 | 2576 | 1328 | 0 | 0% |

**All pass criteria met:**
- `sim_cycle` unchanged for all 7 workloads ✓
- `paper_ccws_load_gate_block = 0` for all ✓
- `vta_probe > 0` for all workloads with L1D misses ✓
- `vta_hit > 0` for all workloads with L1D misses ✓
- `atomic_contention`: `miss_seen = 0` (correct — atomics bypass L1D read path) ✓

### VTA hit rate interpretation

| Workload | vta_hit/probe | Interpretation |
|----------|--------------|----------------|
| vecadd | 72/96 = 75% | High: repeated accesses to same lines (cold-miss then capacity miss) |
| strided_access | 24/224 = 11% | Low: stride-32 destroys locality, each warp hits mostly fresh lines |
| page_stride_access | 24/256 = 9% | Low: cross-page stride, each page freshly accessed |
| mutual_tiled | 384/640 = 60% | High: tiled matmul, shared data reuse; many repeated misses |
| polybench_2dconv | 5616/7860 = 71% | High: convolution kernel repeatedly accesses same rows |
| rodinia_hotspot | 1328/2576 = 52% | Moderate: stencil with some spatial locality within warp |

These hit rates are consistent with the expected cache access patterns.

---

## 8. Limitations

| Limitation | Detail |
|-----------|--------|
| Miss-side approximation | Not the paper's eviction-based VTA; no evicted tag owner available |
| No VTA overwrite in quick set | `vta_overwrite = 0` — workloads too small to fill 16-entry VTA |
| No LLS score | vta_hit does not yet feed into LLS; that is Stage S5 |
| No load gating | `load_gate_block = 0`; that is Stage S6 |

---

## 9. Next Steps — Stage S5 (LLS Array + Score Decay)

- Add `unsigned m_ccws_lls[max_warps]` per-scheduler
- Add `VTAHitsTotal` and `InstIssuedTotal` per-scheduler counters
- On VTA hit: update LLS for warp to LLDS = `(VTAHitsTotal/InstIssuedTotal) × K_THROTTLE × CumLLSCutoff`
- Per-cycle: decay all LLS by 1, floor at `BaseScore`
- Output `paper_ccws_score_update` and `paper_ccws_score_decay`

Stage S6: Can Issue gating (load-blocking) using LLS cutoff.
