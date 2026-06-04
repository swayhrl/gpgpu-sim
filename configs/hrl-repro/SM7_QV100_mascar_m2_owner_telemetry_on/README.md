# SM7_QV100_mascar_m2_owner_telemetry_on

Mascar M2A passive EP/MP and owner-warp telemetry config.

- `gpgpu_enable_mascar=1`
- `gpgpu_mascar_enable_l1_saturation_probe=1`
- `gpgpu_mascar_l1_saturation_margin=1`
- `gpgpu_mascar_enable_mp_owner_telemetry=1`
- `gpgpu_mascar_enable_mp_owner_scheduling=0`
- `gpgpu_mascar_enable_scheduling=0`
- `gpgpu_mascar_enable_would_deprioritize=0`
- `gpgpu_mascar_m2_compute_first=1`
- `gpgpu_mascar_l1_saturation_recent_window=8`
- `gpgpu_mascar_owner_max_hold_cycles=256`
- `gpgpu_mascar_owner_no_progress_limit=64`

No scheduling behavior change: this config only samples L1D pressure and records passive EP/MP owner-control-plane stats.
