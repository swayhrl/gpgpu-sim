# Mascar M4S Detailed Guidance: M2/M3 Active Sanity and Smoke Preflight

## Stage position

This is M4S, the preflight stage of M4.

M1 added L1 saturation probe.
M2 added EP/MP owner-warp scheduling.
M3 added non-owner L1 hit-only / miss-NACK behavior.
M4 will add the re-execution queue.

Before implementing M4A/M4B, run a focused sanity/smoke pass on the current M2/M3 active code. If a smoke or sanity check finds an M2/M3 bug, debug and fix it inside this M4 round. Do not stop merely because a smoke fails. Only stop for broad build breakage or time-limit exhaustion.

## Scope

M4S may fix M2/M3 runtime correctness bugs if found.
M4S must not start implementing re-execution queue yet.
M4S must record exactly what was tested and whether smoke was skipped due missing benchmark environment.

## Required preflight checks

1. Confirm branch:
   hrl/paper/mascar-repro-v0

2. Confirm worktree state at start:
   git status --short

3. Static checks:
   git diff --check
   source setup_environment release && make -j2

4. Config sanity:
   Verify these configs exist:
     configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/
     configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on/

   Verify M3 active config has:
     -gpgpu_enable_mascar 1
     -gpgpu_mascar_enable_l1_saturation_probe 1
     -gpgpu_mascar_enable_mp_owner_telemetry 1
     -gpgpu_mascar_enable_mp_owner_scheduling 1
     -gpgpu_mascar_enable_nonowner_hit_only 1
     -gpgpu_mascar_enable_scheduling 0
     -gpgpu_mascar_enable_would_deprioritize 0

5. Symbol sanity:
   Grep for:
     paper_mascar_m2_
     paper_mascar_m3_
     gpgpu_mascar_enable_mp_owner_scheduling
     gpgpu_mascar_enable_nonowner_hit_only

## Smoke guidance

Try to locate an existing short smoke runner or focused reproduction helper.

Search only within:
  tools/
  experiments/
  configs/hrl-repro/
  docs/papers/
  README.md
  CLAUDE.md

Spend at most 15 minutes searching for a runnable smoke command.

Preferred smoke, if obvious:
  Run one tiny workload or existing quick-set with:
    baseline/off or existing no-op config
    M2 owner active config
    M3 hit-only/NACK active config

Use timeout for each run if possible:
  timeout 20m <command>

Capture:
  command
  exit status
  tail of output or log path
  grep of paper_mascar_m2_ and paper_mascar_m3_ stats if available

If smoke fails:
  Debug and fix M2/M3 code/config issues within M4S.
  Re-run build and the failed smoke if practical.
  Record root cause and fix in experiments/paper-mascar/m4s_smoke_notes.md.

If smoke cannot be run because benchmark environment is missing or command is not obvious:
  Do not stop.
  Document the reason in m4s_smoke_notes.md and continue to M4A.

## Required M4S output

Create:
  experiments/paper-mascar/m4s_smoke_notes.md

Must include:
  start branch and HEAD
  build result
  config sanity result
  smoke command attempted or reason skipped
  failures found
  fixes made, if any
  whether it is safe to continue to M4A

## Stop conditions

Stop only if:
  build fails and cannot be fixed without broad unrelated changes
  M2/M3 active code has an obvious correctness bug that needs a major redesign beyond M4 scope
  elapsed time already exceeds 120 minutes
  repository state becomes unsafe or ambiguous

Do not stop just because smoke fails. Debug it.
