# Kernel Trace Extension

Purpose: provide a generic, default-off kernel launch trace for paper reproduction workflows that need launch order/name evidence for phase mapping.

Config knobs:

- `-gpgpu_paperrepro_kernel_trace 0|1`, default `0`.
- `-gpgpu_paperrepro_kernel_trace_max <N>`, default `4096`.
- `-gpgpu_paperrepro_kernel_trace_stats 0|1`, default `0`.

Trace lines:

```text
paperrepro_kernel_begin launch_index=<N> uid=<UID> name=<NAME> grid=(X,Y,Z) block=(X,Y,Z) stream=<S> cycle=<C>
paperrepro_kernel_end launch_index=<N> uid=<UID> name=<NAME> stream=<S> cycle=<C>
```

Default-off guarantee: when `gpgpu_paperrepro_kernel_trace=0`, no trace line is emitted and default simulator behavior is unchanged.

Parser:

```bash
python3 experiments/common/gpgpusim_matrix/collect_kernel_trace.py <run_dir_or_log_dir>
```

Caveats: trace output is bounded by `gpgpu_paperrepro_kernel_trace_max`; phase mappings should remain `inferred_order` unless independent evidence supports exact alignment.
