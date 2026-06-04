# Mascar M6 Guidance: Final Report and Closeout

## Stage position

This is M6, final closeout for the current Mascar reproduction phase.

M6 must write the final report based on actual M5 results.

If M5 ran workloads:
  Write a focused validation final report with result tables.

If M5 could not run workloads due missing benchmark environment:
  Write an implementation closeout / validation-readiness report.
  Do not claim performance speedups.
  Do not claim paper numerical reproduction.

## Paper reference points that must be covered

The report must explicitly compare the current implementation against the paper:

Paper mechanisms:

1. memory saturation detection from L1 MSHR/miss queue pressure
2. EP and MP scheduling modes
3. owner warp priority/exclusivity for memory accesses
4. WST/WRC-like per-warp memory/stall state
5. compute-ready priority in MP
6. non-owner L1 hit-only / miss-NACK
7. re-execution queue
8. one memory instruction per warp in re-exec queue
9. 32-entry queue default
10. evaluation metrics: speedup, LSU stall reduction, EP/MP time, L1 hit rate, energy

Current implementation must classify each as:

- implemented
- implemented approximately
- implemented partially
- omitted
- not evaluated

## Required final report

Create:

- docs/papers/mascar_final_reproduction_report.md

Required sections:

1. Executive summary
   - state current status:
     paper-like mechanism implemented for M1-M4, with load-only re-exec.
   - state whether runtime validation ran.
   - state whether performance results are available.

2. Repository and branch
   - branch
   - HEAD
   - key tags if known

3. Paper target
   - summarize Mascar in clear terms.
   - mention GPGPU-Sim v3.2.2 / GTX480 / Rodinia-Parboil baseline from the paper.
   - state our version/config differences.

4. Implementation summary
   - M1 L1 saturation
   - M2 EP/MP owner scheduling
   - M3 hit-only/NACK
   - M4 load-only re-exec queue

5. Mechanism checklist
   Table columns:
   - paper mechanism
   - current status
   - implementation files
   - notes/limitations

6. Config matrix
   - include M5 config matrix summary.

7. Validation method
   - describe smoke/focused matrix.
   - describe scripts.
   - list workloads or say none available.

8. Results
   If M5 produced real data:
     include result table from m5_results.csv.
     discuss whether stats activated.
     do not overclaim.
   If no data:
     state no runtime benchmark results are available.
     include script readiness and reason.

9. Debug/fix notes
   - summarize runtime bugs found/fixed in M5 if any.

10. Known limitations
    Must include:
    - GPGPU-Sim 4.x vs paper v3.2.2.
    - SM7/QV100 configs vs paper Fermi GTX480 if applicable.
    - load-only re-exec.
    - stores/atomics/texture/constant not fully re-executed.
    - no energy reproduction unless actually run.
    - no full Rodinia/Parboil sweep unless actually run.
    - re-exec queue uses mem_fetch pointer rather than compact 301-bit metadata.
    - simulator approximation for owner release via scoreboard block.

11. Reproduction instructions
    - how to build.
    - how to run focused validation script.
    - how to collect results.
    - where configs are.

12. Recommended next work
    - run real Rodinia/Parboil subset.
    - add GTX480/Fermi config if desired.
    - extend re-exec beyond loads only if safe.
    - run energy with AccelWattch if desired.

## Required closeout files

Create:

- docs/papers/mascar_closeout_summary.md
- experiments/paper-mascar/m6_closeout_manifest.csv
- experiments/paper-mascar/m6_postcheck.md
- experiments/paper-mascar/m6_diff_name_status.txt
- experiments/paper-mascar/m6_symbol_grep.txt

m6_closeout_manifest.csv columns:

- artifact
- path
- status
- description

Include all major configs, scripts, docs, results.

m6_postcheck.md must include:

- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- build result
- M5 runtime status
- final report status
- changed files
- review pack path
- warnings

## Validation before final pack

Run:

- git diff --check
- source setup_environment release && make -j2
- bash -n experiments/paper-mascar/run_m5_focused_validation.sh
- python3 -m py_compile experiments/paper-mascar/collect_m5_results.py

If there are actual results:
- run collector again and verify m5_results.csv is populated.

If no actual results:
- ensure final report says "no runtime benchmark results collected".

## Review pack

Create:

- /workspace/tmp/mascar_m5_m6_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include all changed source/config/docs/experiments files and full git diff.

Include at minimum:

- src/gpgpu-sim/gpu-cache.cc if changed
- src/gpgpu-sim/gpu-cache.h if changed
- src/gpgpu-sim/gpu-sim.cc if changed
- src/gpgpu-sim/shader.cc if changed
- src/gpgpu-sim/shader.h if changed
- configs/hrl-repro/SM7_QV100_mascar_baseline_off/ if created
- experiments/paper-mascar/run_m5_focused_validation.sh
- experiments/paper-mascar/collect_m5_results.py
- experiments/paper-mascar/m5_config_matrix.csv
- experiments/paper-mascar/m5_workload_manifest.csv
- experiments/paper-mascar/m5_results.csv
- experiments/paper-mascar/m5_results_summary.md
- docs/papers/mascar_final_reproduction_report.md
- docs/papers/mascar_closeout_summary.md
- experiments/paper-mascar/m6_postcheck.md
- full patch

Do not commit M5/M6 outputs.

## Final report to GPT

Report only:

1. elapsed_sec
2. review pack path
3. git status --short
4. build/test status
5. whether real runtime workloads ran
6. files GPT should review
