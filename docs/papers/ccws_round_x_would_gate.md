# CCWS Round X — Would-Gate Telemetry

_Created: 2026-05-01 (Round X). Branch: `hrl/paper/ccws-repro-v0`._

---

## 1. Purpose

Implement the CCWS Can-Issue / would-gate calculation as passive telemetry.
No scheduling behavior changes; no actual load blocking. Goals:

- Based on existing per-warp LLS scores (from Round W), compute which warps "would" be gated
- Record `paper_ccws_would_gate_attempt`, `paper_ccws_would_gate_block`, `paper_ccws_would_gate_allow`
- Show `paper_ccws_load_gate_block = 0` throughout (no actual gating)
- Show `paper_ccws_would_gate_block > 0` for at least one L1D-miss workload
- Show `sim_cycle` unchanged vs. Round W baseline (pure instrumentation)

---

## 2. Would-Gate Computation Design

### Algorithm (faithful to CCWS paper Layer 3)

Each cycle, after LLS decay in `ldst_unit::cycle()`:

1. Collect all per-warp LLS scores into `(score, wid)` vector
2. Sort descending by score (O(n log n), where n = `max_warps_per_shader`)
3. Compute prefix sum of sorted scores
4. Mark `would_can_issue[wid] = (prefix_sum <= CumLLSCutoff)` for each warp in order
5. `CumLLSCutoff = max_warps_per_shader × base_score`

### Would-gate check (in `scheduler_unit::cycle()`)

When processing a `LOAD_OP` or `TENSOR_CORE_LOAD_OP`:
- Call `m_shader->m_ldst_unit->ccws_wg_check_load(warp_id)`
- This looks up `m_ccws_would_can_issue[warp_id]` and increments `wg_attempt`, `wg_block`, or `wg_allow`
- **Does NOT modify `issue_warp()` call** — the load still proceeds unconditionally

### Invariants maintained

- `paper_ccws_load_gate_attempt = 0` (hardcoded — no real gating path)
- `paper_ccws_load_gate_block = 0` (hardcoded — no real gating)
- `sim_cycle` unchanged vs baseline

---

## 3. New Config Knobs

| Knob | Default | Meaning |
|------|---------|---------|
| `-gpgpu_ccws_enable_would_gate` | `0` | Enable would-gate telemetry computation |
| `-gpgpu_ccws_wg_k_throttle` | `8.0` | K_THROTTLE (informational; not used in simple cutoff) |
| `-gpgpu_ccws_wg_debug` | `0` | Per-cycle debug trace (reserved) |

---

## 4. New Stats Output

| Stat | Meaning |
|------|---------|
| `paper_ccws_would_gate_attempt` | LOAD_OPs where would-gate was checked |
| `paper_ccws_would_gate_block` | Load attempts that would be blocked if gating were active |
| `paper_ccws_would_gate_allow` | Load attempts that would be allowed |

---

## 5. Source Changes

| File | Change | Risk |
|------|--------|------|
| `src/gpgpu-sim/shader.h` | `ldst_unit`: add `m_ccws_would_can_issue` vector + 3 wg counters; add `ccws_wg_check_load()`, `get_ccws_wg_stats()`, `ccws_would_can_issue()` to public; 3 new config knobs; `get_ccws_wg_stats()` to `shader_core_ctx` and `simt_core_cluster` | Low |
| `src/gpgpu-sim/shader.cc` | `ldst_unit::init()`: init wg state; `ldst_unit::cycle()`: compute `would_can_issue` after decay; `ccws_wg_check_load()` definition; `get_ccws_wg_stats()` definitions; `scheduler_unit::cycle()`: add 6-line would-gate telemetry call (no actual gating); aggregation methods | Low |
| `src/gpgpu-sim/gpu-sim.cc` | Register 3 new knobs; print `paper_ccws_would_gate_*` stats block | Low |

No changes to `issue_warp()`, cache replacement, or scheduling policy.

---

## 6. Validation Results

### wg_off (feature_off baseline check)

All 7 quick-set workloads: `sim_cycle` = baseline, all `paper_ccws_*` = 0. ✓

### wg_on (would-gate telemetry enabled)

| Workload | sim_cycle | vta_hit | lls_update | load_gate_block | wg_attempt | wg_block | wg_allow | Δ vs baseline |
|----------|-----------|---------|------------|-----------------|------------|----------|----------|--------------|
| vecadd | 5569 | 72 | 72 | 0 | 105 | 0 | 105 | 0% |
| strided_access | 5825 | 24 | 24 | 0 | 97 | 0 | 97 | 0% |
| page_stride_access | 5851 | 24 | 24 | 0 | 97 | 0 | 97 | 0% |
| atomic_contention | 5414 | 0 | 0 | 0 | 50 | 0 | 50 | 0% |
| mutual_tiled | 7479 | 384 | 384 | 0 | 2552 | 0 | 2552 | 0% |
| polybench_2dconv | 6652 | 5616 | 5616 | 0 | 10232 | 0 | 10232 | 0% |
| rodinia_hotspot | 6931 | 1328 | 1328 | 0 | 16415 | **2** | 16413 | 0% |

**All pass criteria met:**
- `sim_cycle` unchanged for all 7 workloads ✓
- `paper_ccws_load_gate_block = 0` for all ✓ (no actual gating)
- `paper_ccws_would_gate_attempt > 0` for all workloads ✓ (LOAD_OPs counted)
- `paper_ccws_would_gate_block > 0` for `rodinia_hotspot` ✓ (mechanism triggers)
- VTA/LLS stats unchanged vs Round W ✓

---

## 7. Interpretation

### Why `would_gate_block` is small on quick-set workloads

With `lls_hit_increment=1` and `lls_decay_interval=100`, the elevated scores stay close to
`base_score=100` (max observed: 105–106). The sorting + prefix-sum algorithm blocks only the
last few warps in sorted order when elevated warps push the prefix sum over `CumLLSCutoff`.

For tiny workloads (few active warps, short simulation), this boundary is crossed rarely.
`rodinia_hotspot` crosses it twice (`wg_block=2`), confirming the mechanism is functional.

On larger HCS workloads (e.g., `cache_focus / irregular_focus` set), where more warps have
higher VTA hit rates and elevated scores, `wg_block` should be significantly higher.

### `atomic_contention`: `wg_attempt = 50`

The 50 LOAD_OP attempts come from scalar loads to initialize/read the atomic operation's address
before the actual `atomicAdd`. The VTA probe correctly shows `vta_hit = 0` (atomics bypass L1D
read path), but scalar loads leading up to the atomic are counted.

### Algorithm faithfulness

The `CumLLSCutoff = nw × BaseScore` formula is faithful to the paper's Layer 3. The current
implementation uses simple score increment (`hit_increment=1`) rather than the dynamic LLDS
formula (`(VTAHitsTotal/InstIssuedTotal) × K_THROTTLE × CumLLSCutoff`). The full LLDS formula
would require tracking per-scheduler instruction counts; this simplification gives valid telemetry
for the would-gate mechanism.

---

## 8. Limitations

| Limitation | Detail |
|-----------|--------|
| No dynamic LLDS formula | Uses fixed `hit_increment=1`; LLDS should be computed dynamically |
| Per-SM, not per-scheduler | LLS state is SM-wide; paper has per-scheduler state |
| Small wg_block on quick-set | Tiny workloads → scores barely elevated → rarely cross cutoff |
| No actual gating | `load_gate_block = 0`; that is Stage S6 (Round Y) |

---

## 9. Next Steps — Stage S6 / Round Y (Real Can-Issue Gating)

- Wire `would_can_issue[wid] == false` into actual load issue decision
- Change `if ((pI->op == LOAD_OP) || ...)` to gate on `can_issue`
- Count `paper_ccws_load_gate_block > 0` for HCS-like workloads
- Verify `sim_cycle` decreases for HCS workloads when gating active
