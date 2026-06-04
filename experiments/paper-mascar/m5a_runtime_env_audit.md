# M5A Runtime Environment Audit

Date: 2026-06-04

## Summary

A runnable short workload environment exists under `/workspace/repos/gpgpu-workloads`.
The runner supports `GPGPUSIM_CONFIG_OVERRIDE`, which lets M5 inject the Mascar
configs without editing benchmark directories.

## Candidate Runtime Commands Found

- `/workspace/repos/gpgpu-workloads/scripts/run_one.sh`
- `/workspace/repos/gpgpu-workloads/scripts/run_workload_set.sh`
- `/workspace/repos/gpgpu-workloads/scripts/list_workloads.sh`
- `/workspace/repos/accel-sim-framework/util/job_launching/run_simulations.py`
- `/workspace/repos/accel-sim-framework/util/job_launching/monitor_func_test.py`

The Accel-Sim job launcher exists, but M5 used the lighter
`gpgpu-workloads/scripts/run_one.sh` path to avoid full suites.

## Workload Availability

`bash /workspace/repos/gpgpu-workloads/scripts/list_workloads.sh` lists ready
short workloads including:

- `vecadd`
- `strided_access`
- `page_stride_access`
- `atomic_contention`
- `polybench_2dconv`
- `rodinia_hotspot`
- other Rodinia/Parboil wrappers

M5 selected `rodinia_hotspot` because prior local notes classified it as a quick
workload and the M5A smoke confirmed it completes quickly.

## Required Environment

The workload runner sources its manifest-provided environment script. Repository
notes also identify:

- `CUDA_INSTALL_PATH=/usr/local/cuda-11.8`
- GPGPU-Sim setup via `source /workspace/repos/gpgpu-sim_distribution/setup_environment release`
- Config injection via
  `GPGPUSIM_CONFIG_OVERRIDE=/workspace/repos/gpgpu-sim_distribution/configs/hrl-repro/<config>`
- Per-run timeout via `TIMEOUT_SECONDS=<seconds>`

## Smoke Command Shape

```bash
cd /workspace/repos/gpgpu-workloads
TIMEOUT_SECONDS=1200 \
GPGPUSIM_CONFIG_OVERRIDE=/workspace/repos/gpgpu-sim_distribution/configs/hrl-repro/<config> \
bash scripts/run_one.sh rodinia_hotspot
```

## Runtime Availability Decision

Runtime is available. M5A ran `rodinia_hotspot` under baseline, M2, M3, and M4
active configs with timeout. All completed with exit code 0.

