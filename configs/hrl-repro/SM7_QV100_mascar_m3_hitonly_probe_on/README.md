# SM7_QV100_mascar_m3_hitonly_probe_on

Mascar M3A passive non-owner L1 hit-only probe config.

- `gpgpu_enable_mascar=1`
- `gpgpu_mascar_enable_l1_saturation_probe=1`
- `gpgpu_mascar_l1_saturation_margin=1`
- `gpgpu_mascar_enable_mp_owner_telemetry=1`
- `gpgpu_mascar_enable_mp_owner_scheduling=0`
- `gpgpu_mascar_enable_nonowner_hit_only_probe=1`
- `gpgpu_mascar_enable_nonowner_hit_only=0`
- `gpgpu_mascar_nonowner_hit_only_loads_only=1`
- `gpgpu_mascar_enable_scheduling=0`
- `gpgpu_mascar_enable_would_deprioritize=0`
- `gpgpu_mascar_m2_compute_first=1`
- `gpgpu_mascar_l1_saturation_recent_window=8`
- `gpgpu_mascar_owner_max_hold_cycles=256`
- `gpgpu_mascar_owner_no_progress_limit=64`

No scheduling or cache behavior change: this config samples would-hit /
would-NACK opportunities for non-owner loads in MP mode. Active hit-only /
miss-NACK remains off.
