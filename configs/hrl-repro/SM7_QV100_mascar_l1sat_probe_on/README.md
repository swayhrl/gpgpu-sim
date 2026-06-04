# SM7_QV100_mascar_l1sat_probe_on

Mascar M1 passive L1D saturation telemetry config.

- `gpgpu_enable_mascar=1`
- `gpgpu_mascar_enable_l1_saturation_probe=1`
- `gpgpu_mascar_l1_saturation_margin=1`
- `gpgpu_mascar_enable_scheduling=0`
- `gpgpu_mascar_enable_would_deprioritize=0`

No scheduling behavior change: this config only samples L1D MSHR and miss queue pressure.
