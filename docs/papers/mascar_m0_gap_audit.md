# Mascar M0 Gap Audit

## 1. Executive summary

Current branch status: **approximate/proxy Mascar**, not paper-like Mascar. The branch contains:

- Config/no-op support: Mascar knobs are registered with default-off values (`src/gpgpu-sim/gpu-sim.cc:766`, `src/gpgpu-sim/gpu-sim.cc:787`), and the config struct explicitly documents `gpgpu_enable_mascar=0` as baseline-safe (`src/gpgpu-sim/shader.h:1813`, `src/gpgpu-sim/shader.h:1821`).
- Passive telemetry: per-warp `m_mascar_stall_streak` and counters are maintained as memory-pipeline stall telemetry (`src/gpgpu-sim/shader.h:2380`, `src/gpgpu-sim/shader.h:2404`).
- Proxy scheduling: when enabled, a post-scoreboard memory-instruction skip gate uses stall streaks before checking `m_mem_out->has_free()` (`src/gpgpu-sim/shader.cc:1416`, `src/gpgpu-sim/shader.cc:1425`; `src/gpgpu-sim/shader.h:2432`, `src/gpgpu-sim/shader.h:2450`).

It is not an exact paper-like Mascar implementation. The branch's own final metadata says the mechanism is a "minimal scheduling skip-gate proxy", with no faithful replay/re-execution, no true MSHR occupancy tracking, and an `m_mem_out / scheduler-side proxy` signal (`tools/paper_repro/papers/mascar.yaml:44`, `tools/paper_repro/papers/mascar.yaml:51`). The CSV summary repeats the same classification (`experiments/paper-mascar/final_summary.csv:12`, `experiments/paper-mascar/final_summary.csv:22`).

M0 changed only documentation and audit helper files. It did not implement mechanisms and did not modify simulator behavior.

## 2. Branch state and ancestry

- Current branch: `hrl/paper/mascar-repro-v0`.
- HEAD: `c4c97548c39c` (`docs: add Mascar M0 audit guidance`).
- Upstream tracking branch: `origin/hrl/paper/mascar-repro-v0`.
- Commits after `hrl/paper/daws-repro-v0`: 17. The short log is saved in `experiments/paper-mascar/m0_commits_after_daws.txt`.
- Commits after `paper-repro-supervisor-v0`: 24.
- Diff versus `hrl/paper/daws-repro-v0`: Mascar docs, experiment files, configs, tool metadata, and simulator changes in `src/gpgpu-sim/gpu-sim.cc`, `src/gpgpu-sim/shader.cc`, and `src/gpgpu-sim/shader.h` (see `experiments/paper-mascar/m0_diff_name_status_vs_daws.txt`).
- Diff versus `baseline-a4ce3fe`: broad paper-repro scaffolding plus CCWS/DAWS/Mascar artifacts; the baseline diff is much wider than Mascar alone, so DAWS-relative diff is the cleaner branch-local audit boundary.

Why this matters: the branch already includes real scheduler behavior behind Mascar knobs. M1 and later work should not assume this is a blank telemetry-only branch; it already has an approximate skip policy in `scheduler_unit::cycle()` (`src/gpgpu-sim/shader.cc:1416`, `src/gpgpu-sim/shader.cc:1444`).

## 3. Current implementation summary

### Knobs and configs

The registered Mascar knobs are `gpgpu_enable_mascar`, telemetry, would-deprioritize, scheduling, stall threshold, max skip streak, and debug (`src/gpgpu-sim/gpu-sim.cc:766`, `src/gpgpu-sim/gpu-sim.cc:787`). The config struct stores exactly these fields and notes the default-off baseline intent (`src/gpgpu-sim/shader.h:1813`, `src/gpgpu-sim/shader.h:1821`).

The hrl configs cover four useful modes:

- `mascar_noop_off`: `-gpgpu_enable_mascar 0` (`configs/hrl-repro/SM7_QV100_mascar_noop_off/gpgpusim.config:240`, `configs/hrl-repro/SM7_QV100_mascar_noop_off/gpgpusim.config:241`).
- `mascar_noop_on`: master enabled but telemetry, would-deprioritize, and scheduling disabled (`configs/hrl-repro/SM7_QV100_mascar_noop_on/gpgpusim.config:240`, `configs/hrl-repro/SM7_QV100_mascar_noop_on/gpgpusim.config:244`).
- `mascar_telemetry_on`: telemetry enabled, scheduling disabled (`configs/hrl-repro/SM7_QV100_mascar_telemetry_on/gpgpusim.config:240`, `configs/hrl-repro/SM7_QV100_mascar_telemetry_on/gpgpusim.config:245`).
- `mascar_policy_on`: telemetry, would-deprioritize, and scheduling enabled, with `stall_threshold 2` and `max_skip_streak 4` (`configs/hrl-repro/SM7_QV100_mascar_policy_on/gpgpusim.config:240`, `configs/hrl-repro/SM7_QV100_mascar_policy_on/gpgpusim.config:246`).

### Stats and reports

Stats are aggregated at SM and cluster level (`src/gpgpu-sim/shader.h:2365`, `src/gpgpu-sim/shader.h:2377`; `src/gpgpu-sim/shader.cc:5275`, `src/gpgpu-sim/shader.cc:5283`) and printed as `paper_mascar_*` fields (`src/gpgpu-sim/gpu-sim.cc:1816`, `src/gpgpu-sim/gpu-sim.cc:1832`). Existing stats are: enabled, mem_stall_event, saturation_event, pitstop_event, would_deprioritize, skip_count, and allow_count. They do not include EP cycles, MP cycles, owner switch count, owner issue count, non-owner blocked count, re-exec enqueue/hit/NACK/retry, or deadlock guard events.

The final report describes the current mechanism as an `m_mem_out` proxy, with stall detection based on `m_mem_out->has_free()` and a post-scoreboard skip gate (`docs/papers/mascar_final_reproduction_report.md:39`, `docs/papers/mascar_final_reproduction_report.md:59`). It explicitly says this is not paper-exact and lacks pipeline replay/re-execution (`docs/papers/mascar_final_reproduction_report.md:188`, `docs/papers/mascar_final_reproduction_report.md:190`).

### Scheduling behavior

The only active Mascar behavior is in the memory-instruction path after scoreboard readiness. A memory instruction calls `mascar_skip_gate_warp()` before the `m_mem_out->has_free()` issue check (`src/gpgpu-sim/shader.cc:1405`, `src/gpgpu-sim/shader.cc:1430`). If `m_mem_out` is full, the code records a memory stall and would-deprioritize telemetry (`src/gpgpu-sim/shader.cc:1439`, `src/gpgpu-sim/shader.cc:1444`). Successful memory issue resets stall and skip streaks (`src/gpgpu-sim/shader.cc:1435`, `src/gpgpu-sim/shader.cc:1438`).

The skip gate is not owner-based. It skips any warp whose stall streak reaches the configured threshold, then force-allows after the configured skip streak (`src/gpgpu-sim/shader.h:2427`, `src/gpgpu-sim/shader.h:2450`). The only Mascar per-warp state is stall streak and skip streak (`src/gpgpu-sim/shader.h:2917`, `src/gpgpu-sim/shader.h:2927`).

### Cache and LSU interfaces

GPGPU-Sim has L1 MSHR and miss queue state: `mshr_table` exposes `probe`, `full`, and `add` (`src/gpgpu-sim/gpu-cache.h:1024`, `src/gpgpu-sim/gpu-cache.h:1041`), and `baseline_cache` stores `m_mshrs` plus `m_miss_queue` (`src/gpgpu-sim/gpu-cache.h:1396`, `src/gpgpu-sim/gpu-cache.h:1402`). The cache miss path checks MSHR availability and miss queue space (`src/gpgpu-sim/gpu-cache.cc:1446`, `src/gpgpu-sim/gpu-cache.cc:1472`), and reservation failure paths are already represented (`src/gpgpu-sim/gpu-cache.cc:1540`, `src/gpgpu-sim/gpu-cache.cc:1543`; `src/gpgpu-sim/gpu-cache.cc:1614`, `src/gpgpu-sim/gpu-cache.cc:1631`).

Current Mascar does not use those cache-side resources. The LSU allocates a `mem_fetch`, calls `cache->access()`, and on `RESERVATION_FAIL` returns `BK_CONF` and deletes the fetch (`src/gpgpu-sim/shader.cc:2205`, `src/gpgpu-sim/shader.cc:2240`; `src/gpgpu-sim/shader.cc:2324`, `src/gpgpu-sim/shader.cc:2330`). There is no Mascar-specific NACK or re-execution queue in this path.

## 4. Paper mechanism checklist

Target paper-like Mascar mechanisms for later rounds:

1. EP mode: equal priority when memory is not saturated; memory-ready warps are favored enough to keep memory pipeline utilization high.
2. MP mode: memory access priority mode during saturation; one owner warp has priority or exclusivity for memory requests.
3. L1 saturation flag: saturation should be detected at L1/back-pressure level using MSHR and miss queue pressure, not only scheduler-side `m_mem_out` fullness.
4. WST/WRC: a Warp Status Table records per-warp memory operation and stall state; a Warp Readiness Checker determines next memory operation, owner status, and scoreboard dependency.
5. Owner warp management: the owner keeps issuing memory requests until it reaches an instruction dependent on its own long-latency load, then releases ownership.
6. MP prioritization: compute-ready warps should have priority over memory-ready warps during MP mode to overlap compute with outstanding memory.
7. Non-owner memory handling: non-owner memory requests may hit in L1, but non-owner misses must not inject new L2 traffic during MP saturation.
8. Cache access re-execution queue: blocked requests can be requeued and later retried; non-owner misses are NACKed/requeued; owner misses proceed when resources are available.
9. Ordering constraint: only one memory instruction per warp may be present in the re-execution queue at a time.
10. Validation/stats: EP cycles, MP cycles, owner switch count, owner issue count, non-owner blocked count, saturation cycles, re-exec enqueue, re-exec hit, re-exec NACK, re-exec retry, and deadlock guard events.

## 5. Gap matrix

| Paper mechanism | Paper requirement | Current implementation | Evidence | Severity | Follow-up |
|---|---|---|---|---|---|
| L1 saturation flag | L1-side saturation/back-pressure from MSHR and miss queue pressure. | Current signal is scheduler-side stall streak from `m_mem_out` fullness; branch metadata says true MSHR tracking is false. | `src/gpgpu-sim/shader.cc:1425`, `src/gpgpu-sim/shader.cc:1441`; `tools/paper_repro/papers/mascar.yaml:47`, `tools/paper_repro/papers/mascar.yaml:49`; L1 resources exist at `src/gpgpu-sim/gpu-cache.h:1024`, `src/gpgpu-sim/gpu-cache.h:1041`. | major | M1 |
| EP mode | Unsaturated mode with memory-ready preference to keep the memory pipeline busy. | No explicit EP mode or EP-cycle state; default scheduler ordering is unchanged except for optional skip gate in memory path. | Scheduler loop orders warps then tests candidates at `src/gpgpu-sim/shader.cc:1294`, `src/gpgpu-sim/shader.cc:1304`; Mascar only gates in memory branch at `src/gpgpu-sim/shader.cc:1416`, `src/gpgpu-sim/shader.cc:1424`. | missing | M2 |
| MP mode | Saturated mode with owner-warp priority/exclusivity for memory requests. | No MP mode variable and no owner-warp field; only per-warp stall/skip streaks exist. | `src/gpgpu-sim/shader.h:2917`, `src/gpgpu-sim/shader.h:2927`; `experiments/paper-mascar/m0_symbol_grep.txt` records no simulator `owner_warp` or WST/WRC symbols. | missing | M2 |
| Owner warp | One owner keeps memory-request priority during MP. | Skip gate deprioritizes stalled warps; it does not select or track an owner. | `src/gpgpu-sim/shader.h:2432`, `src/gpgpu-sim/shader.h:2450`; only state at `src/gpgpu-sim/shader.h:2917`, `src/gpgpu-sim/shader.h:2927`. | missing | M2 |
| WST/WRC | WST tracks warp memory/stall state; WRC determines next memory op, owner, and scoreboard dependency. | Per-warp stall streak is a telemetry counter, not a WST; scoreboard is only used for normal collision checking before memory branch. | Scoreboard check at `src/gpgpu-sim/shader.cc:1382`, `src/gpgpu-sim/shader.cc:1387`; telemetry at `src/gpgpu-sim/shader.h:2380`, `src/gpgpu-sim/shader.h:2404`. | missing | M2 |
| Scoreboard dependency based owner release | Owner releases when it reaches an instruction dependent on its own long-latency load. | No owner exists; current code resets stall/skip streak on successful memory issue, not on owner dependency detection. | `src/gpgpu-sim/shader.cc:1435`, `src/gpgpu-sim/shader.cc:1438`; `src/gpgpu-sim/shader.h:2453`, `src/gpgpu-sim/shader.h:2459`. | missing | M2 |
| Compute-ready priority in MP | In MP, compute-ready warps should outrank memory-ready non-owner work to overlap compute/memory. | Current Mascar hook only handles memory instructions; non-memory instruction priority is not mode-adjusted. | Memory branch starts at `src/gpgpu-sim/shader.cc:1405`; non-memory execution continues through normal SP/INT/SFU paths at `src/gpgpu-sim/shader.cc:1445`, `src/gpgpu-sim/shader.cc:1516`. | missing | M2 |
| Non-owner L1 hit-only / miss NACK | Non-owner memory requests may hit L1 but must not send misses to L2 in MP. | No owner/non-owner test exists at cache access. LSU passes all L1D accesses through `cache->access()` and handles status generically. | `src/gpgpu-sim/shader.cc:2324`, `src/gpgpu-sim/shader.cc:2330`; read misses enqueue to miss queue at `src/gpgpu-sim/gpu-cache.cc:1459`, `src/gpgpu-sim/gpu-cache.cc:1472`. | missing | M3 |
| Cache access re-execution queue | Back-pressure blocked accesses are queued and retried later. | No Mascar re-exec queue; `RESERVATION_FAIL` returns `BK_CONF` and deletes the temporary fetch. Existing docs call replay/re-execution absent. | `src/gpgpu-sim/shader.cc:2236`, `src/gpgpu-sim/shader.cc:2240`; `docs/papers/mascar_final_reproduction_report.md:188`, `docs/papers/mascar_final_reproduction_report.md:190`. | missing | M3 |
| One memory instruction per warp in re-exec queue | At most one queued memory instruction per warp. | No queue exists, so no per-warp queue occupancy invariant exists. | Current memory issue tracks pending writes for loads at `src/gpgpu-sim/shader.cc:2913`, `src/gpgpu-sim/shader.cc:2928`; Mascar state lacks queue occupancy at `src/gpgpu-sim/shader.h:2917`, `src/gpgpu-sim/shader.h:2927`. | missing | M3 |
| Validation/stats gap | Need EP/MP/owner/non-owner/re-exec/deadlock-guard stats. | Current printed stats stop at enabled, mem stall, saturation proxy, pitstop, would-deprioritize, skip, allow. | `src/gpgpu-sim/gpu-sim.cc:1816`, `src/gpgpu-sim/gpu-sim.cc:1832`. | major | M4 |

## 6. No-op and baseline safety

Default simulator config registration is baseline-safe by intent because every Mascar behavior knob defaults off (`src/gpgpu-sim/gpu-sim.cc:766`, `src/gpgpu-sim/gpu-sim.cc:787`) and the shader config comment states `gpgpu_enable_mascar=0` means zero behavior change (`src/gpgpu-sim/shader.h:1813`, `src/gpgpu-sim/shader.h:1821`).

Baseline risk when disabled appears low: `mascar_record_mem_stall`, `mascar_reset_stall_streak`, `mascar_check_would_deprioritize`, `mascar_skip_gate_warp`, and `mascar_reset_skip_streak` all return early when the relevant Mascar enable bits are off (`src/gpgpu-sim/shader.h:2383`, `src/gpgpu-sim/shader.h:2386`; `src/gpgpu-sim/shader.h:2407`, `src/gpgpu-sim/shader.h:2410`; `src/gpgpu-sim/shader.h:2418`, `src/gpgpu-sim/shader.h:2421`; `src/gpgpu-sim/shader.h:2432`, `src/gpgpu-sim/shader.h:2435`; `src/gpgpu-sim/shader.h:2454`, `src/gpgpu-sim/shader.h:2457`). Existing no-op CSV rows report zero cycle delta for sampled workloads (`experiments/paper-mascar/config_matrix.csv:2`, `experiments/paper-mascar/config_matrix.csv:6`).

Risk when enabled is intentional: `mascar_policy_on` changes scheduling by enabling `gpgpu_mascar_enable_scheduling 1` (`configs/hrl-repro/SM7_QV100_mascar_policy_on/gpgpusim.config:240`, `configs/hrl-repro/SM7_QV100_mascar_policy_on/gpgpusim.config:244`), and focused validation shows nonzero skip counts (`experiments/paper-mascar/focused_validation.csv:3`, `experiments/paper-mascar/focused_validation.csv:19`).

Unknown: the exact interaction with all non-QV100 configs is not audited in M0. The implementation is guarded by global options, but broad config/build regression was not run in this round.

## 7. Recommended next rounds

- M1: add passive L1 saturation probe using L1 MSHR/miss queue pressure. The relevant existing resource points are `mshr_table` and `m_miss_queue` (`src/gpgpu-sim/gpu-cache.h:1024`, `src/gpgpu-sim/gpu-cache.h:1041`; `src/gpgpu-sim/gpu-cache.h:1396`, `src/gpgpu-sim/gpu-cache.h:1402`).
- M2: implement explicit EP/MP owner scheduling without re-exec. This should replace the current pure stall-streak skip proxy (`src/gpgpu-sim/shader.h:2432`, `src/gpgpu-sim/shader.h:2450`) with mode and owner state.
- M3: add cache access re-execution queue, non-owner hit-only/miss-NACK handling, and one-memory-instruction-per-warp queue invariant. The sensitive LSU/cache path is `process_cache_access()` and `process_memory_access_queue_l1cache()` (`src/gpgpu-sim/shader.cc:2205`, `src/gpgpu-sim/shader.cc:2330`).
- M4: validate and report with paper-target stats beyond the existing `paper_mascar_*` counters (`src/gpgpu-sim/gpu-sim.cc:1816`, `src/gpgpu-sim/gpu-sim.cc:1832`).

## 8. Risks and unknowns

- Cache API risk: L1 MSHR and miss queue are protected cache internals (`src/gpgpu-sim/gpu-cache.h:1396`, `src/gpgpu-sim/gpu-cache.h:1440`). M1 may need a narrow accessor rather than scheduler-side guessing.
- LSU request representation risk: the LSU creates transient `mem_fetch` objects and deletes them on reservation failure (`src/gpgpu-sim/shader.cc:2263`, `src/gpgpu-sim/shader.cc:2272`; `src/gpgpu-sim/shader.cc:2236`, `src/gpgpu-sim/shader.cc:2240`). Re-exec needs careful ownership/lifetime rules.
- Scoreboard/owner-release risk: load pending writes are tracked in `ldst_unit::issue()` (`src/gpgpu-sim/shader.cc:2913`, `src/gpgpu-sim/shader.cc:2928`), but current Mascar does not inspect these dependencies for owner release.
- Deadlock risk: current proxy has a force-allow after `max_skip_streak` (`src/gpgpu-sim/shader.h:2439`, `src/gpgpu-sim/shader.h:2444`), but paper-like owner plus re-exec will need different deadlock guards and stats.
- Validation limitations: current focused validation is small and reports tiny cycle impact; final summary says paper-level speedup is not reproduced (`experiments/paper-mascar/final_summary.csv:16`, `experiments/paper-mascar/final_summary.csv:22`).
- Unknown: exact original-paper WST/WRC field definitions should be rechecked from the paper before M2 implementation. M0 used the provided guidance as the target mechanism checklist.

## 9. M0 signoff

M0 changed:

- Added this gap audit: `docs/papers/mascar_m0_gap_audit.md`.
- Added postcheck and helper audit files under `experiments/paper-mascar/`.
- Created a review pack under `/workspace/tmp/`.

M0 did not:

- Modify `src/gpgpu-sim`.
- Modify `configs`.
- Implement L1 saturation probing.
- Implement EP/MP owner scheduling.
- Implement WST/WRC.
- Implement non-owner L1 hit-only / miss NACK.
- Implement cache access re-execution.
- Run long benchmarks.
- Commit any M0 artifact.
