# CCWS Round W — LLS Score Instrumentation

_Created: 2026-05-01 (Round W). Branch: `hrl/paper/ccws-repro-v0`._

---

## 1. Purpose

Implement the Lost-Locality Score (LLS) array with per-cycle decay as passive instrumentation.
No scheduling behavior changes; no load gating. Goals:

- Add per-warp LLS array in `ldst_unit` (same location as VTA state from Round V)
- On VTA hit: increment warp's LLS score (capped at `lls_max_score`)
- Per N cycles: decay all scores toward `lls_base_score` floor
- Show `paper_ccws_lls_score_update > 0` when `vta_hit > 0`
- Show `paper_ccws_load_gate_block = 0` throughout (no gating yet)
- Show `sim_cycle` unchanged vs. baseline (pure instrumentation)

---

## 2. Design

### LLS state per `ldst_unit`

| Field | Type | Purpose |
|-------|------|---------|
| `m_ccws_lls` | `std::vector<unsigned>` | Per-warp score, size = `max_warps_per_shader`, indexed by wid |
| `m_ccws_lls_score_update` | `unsigned long long` | Count of score updates (= VTA hits that triggered update) |
| `m_ccws_lls_score_decay_events` | `unsigned long long` | Count of decay sweeps across all warps |
| `m_ccws_lls_score_increment_total` | `unsigned long long` | Total score points added |
| `m_ccws_lls_score_decay_total` | `unsigned long long` | Total score points removed by decay |
| `m_ccws_lls_score_saturations` | `unsigned long long` | Times a score hit the max cap |
| `m_ccws_lls_decay_cycle_count` | `unsigned long long` | Cycle counter for decay interval |

### Update logic (`ccws_lls_update()`)

Called from `ccws_vta_probe_miss()` when `hit=true`:
- `new_s = old_s + hit_increment`; clamp to `lls_max_score`
- Accumulate `increment_total += (new_s - old_s)`, `score_update++`

### Decay logic (in `ldst_unit::cycle()`)

Called every simulation cycle, gated by `lls_decay_interval`:
- Every `lls_decay_interval` cycles: sweep all warps, decrement any warp above `lls_base_score` by `lls_decay_amount` (floored at base)
- Accumulates `decay_total`, increments `decay_events`

### Gating: NONE

`m_ccws_lls` is read-only from scheduler's perspective. No `Can Issue` bit modified.
`paper_ccws_load_gate_block = 0` throughout.

---

## 3. New Config Knobs

| Knob | Default | Meaning |
|------|---------|---------|
| `-gpgpu_ccws_enable_lls_score` | `0` | Enable LLS array + decay (Round W) |
| `-gpgpu_ccws_lls_base_score` | `100` | Floor score value (paper BaseScore) |
| `-gpgpu_ccws_lls_hit_increment` | `1` | Score increment per VTA hit |
| `-gpgpu_ccws_lls_decay_interval` | `100` | Cycles between decay sweeps |
| `-gpgpu_ccws_lls_decay_amount` | `1` | Score decrement per decay sweep |
| `-gpgpu_ccws_lls_max_score` | `1024` | Maximum score cap |

Note: `lls_hit_increment=1` and `lls_decay_interval=100` are instrumentation-only defaults,
not the LLDS formula from the paper (Stage S6 will implement that).

---

## 4. Source Changes

| File | Change | Risk |
|------|--------|------|
| `src/gpgpu-sim/shader.h` | `ldst_unit`: add LLS state vars + `ccws_lls_update()` + `get_ccws_lls_stats()`; `shader_core_config`: add 6 new knobs; `shader_core_ctx`, `simt_core_cluster`: add `get_ccws_lls_stats()` declarations | Low |
| `src/gpgpu-sim/shader.cc` | `ldst_unit::init()`: init LLS array; `ccws_vta_probe_miss()`: call `ccws_lls_update()` on hit; `ldst_unit::cycle()`: add decay; define `ccws_lls_update()`, `get_ccws_lls_stats()`, `shader_core_ctx::get_ccws_lls_stats()`, `simt_core_cluster::get_ccws_lls_stats()` | Low |
| `src/gpgpu-sim/gpu-sim.cc` | Register 6 new knobs; replace hardcoded-zero LLS stats with real aggregated counters | Low |
| `configs/hrl-repro/SM7_QV100_ccws_lls_score_on/` | New config with all LLS knobs enabled | None |

No changes to cache replacement logic, scheduler ordering, or issue gating.

---

## 5. New Stats Output

| Stat | Meaning |
|------|---------|
| `paper_ccws_lls_score_update` | Times a warp's LLS was incremented (= VTA hits with LLS on) |
| `paper_ccws_lls_decay_events` | Number of decay sweep passes |
| `paper_ccws_lls_increment_total` | Total score points added across all warps/time |
| `paper_ccws_lls_decay_total` | Total score points removed by decay |
| `paper_ccws_lls_saturations` | Times score hit `lls_max_score` cap |
| `paper_ccws_lls_nonzero_warps` | Warps with score > base at end of simulation |
| `paper_ccws_lls_max_score` | Highest score observed at end of simulation |
| `paper_ccws_lls_sum_score` | Sum of all warp scores at end (baseline = nwarps × base_score) |

---

## 6. Validation Results

### lls_off (feature_off baseline check)

All 7 quick-set workloads: `sim_cycle` = baseline, all `paper_ccws_lls_*` = 0. ✓

### lls_score_on (LLS score enabled)

| Workload | sim_cycle | vta_hit | lls_update | decay_events | nonzero_warps | max_score | gate_block | Δ vs baseline |
|----------|-----------|---------|------------|--------------|---------------|-----------|------------|--------------|
| vecadd | 5569 | 72 | 72 | 5 | 8 | 105 | 0 | 0% |
| strided_access | 5825 | 24 | 24 | 5 | 6 | 101 | 0 | 0% |
| page_stride_access | 5851 | 24 | 24 | 6 | 4 | 101 | 0 | 0% |
| atomic_contention | 5414 | 0 | 0 | 3 | 0 | 100 | 0 | 0% |
| mutual_tiled | 7479 | 384 | 384 | 96 | 0 | 100 | 0 | 0% |
| polybench_2dconv | 6652 | 5616 | 5616 | 916 | 80 | 106 | 0 | 0% |
| rodinia_hotspot | 6931 | 1328 | 1328 | 442 | 34 | 102 | 0 | 0% |

**All pass criteria met:**
- `sim_cycle` unchanged for all 7 workloads ✓
- `paper_ccws_load_gate_block = 0` for all ✓
- `lls_score_update = vta_hit` for all workloads ✓
- `atomic_contention`: `lls_update = 0` (correct — no VTA hits → no score updates) ✓
- `lls_decay_events > 0` even for atomic_contention (decay fires even without hits) ✓

---

## 7. Interpretation

### `lls_score_update = vta_hit` (exact equality)

This confirms the LLS update path is correctly triggered by every VTA hit with no
additional filtering. Expected: every lost-locality event increments a warp's score.

### `mutual_tiled`: nonzero_warps = 0, max_score = 100 (= base)

Despite 384 hits, all scores decayed back to base by end-of-simulation. With
`hit_increment=1` and `decay_interval=100/amount=1`, the decay rate (96 events) roughly
matches the total increments (384 / ~4 warps = ~96 per warp), so each warp's score
oscillates near base and ends at base. This is expected for a balanced workload.

### `polybench_2dconv`: 80 nonzero_warps, max=106

High vta_hit rate (5616) spread across 80 warps; at end, 80 warps still slightly elevated.
Increment (5616) > decay (5396) for this workload length — score didn't fully decay back.

### Score values relative to base (100)

All `max_score` values are 100–106, well below `lls_max_score=1024`. No saturation observed
on quick-set workloads (expected — tiny workloads). Saturations will appear on larger
irregular workloads with higher VTA hit rates.

---

## 8. Limitations

| Limitation | Detail |
|-----------|--------|
| No LLDS formula | Score updates use fixed `hit_increment=1`, not the dynamic LLDS formula from paper |
| No Can Issue gating | `load_gate_block = 0`; that is Stage S6 |
| Miss-side VTA approximation | Inherited from Round V; not eviction-based |
| Tiny workloads | Max score stays near base; larger workloads needed to see strong elevation |
| Score = `lls_sum_score / nonzero_warps` not directly reported | Can be computed from CSV |

---

## 9. Next Steps — Stage S6 (LLS Can Issue Gating)

- Implement LLDS formula: `LLDS = (VTAHitsTotal / InstIssuedTotal) × K_THROTTLE × CumLLSCutoff`
- Sort warps by LLS; prefix-sum to find cutoff
- Set `can_issue[wid] = 0` for warps below cutoff
- Gate `LOAD_OP` / `STORE_OP` issue in `scheduler_unit::cycle()` when `can_issue[wid] = 0`
- Output `paper_ccws_load_gate_block > 0` for HCS-like workloads
