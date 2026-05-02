# Mascar Approximate Reproduction Final Report

> **Canonical final artifact** for the Mascar approximate reproduction.
> Branch: hrl/paper/mascar-repro-v0

**Paper**: Speeding Up GPU Warps by Reducing Memory Pitstops (HPCA 2015)  
**Authors**: Ali Bakhoda, George L. Yuan, Wilson W. L. Fung, Henry Wong, Tor M. Aamodt  
**Branch**: hrl/paper/mascar-repro-v0  
**Date**: 2026-05-01  
**Status**: Approximate Reproduction Complete (NOT a faithful reproduction)

---

## 1. Paper Summary

Mascar identifies "memory pitstops" — situations where GPU warps are repeatedly stalled
waiting for the memory pipeline (MSHRs full, memory bandwidth saturated). The key insight
is to deprioritize warps that are in pitstops, allowing other warps to drain the memory
pipeline and proceed. This reduces head-of-line blocking and improves overall throughput.

**Core mechanism** (paper):
1. Detect warps whose memory requests saturate MSHR entries
2. Track consecutive stall cycles per warp (pitstop streak)
3. When a warp's streak exceeds a threshold, skip it in scheduling
4. Force-allow after N consecutive skips to prevent deadlock

---

## 2. Implementation: Approximate Proxy

**This is an approximate reproduction only.** The following capabilities from the paper
are NOT implemented:
- **No pipeline replay or re-execution** — the paper's primary speedup source
- **No true MSHR occupancy tracking** — we use a coarser scheduler-side proxy
- **No per-static-PC load classification** — all memory ops (loads + stores) are gated

### Proxy Strategy

The original Mascar paper monitors MSHR occupancy and memory pipeline utilization.
Our GPGPU-Sim implementation approximates this using the per-scheduler `m_mem_out`
register set:

- **Stall detection**: `m_mem_out->has_free()` returning `false` = memory pipeline slot full
  (this is a coarser signal than true MSHR occupancy)
- **Streak counter**: Per-warp `m_mascar_stall_streak[]` incremented on each stall
- **Skip gate**: Post-scoreboard, inside memory instruction block
- **Deadlock prevention**: Per-warp `m_mascar_skip_streak[]` + `max_skip_streak` force-allow

### Hook Point

`scheduler_unit::cycle()` in `shader.cc`, inside the memory instruction branch:

```
ibuffer → pre-scoreboard → scoreboard check →
[memory inst?] → CCWS check → [Mascar skip gate] → m_mem_out check → issue/stall
```

The skip gate fires BEFORE the `m_mem_out->has_free()` check, using the streak
accumulated from previous cycles.

### Mechanism Chain

| Phase | Mechanism | Status |
|-------|-----------|--------|
| 1 | No-op config + stats (7 knobs, 7 stat counters) | ✓ Complete |
| 2 | Memory stall telemetry (stall_streak, mem_stall_event) | ✓ Complete |
| 3 | Would-deprioritize telemetry (passive, no scheduling) | ✓ Complete |
| 4 | Minimal scheduling skip-gate proxy (post-scoreboard, approximate) | ✓ Complete |
| 5 | Focused validation (9 workloads, no deadlock) | ✓ Complete |

---

## 3. Config Knobs

All knob names follow the `gpgpu_mascar_*` prefix convention:

| Knob | Default | policy_on | Description |
|------|---------|-----------|-------------|
| `gpgpu_enable_mascar` | 0 | 1 | Master enable (master guard) |
| `gpgpu_mascar_enable_telemetry` | 0 | 1 | Enable stall streak tracking |
| `gpgpu_mascar_enable_would_deprioritize` | 0 | 1 | Enable would-skip telemetry |
| `gpgpu_mascar_enable_scheduling` | 0 | 1 | Enable real scheduler skip |
| `gpgpu_mascar_stall_threshold` | 8 | 2 | Streak to trigger skip |
| `gpgpu_mascar_max_skip_streak` | 4 | 4 | Skips before force-allow (deadlock guard) |
| `gpgpu_mascar_debug` | 0 | 0 | Debug prints |

**Note on threshold=2**: The original Mascar paper targets significant memory saturation
(MSHR utilization > 80%). Our m_mem_out proxy fires more aggressively; threshold=2
is chosen to produce observable skip events given this coarser proxy.

---

## 4. Focused Validation Results

**Configs**: mascar_noop_off (baseline) vs mascar_policy_on (threshold=2, max_skip_streak=4)  
**Data**: experiments/paper-mascar/focused_validation.csv

| Workload | noop_off | policy_on | Δ cycles | skip_count | allow_count | deadlock |
|----------|----------|-----------|----------|------------|-------------|----------|
| vecadd | 5569 | 5560 | -9 (-0.16%) | 44 | 11 | no |
| rodinia_hotspot | 6931 | 6923 | -8 (-0.12%) | 8300 | 2075 | no |
| rodinia_srad_v2 | 15926 | 15918 | -8 (-0.05%) | 5516 | 1379 | no |
| rodinia_bfs | 136110 | 136110 | 0 | 0 | 0 | no |
| strided_access | 5825 | 5821 | -4 (-0.07%) | 44 | 11 | no |
| polybench_fdtd2d | 35681 | 35720 | +39 (+0.11%) | 1728 | 432 | no |
| polybench_2dconv | 6652 | 6651 | -1 (-0.02%) | 2984 | 746 | no |
| mutual_tiled | 7479 | 7472 | -7 (-0.09%) | 216 | 54 | no |
| parboil_histo | 35472 | 35519 | +47 (+0.13%) | 13440 | 3360 | no |

**Observations under this proxy**:
- skip/allow ratio = 4.0 for all workloads (= max_skip_streak) — deadlock prevention works ✓
- Deadlock prevention verified in all 9 workloads ✓
- BFS: skip_count=0 consistent with low per-warp memory pressure under this proxy ✓
- 7/9 workloads improve-or-neutral (6 improved + 1 unchanged); 2/9 show small regression

---

## 5. Comparison with Paper Claims

The paper reports speedups of 5–15% on memory-intensive benchmarks (STREAM, Rodinia).
Cycle changes in our runs range from **-0.16% to +0.13%** across 9 workloads — all
within noise range for small workloads. **Paper-level speedup (5–15%) is NOT reproduced.**

**Root causes of the gap**:

| Factor | Paper | Our Implementation |
|--------|-------|-------------------|
| Saturation proxy | True MSHR occupancy + BW utilization | m_mem_out register set (scheduler slot, single entry) |
| Threshold | Tuned per GPU (high specificity) | threshold=2 (aggressive, coarse proxy) |
| Workload size | Full benchmarks (large datasets) | Tiny workloads (64×64 matrices, small graphs) |
| MSHR pressure | Real MSHR contention visible | m_mem_out is per-scheduler, coarser granularity |
| Memory bandwidth | HBM bandwidth utilization modeled | Not directly modeled |
| Replay/re-execution | Core speedup mechanism | NOT implemented |

Small effect sizes under this proxy are consistent with the above limitations,
not evidence of the paper's claimed performance benefit.

---

## 6. Approximation Limitations

| Limitation | Impact | Closer Approximation |
|------------|--------|-------------|
| m_mem_out proxy (not true MSHR) | Less precise saturation signal | Monitor MSHR hit-under-miss count |
| No pipeline replay/re-execution | Primary speedup source absent | Implement per-warp replay queue |
| No memory bandwidth tracking | Can't distinguish BW-limited vs MSHR-limited | Add per-cycle BW utilization probe |
| Tiny workloads | Skip-benefit overwhelmed by skip-cost | Use 256×256 or larger datasets |
| Aggressive threshold (=2) | Many spurious skips | Tune per workload or use adaptive threshold |
| No load/store distinction | Skips ALL memory ops including stores | Gate only loads, not stores/barriers |

---

## 7. Mechanism Chain Verification

```
feature_off (noop_off) regression:
  vecadd: 5569 = baseline ✓
  all paper_mascar_* stats = 0 ✓

telemetry signal (would_change_on):
  hotspot: mem_stall_event=12150, would_deprioritize=10025 ✓
  srad_v2: mem_stall_event=3989,  would_deprioritize=3237 ✓

policy_on proxy mechanism:
  skip_count > 0 for 8/9 workloads ✓
  allow_count > 0 where skip_count > 0 ✓
  skip/allow = max_skip_streak for all ✓
  No deadlock ✓
```

---

## 8. Conclusion

**Approximate mechanism reproduction: COMPLETE**  
**This is NOT a faithful reproduction of the paper.**

The Mascar memory-pitstop scheduling mechanism has been implemented as a
scheduler-side proxy (m_mem_out stall streak) in GPGPU-Sim:

- The approximate proxy mechanism chain is complete and functional
- Feature flag (`gpgpu_enable_mascar=0`) preserves baseline exactly
- Deadlock prevention (max_skip_streak force-allow) works correctly
- Memory-pressure signal direction is consistent with this proxy implementation
  (BFS: no skips; hotspot: high skip count, consistent with memory-bound workload)
- Cycle impact is small (<0.2% in either direction) — **not evidence of paper-level speedup**

**What this is NOT**:
- Not a faithful reproduction of the paper
- No pipeline replay or re-execution (the paper's primary speedup source)
- Memory pressure detection uses m_mem_out (scheduler slot), not true MSHR occupancy
- Cycle deltas (<0.2%) are far below the paper's claims — do not cite as confirmation
- Workloads are tiny; quantitative results are not comparable to the paper
- Paper-level speedup (5–15%) is NOT reproduced

**Next steps toward a more faithful reproduction**:
1. Use larger workloads (256×256 matrices, larger graphs)
2. Implement true MSHR tracking via `m_ldst_unit->mshr` occupancy probe
3. Implement per-warp replay queue (primary speedup mechanism)
4. Add memory bandwidth utilization tracking
