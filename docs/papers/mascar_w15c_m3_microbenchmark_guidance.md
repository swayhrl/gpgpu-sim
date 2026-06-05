# Mascar W15C Guidance: M3 Diagnostic Microbenchmark

## Stage position

W15C constructs or uses a diagnostic microbenchmark to determine whether M3 implementation is functional independent of Table III inputs.

## Goal

Create a small workload that intentionally creates:
1. MP mode / memory pressure
2. owner warp
3. non-owner load in MP
4. L1 reuse opportunity
5. hit-only probe call

If microbenchmark triggers M3:
- implementation likely works
- Table III inputs are insufficient

If microbenchmark does not trigger M3:
- implementation or gating likely has a bug

## Microbenchmark design

Create under:

- experiments/paper-mascar/workloads/micro/mascar_m3_diag/

Files:
- mascar_m3_diag.cu
- Makefile or build.sh
- README.md
- run_m3_diag_wrapper.sh

Kernel idea:

- Use two arrays:
  - hot array: small, fits in L1, read repeatedly by many warps
  - cold array: large, stride/random-like, used to create miss pressure

Pattern:
1. warm hot array
2. each warp alternates cold streaming loads and hot reuse loads
3. use enough blocks/warps to create MP mode
4. use volatile or output accumulation to prevent optimization

Pseudo behavior:
- for iter in outer loop:
    read cold[large_strided_index]
    read hot[(lane_id + small_offset) & hot_mask]
    accumulate
    repeat many times

Input knobs:
- num_elements_cold
- num_elements_hot
- outer_iters
- blocks
- threads_per_block
- stride

Default diagnostic should be bounded:
- threads_per_block 256
- blocks enough to fill SMs
- cold large enough to exceed L1
- hot small enough to remain L1-resident

## Build/run policy

Try to build using the same workload build environment used by existing GPGPU workloads.

If nvcc is unavailable:
- document unavailable
- skip build
- W15D should conclude microbenchmark not run

If build succeeds:
- create wrapper compatible with matrix runner:
  - supports --dry-run
  - supports --print-command
  - supports --help
  - actual run exits 0/77 appropriately

Add to extra smoke manifest, not Table III manifest:
- experiments/paper-mascar/workloads/micro/mascar_m3_diag/manifest.csv

## Diagnostic configs

Run microbenchmark with:
1. m3diag_on
2. m3diag_forced_mp_on
3. m4_reexec_load

If m3diag_forced_mp_on still shows skip_not_mp, config path is wrong.

## Up to five strategies

W15C should try up to five strategies if M3 remains zero:

Strategy 1:
- normal microbenchmark hot/cold arrays, m3diag_on

Strategy 2:
- forced-MP diagnostic config

Strategy 3:
- increase blocks and outer_iters

Strategy 4:
- reduce hot array size and increase hot reuse loop

Strategy 5:
- reduce cold stride coalescing or increase cold working set to produce more pressure

Each strategy must record:
- command
- config
- counters
- diagnosis

## Required outputs

- experiments/paper-mascar/workloads/micro/mascar_m3_diag/
- experiments/paper-mascar/workloads/matrix/W15C/w15c_micro_config_matrix.csv
- experiments/paper-mascar/workloads/matrix/W15C/w15c_micro_workload_manifest.csv
- experiments/paper-mascar/workloads/results/W15C/
- experiments/paper-mascar/workloads/matrix/W15C/w15c_m3_micro_summary.csv
- docs/papers/mascar_w15c_m3_microbenchmark_report.md
- experiments/paper-mascar/workloads/audit/W15/w15c_postcheck.md

## If an implementation bug is found

If diagnostics strongly show M3 logic is wrong, for example:
- non-owner load candidate exists
- M3 active config on
- but scheduler blocks it before LSU
- or hit-only path never called due wrong condition

Then Codex may apply a localized M3 fix.

Rules:
- keep default baseline safe
- preserve config gating
- document exact bug and fix
- rerun build
- rerun diagnostic workload
- do not broadly rewrite M1-M4

## Stop conditions

Stop only if:
1. build cannot be restored.
2. microbenchmark cannot be built and Table III diagnostics are also inconclusive.
3. fixing M3 requires broad M1-M4 redesign.
4. elapsed time exceeds 180 minutes.

Do not stop because the first microbenchmark input fails. Try all strategies.
