# Mascar M1 Postcheck

## Timing

- start_iso: 2026-06-04T16:46:07+08:00
- end_iso: 2026-06-04T16:51:01+08:00
- elapsed_sec: 294
- Timing command requirement: start was captured with `start_ts=$(date +%s)`; end was captured with `end_ts=$(date +%s)`; elapsed was computed as `elapsed_sec=$((end_ts - start_ts))`.

## Branch

- current_branch: hrl/paper/mascar-repro-v0
- HEAD: 8b8abe3d10f4
- upstream: origin/hrl/paper/mascar-repro-v0

## Git Status

Initial status:

```text
clean
```

Final status before review-pack creation:

```text
 M src/gpgpu-sim/gpu-cache.h
 M src/gpgpu-sim/gpu-sim.cc
 M src/gpgpu-sim/shader.cc
 M src/gpgpu-sim/shader.h
?? configs/hrl-repro/SM7_QV100_mascar_l1sat_probe_on/
?? docs/papers/mascar_m1_l1_saturation_probe.md
?? experiments/paper-mascar/m1_diff_name_status.txt
?? experiments/paper-mascar/m1_postcheck.md
?? experiments/paper-mascar/m1_symbol_grep.txt
```

## Changed Files

- src/gpgpu-sim/gpu-cache.h
- src/gpgpu-sim/gpu-sim.cc
- src/gpgpu-sim/shader.cc
- src/gpgpu-sim/shader.h
- configs/hrl-repro/SM7_QV100_mascar_l1sat_probe_on/README.md
- configs/hrl-repro/SM7_QV100_mascar_l1sat_probe_on/config_volta_islip.icnt
- configs/hrl-repro/SM7_QV100_mascar_l1sat_probe_on/gpgpusim.config
- docs/papers/mascar_m1_l1_saturation_probe.md
- experiments/paper-mascar/m1_diff_name_status.txt
- experiments/paper-mascar/m1_postcheck.md
- experiments/paper-mascar/m1_symbol_grep.txt

## Build And Checks

Commands run:

```bash
source setup_environment release && make -j2
git diff --check
rg -n -- "-gpgpu_enable_mascar|gpgpu_mascar_enable_l1_saturation_probe|gpgpu_mascar_l1_saturation_margin" src/gpgpu-sim/gpu-sim.cc
rg -n -- "-gpgpu_enable_mascar|-gpgpu_mascar_enable_telemetry|-gpgpu_mascar_enable_would_deprioritize|-gpgpu_mascar_enable_scheduling|-gpgpu_mascar_enable_l1_saturation_probe|-gpgpu_mascar_l1_saturation_margin" configs/hrl-repro/SM7_QV100_mascar_l1sat_probe_on/gpgpusim.config
rg -n "owner_warp|WST|WRC|reexec|re-exec|miss-NACK|hit-only" src/gpgpu-sim/gpu-cache.h src/gpgpu-sim/gpu-sim.cc src/gpgpu-sim/shader.cc src/gpgpu-sim/shader.h || true
```

Results:

- Build passed.
- `git diff --check` passed.
- New probe knob defaults to `0`; L1 saturation margin defaults to `1`.
- `gpgpu_enable_mascar` remains default `0`.
- New passive config has `-gpgpu_mascar_enable_scheduling 0`.
- New passive config has `-gpgpu_mascar_enable_would_deprioritize 0`.
- No new simulator `owner_warp`, WST/WRC, re-exec, miss-NACK, or hit-only symbols were found.

## Smoke

- Smoke run performed: no.
- Reason: the obvious short-test scripts require external workload/config environment variables such as `CONFIG` and `GPUAPPS_ROOT`, and they launch benchmark infrastructure. M1 only required a compile check and explicitly forbids full benchmark runs.
- Printed stats grep result: not applicable because no smoke run was performed.

## Review Pack

- /workspace/tmp/mascar_m1_review_pack_20260604_165101.tar.gz

## Warnings

- M1 adds passive telemetry and stats only. It does not implement EP/MP mode, owner warp, WST/WRC, non-owner hit-only / miss-NACK, or re-execution queue.
- The build emitted existing warning classes, including reorder and signedness warnings, but completed successfully.
- No benchmark validation was run in this round.
