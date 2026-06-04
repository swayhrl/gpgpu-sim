# M5A Runtime Sanity

Date: 2026-06-04

## Initial Checks

- Branch: `hrl/paper/mascar-repro-v0`
- HEAD: `2cf8b33`
- `git diff --check`: passed.
- `source setup_environment release && make -j2`: passed.

## Config Sanity

All required configs exist:

- `configs/hrl-repro/SM7_QV100_mascar_l1sat_probe_on/`
- `configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/`
- `configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/`
- `configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/`
- `configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on/`
- `configs/hrl-repro/SM7_QV100_mascar_m4_reexec_probe_on/`
- `configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on/`

Active configs keep old proxy scheduling off:

- `-gpgpu_mascar_enable_scheduling 0`
- `-gpgpu_mascar_enable_would_deprioritize 0`

M4 active config has:

- `-gpgpu_mascar_enable_reexec_queue 1`
- `-gpgpu_mascar_reexec_loads_only 1`

## Smoke Commands Attempted

Workload: `rodinia_hotspot`

Each run used:

```bash
cd /workspace/repos/gpgpu-workloads
TIMEOUT_SECONDS=1200 \
GPGPUSIM_CONFIG_OVERRIDE=/workspace/repos/gpgpu-sim_distribution/configs/hrl-repro/<config> \
bash scripts/run_one.sh rodinia_hotspot
```

Configs:

- `SM7_QV100_mascar_baseline_off`
- `SM7_QV100_mascar_m2_owner_sched_on`
- `SM7_QV100_mascar_m3_hitonly_nack_on`
- `SM7_QV100_mascar_m4_reexec_load_on`

## Smoke Results

| config | exit | cycles | ipc | selected Mascar signal |
|---|---:|---:|---:|---|
| baseline_off | 0 | 6931 | 133.3510 | Mascar stats zero |
| m2_owner_sched | 0 | 6931 | 133.3510 | `l1_sat_sample=3095`, `m2_mp_cycles=495` |
| m3_hitonly_nack | 0 | 6931 | 133.3510 | M3 active enabled but `hitonly_access_attempt=0` on this short workload |
| m4_reexec_load | 0 | 6918 | 133.6016 | `m4_enqueue_success=53`, `m4_retry_attempt=440` |

Logs:

- `experiments/paper-mascar/m5_runtime_logs/baseline_off_rodinia_hotspot.log`
- `experiments/paper-mascar/m5_runtime_logs/m2_owner_sched_rodinia_hotspot.log`
- `experiments/paper-mascar/m5_runtime_logs/m3_hitonly_nack_rodinia_hotspot.log`
- `experiments/paper-mascar/m5_runtime_logs/m4_reexec_load_rodinia_hotspot.log`

## Bugs Found And Fixes Made

No runtime build/config/assertion/deadlock bug was found during M5A smoke.

## Continue To M5B

Safe to continue. Runtime exists and active M4 completed on the short workload.
The M3 non-owner hit-only path did not trigger on `rodinia_hotspot`; this is a
workload coverage limitation, not a smoke failure.

## Limitations

- This is a focused smoke, not a full Rodinia/Parboil sweep.
- The config is SM7/QV100-style, not paper GTX480/Fermi.
- Results are runtime sanity data only and are not paper-comparable speedup
  claims.

