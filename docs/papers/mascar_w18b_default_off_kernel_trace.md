# Mascar W18B Default-Off Kernel Trace

W18B added bounded, default-off simulator trace knobs:

- gpgpu_paperrepro_kernel_trace, default 0
- gpgpu_paperrepro_kernel_trace_max, default 128
- gpgpu_paperrepro_kernel_trace_stats, default 1

Structured lines:

- paperrepro_kernel_begin launch_index=... uid=... name=... grid=(...) block=(...) stream=... cycle=...
- paperrepro_kernel_end launch_index=... uid=... name=... stream=... cycle=...

Implementation scope:

- src/gpgpu-sim/gpu-sim.h declares knobs and trace state/API.
- src/gpgpu-sim/gpu-sim.cc registers knobs and emits begin/end lines from accepted kernel launch and kernel-done paths.
- configs/hrl-repro/SM7_QV100_mascar_kernel_trace_baseline_off enables trace for W18 diagnostic runs while keeping Mascar disabled.

Build status: source setup_environment release && make -j2 passed.
