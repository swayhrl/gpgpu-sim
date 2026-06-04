# SM7_QV100_mascar_m3_hitonly_nack_on

Mascar M3B active non-owner L1 hit-only / NACK config.

- `gpgpu_enable_mascar=1`
- `gpgpu_mascar_enable_l1_saturation_probe=1`
- `gpgpu_mascar_l1_saturation_margin=1`
- `gpgpu_mascar_enable_mp_owner_telemetry=1`
- `gpgpu_mascar_enable_mp_owner_scheduling=1`
- `gpgpu_mascar_enable_nonowner_hit_only_probe=1`
- `gpgpu_mascar_enable_nonowner_hit_only=1`
- `gpgpu_mascar_nonowner_hit_only_loads_only=1`
- `gpgpu_mascar_nonowner_nack_release_threshold=64`
- `gpgpu_mascar_enable_scheduling=0`
- `gpgpu_mascar_enable_would_deprioritize=0`
- `gpgpu_mascar_m2_compute_first=1`
- `gpgpu_mascar_l1_saturation_recent_window=8`
- `gpgpu_mascar_owner_max_hold_cycles=256`
- `gpgpu_mascar_owner_no_progress_limit=64`

Active behavior is limited to MP mode under the M2 owner-scheduling knob and
the M3 hit-only knob. Non-owner loads may reach L1D and complete only on L1 hit;
misses and reserved statuses are NACKed locally without L2 requests. Stores,
atomics, and non-load memory ops remain blocked by the M2 scheduler gate. This
config does not implement a re-execution queue and does not enable the old proxy
scheduling path.
