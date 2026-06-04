# Mascar Final Reproduction Report

Date: 2026-06-04

## Executive Summary

This branch now contains a paper-like Mascar mechanism stack through M4:

- M1: passive L1 saturation probe.
- M2: EP/MP owner-warp scheduling.
- M3: non-owner L1 hit-only / miss-NACK.
- M4: load-only cache access re-execution queue.

M5 collected focused runtime sanity data on one short workload,
`rodinia_hotspot`, across baseline, M1, M2, M3, and M4 configs. All focused
runs completed. These results are not a paper-comparable reproduction and do
not establish the Mascar paper's reported speedups.

## Repository And Branch

- Repository: `/workspace/repos/gpgpu-sim_distribution`
- Branch: `hrl/paper/mascar-repro-v0`
- HEAD during M5/M6: `2cf8b33`
- Simulator: GPGPU-Sim 4.x.

## Paper Target

The Mascar paper targets GPGPU-Sim v3.2.2 with GTX480/Fermi-style evaluation
and reports performance, LSU stall, EP/MP, L1 hit-rate, and energy results over
Rodinia/Parboil-style benchmark sets.

This reproduction differs materially:

- It uses the local GPGPU-Sim 4.x tree.
- The active configs are SM7/QV100-style, not GTX480/Fermi.
- Runtime validation used one short focused workload, not the paper benchmark
  suite.
- Energy saving was not reproduced.

## Implementation Summary

M1 samples L1-side pressure from MSHR/miss-queue state and prints
`paper_mascar_l1_sat_*` stats.

M2 adds EP/MP control state in `shader_core_ctx`, shared owner state across SM
schedulers, approximate WST memory/stall bits, compute-first priority in MP,
and owner release guards.

M3 adds passive and active non-owner hit-only checks. Active non-owner L1 misses
are treated as NACK/retry at the scheduler/cache interface and do not send L2
requests.

M4 adds a load-only LSU re-execution queue. It owns queued `mem_fetch *`
entries, enforces one queued memory instruction per warp, retries queue work
before new dispatch memory work, and has stats under `paper_mascar_m4_*`.

## Mechanism Checklist

| Paper mechanism | Current status | Implementation files | Notes/limitations |
|---|---|---|---|
| L1 MSHR/miss-queue saturation detection | implemented approximately | `src/gpgpu-sim/shader.cc`, `src/gpgpu-sim/shader.h` | Uses local L1D pressure sampling; not paper hardware verbatim. |
| EP/MP scheduling modes | implemented approximately | `src/gpgpu-sim/shader.cc`, `src/gpgpu-sim/shader.h` | Based on recent L1 saturation flag. |
| owner warp priority/exclusivity | implemented approximately | `src/gpgpu-sim/shader.cc` | Owner state is per SM and shared across schedulers. |
| WST/WRC-like state | implemented partially | `src/gpgpu-sim/shader.cc`, `src/gpgpu-sim/shader.h` | WST memory/stall bit approximation; no full paper WRC structure. |
| compute-ready priority in MP | implemented approximately | `src/gpgpu-sim/shader.cc` | Scheduler reorder under M2 active knob. |
| non-owner L1 hit-only / miss-NACK | implemented approximately | `src/gpgpu-sim/gpu-cache.cc`, `src/gpgpu-sim/gpu-cache.h`, `src/gpgpu-sim/shader.cc` | Active non-owner loads only; stores/atomics remain blocked. |
| cache access re-execution queue | implemented partially | `src/gpgpu-sim/shader.cc`, `src/gpgpu-sim/shader.h` | Load-only; queue stores `mem_fetch *` rather than compact metadata. |
| one memory instruction per warp in re-exec queue | implemented | `src/gpgpu-sim/shader.cc`, `src/gpgpu-sim/shader.h` | Per-warp in-queue bit. |
| 32-entry queue default | implemented | `src/gpgpu-sim/gpu-sim.cc`, configs under `configs/hrl-repro/` | `gpgpu_mascar_reexec_queue_size=32`. |
| speedup / LSU stall / EP-MP / L1 hit / energy evaluation | partially evaluated | `experiments/paper-mascar/m5_results.csv` | Only focused runtime sanity; no energy, no full suite. |

## Config Matrix

M5 config matrix: `experiments/paper-mascar/m5_config_matrix.csv`.

Included configs:

- `baseline_off`
- `m1_l1sat_probe`
- `m2_owner_sched`
- `m3_hitonly_nack`
- `m4_reexec_load`
- `m4_reexec_probe_only`

The old proxy Mascar scheduling knob remains off in the focused configs.

## Validation Method

Validation used:

```bash
git diff --check
source setup_environment release && make -j2
bash -n experiments/paper-mascar/run_m5_focused_validation.sh
python3 -m py_compile experiments/paper-mascar/collect_m5_results.py
bash experiments/paper-mascar/run_m5_focused_validation.sh
python3 experiments/paper-mascar/collect_m5_results.py <run_dir>
```

Runtime workload:

- `rodinia_hotspot`, short 64x64 1-iteration wrapper from
  `/workspace/repos/gpgpu-workloads`.

## Results

M5 focused results: `experiments/paper-mascar/m5_results.csv`.

| config | workload | exit | cycles | ipc | L1 samples | M2 MP cycles | M3 attempts | M4 enqueue | M4 retry |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| baseline_off | rodinia_hotspot | 0 | 6931 | 133.3510 | 0 | 0 | 0 | 0 | 0 |
| m1_l1sat_probe | rodinia_hotspot | 0 | 6931 | 133.3510 | 3095 | 0 | 0 | 0 | 0 |
| m2_owner_sched | rodinia_hotspot | 0 | 6931 | 133.3510 | 3095 | 495 | 0 | 0 | 0 |
| m3_hitonly_nack | rodinia_hotspot | 0 | 6931 | 133.3510 | 3095 | 495 | 0 | 0 | 0 |
| m4_reexec_load | rodinia_hotspot | 0 | 6918 | 133.6016 | 3498 | 631 | 0 | 53 | 440 |
| m4_reexec_probe_only | rodinia_hotspot | 0 | 6931 | 133.3510 | 3095 | 495 | 0 | 0 | 0 |

Interpretation:

- Baseline has Mascar active stats at zero.
- M1 detects L1 pressure on `rodinia_hotspot`.
- M2 enters MP for this workload.
- M3 active config completed, but the short workload did not produce non-owner
  hit-only attempts.
- M4 active config completed and exercised the load re-execution queue:
  `enqueue_success=53`, `retry_attempt=440`.
- The small cycle delta in the M4 row is a focused sanity observation only, not
  a paper-level performance claim.

## Debug And Fix Notes

No M5 runtime assertion, timeout, config failure, or deadlock was observed.
No M1-M4 code bug was fixed during M5/M6.

M5 added the explicit baseline comparator config:

- `configs/hrl-repro/SM7_QV100_mascar_baseline_off/`

## Known Limitations

- Current simulator is GPGPU-Sim 4.x, not the paper's GPGPU-Sim v3.2.2.
- Current active configs are SM7/QV100-style, not GTX480/Fermi.
- M4 re-execution is load-only.
- Store, atomic, texture, and constant re-execution are not supported.
- Energy saving was not reproduced; no AccelWattch energy evaluation was run.
- No full Rodinia/Parboil sweep was run.
- The re-exec queue holds `mem_fetch *` entries rather than the paper's compact
  metadata representation.
- Owner release uses simulator approximations such as scoreboard-block release
  rather than exact paper WST/WRC hardware.
- M3 hit-only/NACK needs additional workloads to show active non-owner attempts.

## Reproduction Instructions

Build:

```bash
cd /workspace/repos/gpgpu-sim_distribution
source setup_environment release
make -j2
```

Run focused validation:

```bash
cd /workspace/repos/gpgpu-sim_distribution
bash experiments/paper-mascar/run_m5_focused_validation.sh
```

Collect results:

```bash
python3 experiments/paper-mascar/collect_m5_results.py experiments/paper-mascar/m5_runs/<run_id>
```

Key configs are under `configs/hrl-repro/`:

- `SM7_QV100_mascar_baseline_off`
- `SM7_QV100_mascar_l1sat_probe_on`
- `SM7_QV100_mascar_m2_owner_sched_on`
- `SM7_QV100_mascar_m3_hitonly_nack_on`
- `SM7_QV100_mascar_m4_reexec_load_on`

## Recommended Next Work

- Run a broader but still bounded Rodinia/Parboil subset with memory and
  irregular workloads.
- Add or adapt a GTX480/Fermi config if paper-comparable evaluation is required.
- Identify workloads that trigger M3 non-owner hit-only attempts.
- Extend re-execution beyond loads only only after a careful ownership audit.
- Run AccelWattch energy studies if energy reproduction is required.
