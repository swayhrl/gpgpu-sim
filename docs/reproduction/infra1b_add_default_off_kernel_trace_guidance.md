# INFRA-1B Guidance: Add Default-Off Kernel Trace Branch

## Stage position

This is INFRA-1B.

Goal:
Create a second infra branch with default-off kernel launch trace support.

Target branch:
  hrl/infra/paper-repro-framework-trace-v0

Base branch:
  hrl/infra/paper-repro-framework-v0

Source branch for reference:
  hrl/paper/mascar-repro-v0

Important:
Do not directly checkout whole src/gpgpu-sim/gpu-sim.cc or gpu-sim.h from the Mascar branch. Those files may contain Mascar-specific code. Port only default-off paperrepro kernel trace changes.

## Hard constraints

1. Start from hrl/infra/paper-repro-framework-v0 local commit.
2. Do not force-overwrite existing trace branch.
3. Do not include Mascar mechanism code.
4. Do not include gpgpu_mascar_* options.
5. New simulator source changes must be default-off.
6. Trace must not change default behavior when disabled.
7. Do not include Mascar configs or experiment results.
8. Do not use git add . or git add -A.
9. Local commit is allowed and expected.
10. Do not push.

## Branch creation

If base infra branch exists:

  git checkout hrl/infra/paper-repro-framework-v0
  git checkout -b hrl/infra/paper-repro-framework-trace-v0

If trace branch exists:
- stop and report
- do not overwrite

## Identify trace patch from Mascar branch

Search Mascar branch:

  git grep -n "gpgpu_paperrepro_kernel_trace\\|paperrepro_kernel" hrl/paper/mascar-repro-v0 -- src/gpgpu-sim src/cuda-sim configs docs experiments 2>/dev/null

Inspect relevant snippets using:

  git show hrl/paper/mascar-repro-v0:src/gpgpu-sim/gpu-sim.cc
  git show hrl/paper/mascar-repro-v0:src/gpgpu-sim/gpu-sim.h

Port only symbols related to:

  gpgpu_paperrepro_kernel_trace
  gpgpu_paperrepro_kernel_trace_max
  gpgpu_paperrepro_kernel_trace_stats
  paperrepro_kernel_begin
  paperrepro_kernel_end

Do not port:
- gpgpu_mascar_*
- paper_mascar_*
- M1/M2/M3/M4 stats
- M3 diagnostic
- Mascar configs

## Expected trace behavior

Add config knobs:

  gpgpu_paperrepro_kernel_trace
  gpgpu_paperrepro_kernel_trace_max
  gpgpu_paperrepro_kernel_trace_stats

Defaults:

  gpgpu_paperrepro_kernel_trace = 0
  gpgpu_paperrepro_kernel_trace_max = 4096
  gpgpu_paperrepro_kernel_trace_stats = 0

Trace output format:

  paperrepro_kernel_begin launch_index=<N> uid=<UID> name=<NAME> grid=(X,Y,Z) block=(X,Y,Z) stream=<S> cycle=<C>
  paperrepro_kernel_end launch_index=<N> uid=<UID> name=<NAME> cycle=<C>

If end trace is hard to port safely, begin trace is acceptable, but document limitation.

## Trace config

Create a generic trace config if a suitable baseline config exists:

  configs/hrl-repro/SM7_QV100_paperrepro_kernel_trace_baseline_off/

Do not use a Mascar config name.

Base it on an existing non-Mascar baseline config if available. Add only:

  -gpgpu_paperrepro_kernel_trace 1
  -gpgpu_paperrepro_kernel_trace_max 4096
  -gpgpu_paperrepro_kernel_trace_stats 1

If no suitable config exists, create docs explaining how to enable trace instead of inventing a broken config.

## Documentation

Create:

  docs/reproduction/KERNEL_TRACE_EXTENSION.md
  docs/reproduction/TRACE_BRANCH_POLICY.md
  docs/reproduction/INFRA1B_TRACE_POSTCHECK.md

KERNEL_TRACE_EXTENSION.md must include:
- purpose
- config knobs
- trace line format
- default-off guarantee
- parser command
- caveats

TRACE_BRANCH_POLICY.md must include:
- trace branch is optional
- use it only when paper needs kernel/phase mapping
- performance runs should keep trace disabled unless explicitly needed

## Validation

Run:

  git diff --check
  source setup_environment release && make -j2
  python3 -m py_compile experiments/common/gpgpusim_matrix/collect_kernel_trace.py

Check source diff contains only paperrepro trace, not Mascar:

  git diff hrl/infra/paper-repro-framework-v0 -- src/gpgpu-sim/gpu-sim.cc src/gpgpu-sim/gpu-sim.h | grep -i mascar && stop/report

Check option grep:

  grep -RIn "gpgpu_paperrepro_kernel_trace\\|paperrepro_kernel" src/gpgpu-sim configs docs/reproduction experiments/common

Check no Mascar config was added:

  find configs -type f 2>/dev/null | grep -i mascar && stop/report

## Review pack

Create:

  /workspace/tmp/infra1b_paper_repro_trace_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- source diffs
- trace config if created
- docs/reproduction/KERNEL_TRACE_EXTENSION.md
- docs/reproduction/TRACE_BRANCH_POLICY.md
- postcheck
- full diff against base infra branch

## Commit

If validation passes:

  git add <explicit trace source/config/docs files>
  git commit -m "infra: add default-off kernel launch trace"

Do not push.

Report:
- branch name
- local commit hash
- review pack path
