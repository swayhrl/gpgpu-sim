# CCWS Reproduction Plan

_Created: 2026-04-30 (Round R). Updated: 2026-05-01 (Round U — SWL static baseline complete)._  
_Template: docs/paper_repro_template.md_

---

## 1. Paper Metadata

| Field | Value |
|-------|-------|
| **Paper key** | `ccws` |
| **Title** | Cache-Conscious Wavefront Scheduling |
| **Authors** | Timothy G. Rogers, Mike O'Connor, Tor M. Aamodt |
| **Venue / Year** | MICRO 2012 |
| **Link / DOI** | DOI: 10.1109/MICRO.2012.16 |
| **Target mechanism category** | Cache-conscious warp/wavefront scheduling (load-issue gating) |
| **Expected GPGPU-Sim modules** | `shader.h`, `shader.cc`, `gpu-cache.h`, `gpu-sim.cc` |

---

## 2. One-sentence Summary

CCWS dynamically limits which warps may issue load instructions by tracking lost intra-warp L1D locality via a per-warp victim tag array, assigning locality scores, and gating low-score warps from issuing loads.

---

## 3. Problem and Motivation

- **Problem**: On GPU cores with many concurrent active warps, the aggregate memory footprint of all warps exceeds L1D capacity, causing thrashing and destroying intra-warp locality.
- **Root cause**: The wavefront issue arbiter (scheduler) determines which memory accesses enter the L1D. Standard schedulers (LRR, GTO) are cache-oblivious and can interleave accesses from many warps, evicting data before the issuing warp can re-use it.
- **Evidence metric**: MPKI (misses per thousand instructions) and normalized IPC on HCS benchmarks. Figure 3 shows that peak IPC occurs at 4–7 active warps, not max concurrency (32).
- **Key insight**: Even Belady-optimal replacement fails to recover from a bad scheduler. Scheduling policy dominates replacement policy for HCS workloads.

---

## 4. Mechanism Summary

CCWS operates in three layers:

**Layer 1 — VTA (Victim Tag Array) + LLD**: When a warp W misses in L1D and reserves a cache line, W's warp ID is stored with the line. When that line is later evicted (by another warp's miss), its tag is written to VTA[W]. On W's next L1D miss, VTA[W] is probed. A VTA hit signals "lost locality"—W could have hit if it had more exclusive L1D access.

**Layer 2 — LSS (Locality Scoring System)**: Each warp has a lost-locality score (LLS). LLS starts at `BaseScore` (100), jumps to LLDS on a VTA hit (capped at LLDS), and decays by 1/cycle back to `BaseScore`. LLDS is computed as:
```
LLDS = (VTAHitsTotal / InstIssuedTotal) × K_THROTTLE × CumLLSCutoff
CumLLSCutoff = NumActiveWarps × BaseScore
```
K_THROTTLE = 8 is a single static constant that works well across all workloads.

**Layer 3 — Can Issue gating**: Warps are sorted by LLS (max-heap). The prefix sum is computed. Warps whose LLS falls below the cutoff (`CumLLSCutoff`) in the sorted order have their "Can Issue" bit cleared, preventing them from issuing **load instructions** (non-load instructions are unaffected).

---

## 5. Mapping to GPGPU-Sim

| Paper concept | GPGPU-Sim candidate | Files / functions | Confidence | Notes |
|---|---|---|---|---|
| Wavefront Issue Arbiter (WIA) | `scheduler_unit::cycle()` | `shader.cc:1259` | **High** | Main scheduling loop; iterates `m_next_cycle_prioritized_warps` |
| Baseline priority logic | `lrr_scheduler`, `gto_scheduler`, `swl_scheduler` | `shader.h:492–636`, `shader.cc:1592–1710` | **High** | Three subclasses of `scheduler_unit`; SM7_QV100 uses `lrr` |
| SWL (Static Wavefront Limiting) | `swl_scheduler` | `shader.h:619`, `shader.cc:1678` | **High** | **Already implemented!** Config: `warp_limiting:<prio>:<limit>`. Currently only GTO prioritization. Audit before reimplementing. |
| Can Issue bit vector | New per-scheduler gate | `shader.cc:1344` (before `LOAD_OP` issue) | **High** | `warp_id` already available as local var; gate by `ccws_can_issue[warp_id]` |
| Load instruction detection | `pI->op == LOAD_OP` | `shader.cc:1344` | **High** | Condition already exists; also `TENSOR_CORE_LOAD_OP` |
| Warp ID at scheduler | `warp_id` local variable | `scheduler_unit::cycle()`, `shader.cc:1280` | **High** | Available as `unsigned warp_id = (*iter)->get_warp_id()` |
| L1D miss feedback | `cache_request_status::MISS` + `mf->get_wid()` | `gpu-cache.h:52`, `mem_fetch.h:98` | **Medium** | mf carries wid; routing from ldst_unit back to scheduler needs plumbing |
| Cache line eviction feedback | `evicted_block_info` | `gpu-cache.h:82` | **Medium-Low** | **`evicted_block_info` does NOT currently store warp_id.** Faithful VTA requires adding `m_warp_id` to `cache_block_t` and `evicted_block_info`. This is the highest-risk change. |
| VTA per-warp tag store | New `ccws_vta_t` struct | `shader.h` (new per-scheduler member) | **Low-Medium** | Novel data structure; can be simplified to miss-side tracking first |
| LLS score array per warp | New `unsigned ccws_lls[MAX_WARPS]` | `shader.h` (new per-scheduler member) | **Medium** | Per-scheduler state, indexed by warp_id |
| Score decay per cycle | New logic in `scheduler_unit::cycle()` | `shader.cc:1259` | **Medium** | Decrement all LLS by 1 per cycle call; bounded at `BaseScore` |
| LLDS computation | New function in scheduler | `shader.cc` (new method) | **Medium** | Requires tracking `VTAHitsTotal` and `InstIssuedTotal` counters |
| Cumulative LLS cutoff test | New logic in `scheduler_unit::cycle()` | `shader.cc:1344` | **Medium** | Sort LLS, prefix sum, compute cutoff; can be pipelined |
| Config knob parsing | `option_parser_register()` | `gpu-sim.cc` or `gpgpu-sim.cc` | **High** | Standard GPGPU-Sim pattern |
| Stats printing | Existing `print_stats()` | `gpu-sim.cc` or `shader.cc` | **High** | Add `paper_ccws_*` counters to existing stats infrastructure |
| Cache instrumentation baseline | `cacheinst_*` counters | `gpu-cache.h:1261` | **High** | Round O already added passive instrumentation; use as baseline |

### Key confirmed facts from source reading

- `swl_scheduler` already exists at `shader.cc:1678`; config format: `warp_limiting:<prioritization>:<num_warps_to_limit>`; currently only GTO prioritization.
- Load/store issue branch in scheduler at `shader.cc:1344–1358`; CCWS gate would be an additional `&&` condition before `m_shader->issue_warp(...)` at `shader.cc:1352`.
- `evicted_block_info` (gpu-cache.h:82) fields: `m_block_addr`, `m_modified_size`, `m_byte_mask`, `m_sector_mask`. **No warp_id field.** Must add for faithful VTA.
- `mf->get_wid()` is available from `mem_fetch.h:98` — warp_id accessible at miss time.
- SM7_QV100 current scheduler: `lrr` (config line 134).

---

## 6. Implementation Scope

### A. Must implement for minimal CCWS

- [x] Config flag `gpgpu_enable_ccws` default `0` — no behavior change when off
- [x] Audit and document existing `swl_scheduler` / `warp_limiting` (Stage S1) → `ccws_swl_audit.md`
- [x] No-op CCWS feature flag: compiles, `feature_off` passes quick set (Stage S2/S3) → `ccws-config-noop` tag
- [x] `paper_ccws_*` instrumentation stats (Stage S4) — no-op zero output confirmed
- [x] VTA miss-side probe (Stage V) — `vta_probe/hit > 0` for L1D-miss workloads; no behavior change

### B. Nice to have

- [ ] Faithful VTA with eviction-side warp_id (requires `cache_block_t` modification)
- [ ] K_THROTTLE parameter sweep across HCS-like workloads
- [ ] SWL sweep comparison with CCWS
- [ ] Two-level scheduler comparison (2LVL-GTO)
- [ ] Per-workload config matrix in `experiments/paper-ccws/`
- [ ] Area estimate note (0.17% from paper — no simulation needed)

### C. Out of scope for first implementation

- Exact GPGPU-Sim 3.1.0 baseline reproduction
- Exact original benchmark suite (BFS GPU / MEMC / GC — not in workload repo)
- Belady-optimal replacement comparison
- Hardware area modeling
- Power-tuned CCWS configuration
- Changing cache replacement policy
- Any self-developed improvements to CCWS (→ `hrl/idea/*` branch only)

---

## 7. Config Knobs

| Parameter | Type | Default | Off behavior | On behavior | Suggested values |
|-----------|------|---------|-------------|-------------|-----------------|
| `gpgpu_enable_ccws` | `int` | `0` | No CCWS; simulation identical to baseline | Enable full CCWS | `0` or `1` |
| `gpgpu_ccws_enable_swl` | `int` | `0` | No SWL limit | Enable static warp limit | `0` or `1` |
| `gpgpu_ccws_swl_limit` | `int` | `32` | N/A | Limit active warps per scheduler to this value | `4`–`32` |
| `gpgpu_ccws_base_locality_score` | `int` | `100` | N/A | Base LLS value; also controls score floor | `100` (paper default) |
| `gpgpu_ccws_k_throttle` | `float` | `8.0` | N/A | LLDS multiplier; larger = more throttling | `8.0` (paper default) |
| `gpgpu_ccws_vta_entries_per_warp` | `int` | `16` | N/A | VTA size per warp | `16` (paper default) |
| `gpgpu_ccws_score_decay_interval` | `int` | `1` | N/A | Cycles between score decay steps | `1` (per-cycle decay) |
| `gpgpu_ccws_gate_loads_only` | `int` | `1` | N/A | If 0, gate all mem ops (not recommended) | `1` |
| `gpgpu_ccws_debug` | `int` | `0` | N/A | Print per-cycle CCWS decision trace | `0` |

**Rule**: All default to off or to paper-recommended values. `gpgpu_enable_ccws=0` must produce identical results to `cache-inst-v0` baseline.

---

## 8. Instrumentation Plan

All stats gated by `gpgpu_enable_ccws`. Prefix: `paper_ccws_`.

| Stat | Description | Expected value |
|------|-------------|----------------|
| `paper_ccws_enabled` | Binary: was CCWS enabled for this run | 0 or 1 |
| `paper_ccws_vta_probe` | Times VTA was probed on L1D miss | High for HCS; ~0 for CI |
| `paper_ccws_vta_hit` | VTA hits (lost locality events) | High for HCS; ~0 for CI |
| `paper_ccws_lost_locality_event` | VTA hits that triggered score update | = vta_hit |
| `paper_ccws_score_update` | Times any warp's LLS was increased | = lost_locality_event |
| `paper_ccws_score_decay` | Total score decrements across all warps | Proportional to cycles × warps |
| `paper_ccws_load_gate_attempt` | Times a warp tried to issue a load | All load instructions |
| `paper_ccws_load_gate_block` | Times load was blocked by Can Issue = 0 | High for HCS with CCWS on |
| `paper_ccws_load_gate_allow` | Times load was allowed | = attempt - block |
| `paper_ccws_active_warp_limit` | Effective avg active warps per scheduler | Should decrease for HCS |
| `paper_ccws_avg_allowed_warps` | Mean warps with Can Issue = 1 | |
| `paper_ccws_total_blocked_loads` | Cumulative blocked load instructions | |

---

## 9. Behavior Change Plan

### Stage breakdown

| Stage | Description | Branch | Status | Expected risk |
|-------|-------------|--------|--------|--------------|
| **S0** | Repro plan only | `hrl/repro-infra-v0` | ✓ Done (Round R) | None |
| **S1** | Audit `swl_scheduler` / `warp_limiting`; document behavior | `hrl/paper/ccws-repro-v0` | ✓ Done (Round S) | None |
| **S2** | Add config knobs (`option_parser_register`); no-op feature flag; compile | `hrl/paper/ccws-repro-v0` | ✓ Done (Round S) | Low |
| **S3** | Confirm `feature_off` quick set pass (≈ baseline) | `hrl/paper/ccws-repro-v0` | ✓ Done (Round S) | Low |
| **S4** | Add `paper_ccws_*` instrumentation counters (no behavior change) | `hrl/paper/ccws-repro-v0` | ✓ Done (Round S) | Low |
| **T** | No-op behavior check: off/on_noop both ≈ baseline; config override automation | `hrl/paper/ccws-repro-v0` | ✓ Done (Round T) | Low |
| **U** | SWL static baseline: create limit_4/8/16 hrl-repro configs; quick set pass | `hrl/paper/ccws-repro-v0` | ✓ Done (Round U) | Low |
| **V** | VTA probe instrumentation-only: per-warp miss-side probe; vta_probe/hit > 0; no gating | `hrl/paper/ccws-repro-v0` | ✓ Done (Round V) | Low |
| **S5** | LLS array + score decay per-scheduler; vta_hit feeds score update | `hrl/paper/ccws-repro-v0` | ✓ Done (Round W) | Medium |
| **X** | Would-gate telemetry: sort+prefix-sum; `would_gate_block > 0`; no real gating | `hrl/paper/ccws-repro-v0` | ✓ Done (Round X) | Low |
| **S6** | LSS Can Issue gating in `scheduler_unit::cycle()` for LOAD_OP | `hrl/paper/ccws-repro-v0` | ✓ Done (Round Y) | High |
| **S7** | Quick set validation: `feature_off ≈ baseline`, `feature_on` triggers | `hrl/paper/ccws-repro-v0` | ✓ Done (Round Y) | Medium |
| **Z** | Post-gating validation: 7 workloads × 3 thresholds; signal analysis | `hrl/paper/ccws-repro-v0` | ✓ Done (Round Z) | Low |
| **AA** | Independent `lg_score_threshold` knob; decouple from `lls_base_score`; tiny validation | `hrl/paper/ccws-repro-v0` | ✓ Done (Round AA) | Low |
| **AB** | Focused threshold validation: 7 workloads × th99/100/101; signal analysis | `hrl/paper/ccws-repro-v0` | ✓ Done (Round AB) | Low |
| **AC** | LLS hit-increment sensitivity: inc1/10/50 × 7 workloads; find working range | `hrl/paper/ccws-repro-v0` | ✓ Done (Round AC) | Low |
| **S8** | Standard / `cache_focus` set validation | `hrl/paper/ccws-repro-v0` | — | Low |
| **S9** | K_THROTTLE sweep; result notes | `hrl/paper/ccws-repro-v0` | — | Low |

**Round T note**: `GPGPUSIM_CONFIG_OVERRIDE` env var added to `run_one.sh`; config override confirmed
working. `feature_on_noop` ≡ `feature_off` across all 7 quick-set workloads.

**Round U note**: SWL configs created (`limit_4/8/16`). All three produce **identical results** to
each other on quick-set workloads because tiny workloads have fewer active warps than even
`limit=4`. Difference vs LRR baseline (0.1–0.4%) is from GTO vs LRR scheduling, not warp limiting.
Key finding: quick-set workloads are insufficient to show SWL discriminating effect; larger
workloads (cache_focus / irregular_focus) are needed before CCWS comparison is meaningful.

**Round V note**: VTA miss-side probe implemented. `evicted_block_info` confirmed to have no
warp_id field; miss-side approximation used (per-warp circular buffer, probe/insert on each L1D
miss). Primary probe point: `L1_latency_queue_cycle()` MISS branch (`mf_next->get_wid()` +
`block_addr()`). New knob: `gpgpu_ccws_enable_vta_probe` (default 0). Validated on all 7
quick-set workloads: `sim_cycle` unchanged, `load_gate_block=0`, `vta_probe/hit > 0` for all
L1D-miss workloads, `atomic_contention` correctly shows 0. VTA hit rates: 9–75% consistent with
expected access patterns. No LLS/score/gating yet.

**Round W note**: LLS array + per-cycle score decay implemented in `ldst_unit` (instrumentation-only).
6 new config knobs (`gpgpu_ccws_enable_lls_score`, `lls_base_score`, `lls_hit_increment`,
`lls_decay_interval`, `lls_decay_amount`, `lls_max_score`). VTA hit → `ccws_lls_update(wid)`;
decay sweep in `ldst_unit::cycle()`. Validated on all 7 quick-set workloads: `sim_cycle` unchanged,
`lls_score_update = vta_hit` (exact), `load_gate_block = 0`. `atomic_contention` shows `lls_update=0`
(correct). `mutual_tiled` shows `nonzero_warps=0` at end (decay balanced hits). No Can Issue gating yet.

**Round X note**: Would-gate telemetry implemented (instrumentation-only). Sort+prefix-sum algorithm
computes `m_ccws_would_can_issue[]` per cycle in `ldst_unit`. Scheduler calls `ccws_wg_check_load()`
on each LOAD_OP attempt (no actual gating). 3 new knobs (`enable_would_gate`, `wg_k_throttle`,
`wg_debug`). Validated on 7 quick-set workloads: `sim_cycle` unchanged, `load_gate_block=0`,
`would_gate_attempt > 0` for all workloads, `would_gate_block = 2` for `rodinia_hotspot`.
Mechanism confirmed functional; small block count expected for tiny workloads with low LLS elevation.

**Round Y note**: Real load-only gating implemented (Stage S6+S7). `ccws_lg_gate_load(wid)` queries
`m_ccws_would_can_issue[wid]` and blocks issue if false. 2 new knobs (`enable_load_gating`,
`load_gate_debug`). New config `SM7_QV100_ccws_load_gate_on`. Validated on 7 quick-set workloads:
feature_off 7/7 pass (all counters=0, cycle=baseline). load_gate_on 7/7 pass: `rodinia_hotspot`
shows `lg_block=5` (real gating active), `lg_block=wg_block` (gate consistent with telemetry).
Other workloads: `lg_block=0` (quick-set too small; standard set expected to show more blocking).
Only LOAD_OP / TENSOR_CORE_LOAD_OP gated; STORE / MEMORY_BARRIER / compute unaffected.

**Round Z note**: Post-gating validation on 7 workloads × 3 threshold configs. Only `rodinia_hotspot`
shows `lg_block=5` across all thresholds. Key finding: `base_score` is NOT an independent threshold —
it scales both LLS initial value and cutoff proportionally, so behavior is invariant to base_score
changes. High-vta_hit workloads (srad_v2=3924, fdtd2d=8338) show no gating because hits are spread
across many warps/cycles; no single warp accumulates enough to push prefix sum over cutoff. To enable
effective threshold sweep, a separate `lls_gate_threshold` knob (decoupled from base_score) is needed.
sim_cycle unchanged for all workloads (5 gate blocks too few to affect timing).

**Round AA note**: New knob `gpgpu_ccws_lg_score_threshold` (default 100) decouples gating cutoff
from `lls_base_score`. `cum_cutoff = nw * lg_score_threshold`. Threshold sweep now effective:
th99/100 → hotspot lg_block=5; th101/200 → 0 blocks. Warning: threshold < lls_base_score causes
deadlock (all warps gated at init). Valid range: `lg_score_threshold >= lls_base_score`.

**Round AB note**: Focused threshold validation on 7 workloads × th99/100/101. Only `rodinia_hotspot`
shows gating (lg_block=5 for th99/100, 0 for th101). Threshold trend monotone and correct. Signal
weak because tiny workloads have few active warps and `lls_hit_increment=1` causes slow score
accumulation. Recommendation before standard validation: increase `lls_hit_increment` (e.g. 10–50)
to amplify LLS score differentiation. Current mechanism is functionally correct.

**Round AC note**: LLS hit-increment sensitivity on 7 workloads × inc1/10/50 (th100) + inc10 (th101).
inc=1: only hotspot 5 blocks (weak). inc=10: 0 blocks all workloads (timing issue with tiny kernels).
inc=50: srad_v2 +45% cycle, fdtd2d +61% cycle (over-gating). Recommended next: try inc=5 or inc=20
for moderate signal before standard validation. inc=50 is too aggressive for tiny workloads.

**Rules**:
- Feature flag **always default 0**.
- Never commit with CCWS behavior-altering code while `gpgpu_enable_ccws` defaults to 1.
- `baseline ≈ feature_off` must hold before proceeding to `feature_on` testing.

---

## 10. Validation Plan

| Group | Config | Workload set | Pass criteria |
|-------|--------|-------------|---------------|
| **baseline** | `cache-inst-v0` tag | quick | Reference numbers established |
| **feature_off** | `ccws` branch, `gpgpu_enable_ccws=0` | quick | sim_cycle ≈ baseline ±1%; `paper_ccws_load_gate_block = 0` |
| **feature_on** | `ccws` branch, `gpgpu_enable_ccws=1` | quick | `paper_ccws_vta_hit > 0` for HCS-like; mechanism triggers |
| **feature_on** | `ccws` branch, `gpgpu_enable_ccws=1` | cache_focus | L1D miss rate decreases for HCS-like workloads |
| **feature_on** | `ccws` branch, `gpgpu_enable_ccws=1` | irregular_focus | Mechanism triggers; no significant regression |
| **feature_on** | `ccws` branch, `gpgpu_enable_ccws=1` | standard | No significant regression on CI workloads |

**Validation order**: compile → quick feature_off → quick feature_on → cache_focus → irregular_focus → standard.

---

## 11. Workload Mapping

### HCS-like (expected to benefit from CCWS)

| Our workload | Analogue in paper | Rationale |
|---|---|---|
| `rodinia_srad_v2` | SRAD (CI in paper, but high L1D miss in our setup) | L1D miss rate 0.79, cache-sensitive stencil |
| `page_stride_access` | BFS (synthetic irregular) | Cross-page irregular access, maximum TLB+cache footprint |
| `strided_access` | — (synthetic) | Coalescing degradation + L2 miss spike |
| `rodinia_bfs` (if added) | BFS | Direct analogue |
| `parboil_spmv` | SpMV-like irregular | Irregular row access pattern |
| `irregular_focus` set overall | HCS in paper | Our irregular_focus set targets similar workloads |

### CI-like / control (should not regress)

| Our workload | Analogue in paper | Rationale |
|---|---|---|
| `mutual_tiled` | LUD | Dense tiled compute; low L1D miss |
| `polybench_gemm` | — | Dense GEMM; L2 miss 0, compute-bound |
| `rodinia_lud` | LUD | Direct analogue |
| `rodinia_backprop` (if added) | BACKP | CI in paper |
| `rodinia_hotspot` | — | Stencil with high L1D miss but regular access |

### Quick sanity set (smoke regression)

`vecadd`, `strided_access`, `page_stride_access`, `atomic_contention`, `mutual_tiled`, `polybench_2dconv`, `rodinia_hotspot`.

---

## 12. Expected Outcomes

| Metric | feature_off | feature_on (HCS-like) | feature_on (CI) |
|--------|-------------|----------------------|-----------------|
| `sim_cycle` vs baseline | ≈ same (±1%) | Decrease for HCS-like | ≈ same |
| `cacheinst_L1D_miss_rate` | ≈ baseline | Decrease expected | ≈ same |
| `W0_Scoreboard` | ≈ baseline | May decrease | ≈ same |
| `paper_ccws_vta_hit` | 0 | > 0 for HCS-like; ~0 for CI | ~0 |
| `paper_ccws_load_gate_block` | 0 | > 0 for HCS-like | ~0 |

**Caveat**: Our results will not match paper values because: GPGPU-Sim version differs (4.2.0 vs 3.1.0), GPU config differs (SM7_QV100 Volta vs paper's GPGPU-Sim default), workloads differ (no MEMC/GC/BFS-large), and input sizes are tiny (to keep sim time tractable).

Goal: Reproduce **mechanism behavior trend** — not exact numbers.

---

## 13. Risks and Known Gaps

| Risk | Severity | Mitigation |
|------|---------|------------|
| `evicted_block_info` has no warp_id | **High** | Stage 1: use miss-side VTA (approximation); Stage 2: add warp_id to `cache_block_t` |
| Load gating affects scheduler/scoreboard interaction | **High** | Test feature_off strictly; add assert that blocked load is re-tried next cycle |
| `swl_scheduler` already exists — risk of conflict or confusion | **Medium** | Audit in S1 before writing new SWL code; use existing class if compatible |
| GPGPU-Sim 4.2.0 vs 3.1.0 scheduler interface differences | **Medium** | Focus on behavioral equivalence, not code-level equivalence |
| Tiny workloads may not show CCWS effect | **Medium** | Use `irregular_focus` set; may need larger BFS/SpMV inputs |
| SM7_QV100 uses `lrr` baseline, not `gto` | **Low** | CCWS paper uses GTO baseline; CCWS is compatible with any base scheduler |
| Multiple schedulers per core (SM7_QV100 has 2) | **Low** | CCWS state should be per-scheduler, not per-core; verify in S1 |
| LLDS formula requires per-core instruction counter | **Low** | `InstIssuedTotal` can be maintained in `scheduler_unit` |
| Self-developed cache policy mixed into paper branch | **Low** | Policy: never mix; self-developed ideas go to `hrl/idea/*` |

---

## 14. Commit / Tag Milestones

| Milestone | Tag | Content |
|-----------|-----|---------|
| Plan complete | `ccws-plan-v0` | This file + reading notes, no code change |
| SWL audit done | `ccws-swl-audit` | Docs/notes on existing `swl_scheduler` behavior |
| Config knobs + no-op flag | `ccws-config-noop` | Config parsing only; compile pass; feature_off = baseline |
| Instrumentation only | `ccws-instrumentation-only` | Stats counters, no behavior; feature_off = baseline |
| VTA + LLD prototype | `ccws-vta-lld-prototype` | VTA and LLD; vta_hit > 0 for HCS |
| Load gating prototype | `ccws-load-gating-prototype` | Can Issue gating; feature_on alters scheduling |
| Quick set pass | `ccws-quick-pass` | feature_off ≈ baseline; feature_on triggers |
| Standard set result | `ccws-standard-pass` | Standard + cache_focus results documented |

---

## 15. Round S Recommendation

**Do not attempt full CCWS in Round S.**

Round S plan:
1. Create branch `hrl/paper/ccws-repro-v0` from `hrl/repro-infra-v0`.
2. **Stage S1**: Read `swl_scheduler` carefully. Confirm: does it match paper SWL semantics? Document the answer. Decide: reuse as-is, extend, or implement separately.
3. **Stage S2**: Add `gpgpu_enable_ccws` config knob and all other knobs via `option_parser_register`. No behavior change. Compile.
4. **Stage S3**: Run quick set with `gpgpu_enable_ccws=0`. Confirm `feature_off ≈ baseline`. Tag `ccws-config-noop`.
5. Only after S3 passes: proceed to S4 (instrumentation) and beyond.

**Reminder**: Any self-developed improvement on top of CCWS must go to `hrl/idea/<idea-key>-from-ccws-v0`, not into this paper branch.
