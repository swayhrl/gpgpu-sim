# Mascar M2 Postcheck

## Timing

- start_ts: 1780565255
- end_ts: 1780565922
- elapsed_sec: 667

## Review Pack

- path: /workspace/tmp/mascar_m2_review_pack_20260604_173842.tar.gz

## Branch

- branch: hrl/paper/mascar-repro-v0
- head: 9312452

## Required Validation

- git diff --check: pass
- source setup_environment release && make -j2: pass
- smoke run: skipped; no obvious short benchmark environment was selected, and full benchmarks are disallowed for M2.

## Config Checks

- M2A owner scheduling: 0
- M2B owner scheduling: 1
- M2B old proxy scheduling: 0
- M2B old proxy would-deprioritize: 0

## Implementation Summary

- M2A passive control plane: new knobs, recent L1 saturation flag, per-SM MP/owner state in shader_core_ctx, WST memory/stall approximations, passive acquire/release telemetry, and paper_mascar_m2 stats.
- M2B active scheduling: compute-first MP reorder helper, owner/first/non-owner memory ordering, scheduler-level non-owner memory blocking, owner issue stats, and deadlock release guards.
- Not implemented in M2: re-execution queue, non-owner hit-only, miss-NACK, cache access return changes.

## Generated Files

- docs/papers/mascar_m2a_ep_mp_owner_telemetry.md
- docs/papers/mascar_m2b_active_owner_scheduling.md
- experiments/paper-mascar/m2a_notes.md
- experiments/paper-mascar/m2_postcheck.md
- experiments/paper-mascar/m2_diff_name_status.txt
- experiments/paper-mascar/m2_symbol_grep.txt
- configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/
- configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/

## Git Status Short

```
 M src/gpgpu-sim/gpu-sim.cc
 M src/gpgpu-sim/shader.cc
 M src/gpgpu-sim/shader.h
?? configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/
?? configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/
?? docs/papers/mascar_m2a_ep_mp_owner_telemetry.md
?? docs/papers/mascar_m2b_active_owner_scheduling.md
?? experiments/paper-mascar/m2_diff_name_status.txt
?? experiments/paper-mascar/m2_postcheck.md
?? experiments/paper-mascar/m2_symbol_grep.txt
?? experiments/paper-mascar/m2a_notes.md
```
