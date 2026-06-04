# Mascar M3 Postcheck

## Timing

- start_iso: 2026-06-04T17:53:31+08:00
- end_iso: 2026-06-04T18:02:55+08:00
- start_ts: 1780566811
- end_ts: 1780567375
- elapsed_sec: 564

## Review Pack

- path: /workspace/tmp/mascar_m3_review_pack_20260604_180255.tar.gz

## Branch

- branch: hrl/paper/mascar-repro-v0
- head: 3bd1a5b

## Git Status Before

M3 start status was clean on branch hrl/paper/mascar-repro-v0 before M3 edits.

## Git Status After

~~~text
 M src/gpgpu-sim/gpu-cache.cc
 M src/gpgpu-sim/gpu-cache.h
 M src/gpgpu-sim/gpu-sim.cc
 M src/gpgpu-sim/shader.cc
 M src/gpgpu-sim/shader.h
?? configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on/
?? configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/
?? docs/papers/mascar_m3_status.md
?? docs/papers/mascar_m3a_nonowner_hitonly_probe.md
?? docs/papers/mascar_m3b_active_hitonly_nack.md
?? experiments/paper-mascar/m3_diff_name_status.txt
?? experiments/paper-mascar/m3_postcheck.md
?? experiments/paper-mascar/m3_symbol_grep.txt
?? experiments/paper-mascar/m3a_notes.md
~~~

## Changed Files

See experiments/paper-mascar/m3_diff_name_status.txt.

## M3A Summary

- Added default-off passive and active M3 knobs.
- Added read-only L1D hit-only tag probe helper.
- Added passive would-hit / would-NACK telemetry before normal L1D access in no-latency and L1 latency queue paths.
- Added passive M3A config with active hit-only off.

## M3B Summary

- Refined M2 scheduler gate so non-owner loads can issue to LSU only when active M3 hit-only is enabled.
- Kept non-owner stores, atomics, and non-load memory ops blocked by M2 default behavior.
- Added active L1D hit-only access: hits complete through the normal read-hit path; non-hit statuses NACK locally without L2 requests.
- Covered no-latency and L1 latency queue paths.
- Added NACK guard owner release after repeated non-owner NACKs.

## Build Result

- M3A checkpoint git diff --check: pass.
- M3A checkpoint source setup_environment release && make -j2: pass.
- Final git diff --check: pass.
- Final source setup_environment release && make -j2: pass.

## Smoke Result

Skipped. No obvious short benchmark environment was selected, and full benchmark suites are disallowed for M3.

## Grep and Config Checks

- New M3 knob grep: recorded in experiments/paper-mascar/m3_symbol_grep.txt.
- Required paper_mascar_m3 stats grep: recorded in experiments/paper-mascar/m3_symbol_grep.txt.
- M3A config active hit-only: 0.
- M3B config active hit-only: 1.
- M3A old proxy scheduling: 0.
- M3B old proxy scheduling: 0.

## Warnings and Limitations

- M3 does not implement re-execution queue, request recycling queue, or one-memory-instruction-per-warp re-exec enforcement.
- Active M3 NACK uses existing local retry behavior; M4 is still needed to avoid LSU head-of-line blocking from repeated NACKs.
- Smoke was skipped, so runtime counter behavior has not been exercised by a workload in this round.
