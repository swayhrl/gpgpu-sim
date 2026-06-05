# W26 Known Limitations

- W25 actual logs did not expose true power/energy fields, so energy is unavailable for the integrated sweep.
- Current-simulator QV100-oriented runs are not the paper's GPGPU-Sim v3.2.2 / GTX480 / GPUWattch environment.
- W24 Table III results are app-level/inferred-order where applicable, not strict per-kernel paper phase alignment.
- `completed_no_explicit_pass` is not a correctness pass.
- Remaining unavailable workload rows must stay represented in manifests until source, data, build, wrapper, and phase blockers are resolved.
