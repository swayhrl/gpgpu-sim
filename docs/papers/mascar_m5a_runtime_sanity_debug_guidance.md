# Mascar M5A Guidance: Runtime Sanity, Smoke Discovery, and Debug

## Stage position

This is M5A of the Mascar reproduction.

Implemented so far:
- M1: L1 saturation probe.
- M2: EP/MP owner-warp scheduling.
- M3: non-owner L1 hit-only / miss-NACK.
- M4: load-only re-execution queue.

M5A must verify that the active mechanisms can run, or clearly determine that runtime benchmark environment is missing. If an active run fails due to code/config bugs, debug and fix inside this round. Do not stop just because a smoke fails.

M5B will build and run the focused validation matrix.
M6 will write the final report based on M5 results.

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not run full benchmark suites.
4. Do not claim performance results if no workload actually ran.
5. If runtime fails due to a mechanism bug, debug and fix.
6. If runtime cannot run because no benchmark environment exists, document that and continue to create reusable scripts and a limited closeout report.
7. Keep all new behavior baseline-safe.
8. Do not commit M5/M6 outputs.
9. Do not use git add . or git add -A.
10. Use start_ts/end_ts in postcheck.

## Required initial checks

Run and record:

- git status --short
- git branch --show-current
- git log -1 --oneline --decorate
- git diff --check
- source setup_environment release && make -j2

If build fails:
- debug and fix the build.
- rerun build.
- document root cause in experiments/paper-mascar/m5a_runtime_sanity.md.

## Config sanity

Verify these configs exist:

- configs/hrl-repro/SM7_QV100_mascar_l1sat_probe_on/
- configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/
- configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/
- configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/
- configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on/
- configs/hrl-repro/SM7_QV100_mascar_m4_reexec_probe_on/
- configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on/

Verify active configs keep old proxy scheduling off:

- -gpgpu_mascar_enable_scheduling 0
- -gpgpu_mascar_enable_would_deprioritize 0

Verify M4 active config has:

- -gpgpu_mascar_enable_reexec_queue 1
- -gpgpu_mascar_reexec_loads_only 1

## Runtime environment discovery

Search for existing smoke or benchmark runners. Spend at most 20 minutes.

Search locations:

- tools/
- experiments/
- configs/hrl-repro/
- docs/
- README.md
- CLAUDE.md
- /workspace/repos/
- /workspace/

Search terms:

- run_simulations.py
- monitor_func_test.py
- rodinia
- parboil
- smoke
- benchmark
- gpgpusim.config
- CONFIG=
- GPGPUSIM_ROOT
- CUDA_INSTALL_PATH

Create:

- experiments/paper-mascar/m5a_runtime_env_audit.md

It must include:
- whether a runnable workload exists
- candidate commands found
- required env variables
- whether benchmark binaries/data are present
- reason if runtime is unavailable

## Smoke attempt

If a short self-contained workload command is found, run at most one or two tiny workloads with timeout.

Preferred configs:

1. baseline/off config:
   choose a config with gpgpu_enable_mascar 0 if available.
2. M2 active:
   SM7_QV100_mascar_m2_owner_sched_on
3. M3 active:
   SM7_QV100_mascar_m3_hitonly_nack_on
4. M4 active:
   SM7_QV100_mascar_m4_reexec_load_on

Use timeout:
- timeout 20m for each run.

Capture output logs under:
- experiments/paper-mascar/m5_runtime_logs/

For each completed run, grep stats:
- gpu_tot_sim_cycle
- gpu_tot_ipc
- paper_mascar_l1_sat_
- paper_mascar_m2_
- paper_mascar_m3_
- paper_mascar_m4_
- L1D or cache hit stats if present

If a run fails:
- inspect log tail
- identify whether failure is config, runtime env, assertion, deadlock, or simulator bug
- fix code/config if bug is in M1-M4 implementation
- rerun build and the failed tiny run if possible
- record fix and evidence

If a run times out:
- inspect whether it is a benchmark size issue or a deadlock
- if likely deadlock in M2/M3/M4, debug and fix
- if benchmark naturally too long, reduce workload only if obvious
- otherwise record as timeout and do not claim success

If no workload can run:
- do not fabricate stats
- proceed to M5B by creating reusable validation scripts and placeholders
- M6 must be a mechanism closeout report rather than performance final report

## Required M5A output

Create:

- experiments/paper-mascar/m5a_runtime_sanity.md
- experiments/paper-mascar/m5a_runtime_env_audit.md

m5a_runtime_sanity.md must include:

1. build status
2. config sanity status
3. smoke commands attempted
4. smoke results
5. bugs found
6. fixes made
7. whether it is safe to continue to M5B
8. limitations

## Stop conditions

Stop only if:

1. build cannot be restored.
2. code enters a broad redesign outside M5/M6.
3. active M4 has a severe ownership/lifetime bug requiring M4 rewrite.
4. elapsed time already exceeds 150 minutes.

Do not stop merely because smoke fails. Debug it.
