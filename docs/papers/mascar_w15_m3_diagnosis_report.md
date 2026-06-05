# Mascar W15 M3 Diagnosis Report

## Executive summary

M3 is functional and did activate in W15. Table III normal diagnostic inputs did not activate active M3, but forced-MP `srad_1`/`srad_2` and all five W15C microbenchmark strategies activated active non-owner hit-only access. The primary root cause for earlier non-activation is workload/input condition alignment, not an M3 implementation-path bug.

## Why M3 matters

M3 allows non-owner warps in MP mode to make hit-only L1D progress without taking ownership or sending new misses. This is the mechanism that can reduce memory pitstop idle time when reuse exists under pressure.

## M3 activation conditions

M3 requires Mascar enabled, MP mode, a valid owner, a non-owner warp, a supported load, an LSU/L1D access path, and a hit-only probe/access result. Missing any condition prevents active M3 counters from incrementing.

## W15A diagnostic counters added

W15A added `gpgpu_mascar_enable_m3_diagnostic`, `gpgpu_mascar_m3_diag_verbose`, `gpgpu_mascar_m3_diag_max_trace`, `paper_mascar_m3diag_*` counters, scheduler skip reasons, LSU/L1D visibility counters, two diagnostic configs, and collector support.

## W15B Table III diagnostic results

W15B ran 21 rows: 7 workloads across `m4_reexec_load`, `m3diag_on`, and `m3diag_forced_mp_on`. Normal `m3diag_on` did not activate active M3. Forced-MP activated active M3 on `srad_1` and `srad_2`; both had non-owner load candidates, scheduler allows, hit-only probe calls, and active hit-only NACKs.

## W15C microbenchmark results

W15C created `mascar_m3_diag`, fixed the micro build to dynamic CUDA runtime, and ran five smoke-sized strategies. All five completed and activated active M3 hit-only access, including normal `m3diag_on` strategy 1.

## Bug fixes made, if any

No M3 mechanism bug was found or fixed. A microbenchmark build/wrapper issue was fixed by using dynamic CUDA runtime (`-cudart shared`) so GPGPU-Sim can intercept the workload.

## Final diagnosis

M3 implementation path is reachable. Earlier M3 non-activation is primarily an input/config-condition issue: normal Table III tiny inputs usually do not simultaneously create sustained MP mode, valid owner, non-owner load candidate, and L1 reuse. Forced-MP and microbenchmark inputs produce those conditions and activate M3.

## Remaining limitations

Forced-MP results are diagnostic only and are not performance evidence. The W15C microbenchmark is synthetic and smoke-sized after the original larger parameters were too slow for bounded diagnostics.

## Recommendation for W16/W17

Use W15C-style hot/cold inputs or larger Table III inputs to study M3 behavior, then tune MP detection thresholds without relying on forced-MP for performance claims.
