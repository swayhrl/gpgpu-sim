# Paper Reproduction Plan Template

_Copy this file to `docs/papers/<paper-key>_repro_plan.md` and fill in each section._

---

## 1. Paper Metadata

| Field | Value |
|-------|-------|
| **paper key** | `<paper-key>` (lowercase, no spaces, e.g. `ccws`) |
| **title** | Full paper title |
| **authors** | Author list |
| **venue / year** | Conference + year (e.g. MICRO 2011) |
| **link / DOI** | URL or DOI string |
| **target mechanism category** | e.g. cache bypassing / replacement policy / throttling |
| **expected GPGPU-Sim modules** | e.g. `gpu-cache.h`, `shader.cc`, `gpu-sim.cc` |

---

## 2. One-sentence Summary

_What does this paper propose, in one sentence?_

> Example: "CCWS throttles warps per-SM to prevent cache thrashing when the working set exceeds L1 capacity."

---

## 3. Problem and Motivation

_What bottleneck or inefficiency does the paper address? What is the root cause?_

- Problem:
- Root cause (as described by the paper):
- Evidence metric (what stat worsens under the problem):

---

## 4. Mechanism Summary

_Describe the paper's mechanism concisely. Include the key decision logic and data structures._

Key mechanism steps:
1.
2.
3.

Key data structures needed:
-
-

---

## 5. Mapping to GPGPU-Sim

| Paper Concept | GPGPU-Sim Module | Candidate Files | Confidence | Notes |
|---------------|-----------------|-----------------|-----------|-------|
| | | | high / med / low | |
| | | | | |
| | | | | |

---

## 6. Implementation Scope

### Must implement
- [ ]
- [ ]

### Nice to have
- [ ]
- [ ]

### Explicitly out of scope
- Everything not in "must implement" — do not expand scope mid-implementation.
- Do not port paper's evaluation methodology beyond what is needed to verify the mechanism.

---

## 7. Config Knobs

| Parameter | Default | Off behavior | On behavior | Expected values |
|-----------|---------|-------------|------------|----------------|
| `-gpgpu_enable_<paper_key>` | `0` | No change to simulation | Mechanism active | `0` or `1` |
| | | | | |

---

## 8. Instrumentation Plan

Suggested stats (prefix: `paper_<key>_`):

| Stat | Description |
|------|-------------|
| `paper_<key>_trigger_count` | How many times the mechanism decision logic ran |
| `paper_<key>_action_count` | How many times an action was taken (bypass / insert / etc.) |
| `paper_<key>_bypass_count` | Cache bypass events (if applicable) |
| `paper_<key>_insert_count` | Cache insert events (if applicable) |
| `paper_<key>_evict_count` | Cache eviction events triggered by mechanism |
| `paper_<key>_stall_count` | Stall cycles attributed to mechanism overhead |

All stats must be gated by `gpgpu_enable_<paper_key>` so they read 0 when the feature is off.

---

## 9. Behavior Change Plan

Rules:
- Feature flag **default off** (`-gpgpu_enable_<paper_key> 0`).
- Minimal implementation first — do not over-engineer.
- Avoid modifying unrelated paths (no TLB, no interconnect, no DRAM changes unless explicitly required).
- `baseline` vs `feature_off` **must match** before proceeding to `feature_on`.

Implementation steps:
1. Add config knob and stat declarations (no behavior change).
2. Add instrumentation hooks (no behavior change, stats should show 0 or expected passive count).
3. Add minimal behavior gated by feature flag.
4. Run quick set feature_off — confirm ≈ baseline.
5. Run quick set feature_on — confirm mechanism triggers.

---

## 10. Validation Plan

| Step | Command / Action | Pass Criteria |
|------|-----------------|---------------|
| Compile check | `make -j$(nproc)` | No errors, warnings only |
| Quick set feature_off | `run_workload_set.sh quick` with feature off | sim_cycle ≈ baseline ±1% |
| Quick set feature_on | `run_workload_set.sh quick` with feature on | Mechanism triggers (stats > 0) |
| Standard set feature_on | `run_workload_set.sh standard` | Sensitive workloads show expected change |
| Focused workload set | Run `<focus_workloads>` only | Paper's key workloads behave as expected |
| Behavior check | Inspect `paper_<key>_trigger_count` | > 0 for sensitive workloads |
| Regression check | Compare all metrics vs baseline | No unexpected regressions in feature_off mode |

---

## 11. Expected Workload Sensitivity

| Category | Workloads |
|----------|-----------|
| Quick workloads | vecadd, strided_access, page_stride_access, atomic_contention, mutual_tiled, polybench_2dconv, rodinia_hotspot |
| Standard workloads | (all quick) + mutual_naive, polybench_gemm, polybench_fdtd2d, rodinia_srad_v2, rodinia_lud, rodinia_pathfinder |
| Focus workloads | _List workloads most likely to be sensitive to this mechanism_ |
| Expected positive (mechanism helps) | _List_ |
| Expected neutral (mechanism has no effect) | _List_ |

---

## 12. Result Recording

| Item | Path |
|------|------|
| Result CSV | `experiments/paper-<paper-key>/result_manifest.csv` |
| Run notes | `experiments/paper-<paper-key>/run_notes.md` |
| Logs | `/workspace/experiments/gpgpu-sim/<paper-key>/<timestamp>/` (not committed) |
| Figures | `experiments/paper-<paper-key>/figures/` (if generated) |
| Summary CSV | `/workspace/repos/gpgpu-workloads/runs/latest_summary.csv` |

---

## 13. Risks and Known Gaps

| Risk | Severity | Mitigation |
|------|---------|------------|
| Paper uses a different cache model (e.g., victim cache) | Medium | Note gap, document simplification |
| Paper's hardware has different parameters than QV100 | Low | Use closest available config |
| Missing performance counters for full replication | Low | Note which stats are unavailable |

---

## 14. Commit / Tag Milestones

| Milestone | Tag | Description |
|-----------|-----|-------------|
| repro plan complete | `<paper-key>-plan-v0` | Written this template, no code change |
| minimal implementation | `<paper-key>-minimal-impl` | Feature flag + instrumentation + minimal behavior |
| quick set pass | `<paper-key>-quick-pass` | feature_off ≈ baseline confirmed |
| standard set result | `<paper-key>-standard-pass` | feature_on results on standard set |

---

## 15. Final Decision

- [ ] **keep** — results match paper claims sufficiently, merge to `hrl/integration/cache-papers-v0`
- [ ] **revise** — results partially match, need refinement
- [ ] **abandon** — mechanism not reproducible in GPGPU-Sim, document why
- [ ] **merge to integration** — selected for integration testing

Notes:
