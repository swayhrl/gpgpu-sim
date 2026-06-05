# Mascar W17B Guidance: Build and Binary Recovery

## Stage position

This is W17B.

W17A identified missing binaries, candidate source directories, data directories, and possible build scripts. W17B attempts to build or recover binaries for as many Table III rows as possible.

## Goal

Increase ready rows by building missing binaries or locating existing binaries.

W17B should be bold but controlled:
- try obvious local builds
- fix simple Makefile/CUDA arch issues
- keep logs
- avoid destructive changes
- do not download external sources/data

## Hard constraints

1. Do not modify M1-M4 Mascar mechanism code.
2. Do not fetch upstream or external sources.
3. Do not run full benchmark suites.
4. Build only local candidate workloads.
5. Use timeout for every build.
6. Do not commit binary outputs unless they are already intended tracked source artifacts. Prefer not to commit binaries.
7. Build outputs in external workload repo are acceptable if needed, but document them.
8. Build outputs inside gpgpu-sim_distribution should not be committed.
9. If build creates large logs, archive them to /workspace/tmp.
10. Try at least two approaches for high-priority missing_binary workloads before marking unresolved, when source exists.

## Inputs

Read:

- experiments/paper-mascar/workloads/matrix/W17/w17a_gap_matrix.csv
- experiments/paper-mascar/workloads/matrix/W17/w17a_next_action_plan.csv
- experiments/paper-mascar/workloads/matrix/W17/w17a_candidate_inventory.csv

## Build strategy

For each high-priority candidate:

### Attempt 1: native existing build

If Makefile exists:

- run `make clean` only if safe and local
- run `timeout <N> make`
- capture log

If build.sh exists:

- run `timeout <N> bash build.sh`

If CMake exists:

- use out-of-source build directory under /workspace/tmp if possible

Default build timeout:

- 1200 seconds per workload
- may increase to 2400 seconds for complex workloads

### Attempt 2: CUDA arch / compiler adjustment

If build fails due CUDA arch:

- inspect existing GPGPU-Sim/benchmark compile flags
- try adding or adjusting:
  - -arch=sm_XX matching current config
  - -gencode if already used elsewhere
- do not rewrite build system broadly

If build fails due old CUDA syntax/API:

- document unsupported_env
- do not spend unlimited time

### Attempt 3: binary recovery

Search for compiled binaries under:

- /workspace/repos/gpgpu-workloads
- /workspace/repos/gpgpu-sim_simulations
- /workspace/repos

If binary exists and runs help/no-arg safely, mark binary_found.

### Attempt 4: data check

If binary exists but data missing:

- search local data dirs
- if data missing, mark missing_data
- do not download

## Required build helper

Create:

- experiments/paper-mascar/workloads/matrix/W17/w17b_build_missing_workloads.sh

Requirements:

- bash
- reads W17A next action plan
- supports env vars:
  - W17_BUILD_TIMEOUT_SEC default 1200
  - W17_MAX_BUILDS default 0 no cap
  - W17_DRY_RUN default 0
  - W17_ONLY_PAPER_ID optional
  - W17_LOG_DIR default experiments/paper-mascar/workloads/results/W17/build_logs/<timestamp>
- continues after build failures
- writes build_results.csv
- never uses git add .
- prints summary

## Required outputs

Create:

- experiments/paper-mascar/workloads/matrix/W17/w17b_build_missing_workloads.sh
- experiments/paper-mascar/workloads/results/W17/w17b_build_results.csv
- experiments/paper-mascar/workloads/results/W17/w17b_binary_recovery_results.csv
- experiments/paper-mascar/workloads/results/W17/w17b_data_availability_results.csv
- docs/papers/mascar_w17b_build_binary_recovery.md
- experiments/paper-mascar/workloads/audit/W17/w17b_postcheck.md

## Build results columns

w17b_build_results.csv columns:

- paper_id
- source_path
- build_command
- build_attempt
- exit_code
- timeout
- binary_path
- build_status
- log_path
- notes

build_status values:

- built
- already_built
- failed_compile
- failed_missing_dependency
- failed_unsupported_cuda
- failed_timeout
- skipped_no_source
- skipped_no_build_command
- unknown

## Report requirements

docs/papers/mascar_w17b_build_binary_recovery.md must include:

1. Goal
2. Build candidates attempted
3. Binaries recovered
4. Build failures and reasons
5. Missing data issues
6. Updated ready candidates
7. Workloads still blocked
8. Recommendations for W17C wrappers and W18 phase trace

## Validation

Run:

- bash -n experiments/paper-mascar/workloads/matrix/W17/w17b_build_missing_workloads.sh
- W17_DRY_RUN=1 bash experiments/paper-mascar/workloads/matrix/W17/w17b_build_missing_workloads.sh
- actual build for selected candidates
- Check build_results.csv exists
- Check report exists

## Stop conditions

Stop W17B only if:

1. build helper is broken and cannot be fixed.
2. build process corrupts repo state.
3. repeated builds cause environment instability.
4. elapsed time exceeds agreed W17 budget.

Do not stop because individual workloads fail to build. Record the failure and continue.
