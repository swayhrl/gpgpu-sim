# Mascar M4S Smoke Preflight Notes

Date: 2026-06-04

Branch: `hrl/paper/mascar-repro-v0`
HEAD at preflight: `123cc64`

## Static build

Command:

```bash
git diff --check && source setup_environment release && make -j2
```

Result: passed before M4 implementation. The build emitted existing warnings in
GPGPU-Sim/CUDA parser code but no errors.

## M2/M3 active smoke

I searched `tools`, `experiments`, `configs/hrl-repro`, and `docs/papers` for a
short smoke runner. The repo-local references point to an external workload
runner pattern:

```bash
GPGPUSIM_CONFIG_OVERRIDE=/workspace/repos/gpgpu-sim_distribution/configs/hrl-repro/<config_dir> bash scripts/run_one.sh <workload_name>
```

No self-contained short runner was obvious inside the repo. Full benchmarks are
disallowed for M4, so no workload smoke was run at M4S. No M2/M3 active failure
was observed during static build/config preflight.

