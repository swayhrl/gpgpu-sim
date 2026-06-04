# SM7_QV100_mascar_m2_owner_sched_on

Mascar M2B active MP owner-warp scheduling config.

- `gpgpu_enable_mascar=1`
- `gpgpu_mascar_enable_l1_saturation_probe=1`
- `gpgpu_mascar_l1_saturation_margin=1`
- `gpgpu_mascar_enable_mp_owner_telemetry=1`
- `gpgpu_mascar_enable_mp_owner_scheduling=1`
- `gpgpu_mascar_enable_scheduling=0`
- `gpgpu_mascar_enable_would_deprioritize=0`
- `gpgpu_mascar_m2_compute_first=1`
- `gpgpu_mascar_l1_saturation_recent_window=8`
- `gpgpu_mascar_owner_max_hold_cycles=256`
- `gpgpu_mascar_owner_no_progress_limit=64`

Active behavior is limited to MP mode under the new owner-scheduling knob:
compute-ready warps are considered before memory-ready warps, owner memory
warps are preferred, and non-owner memory warps are blocked at the scheduler
level. It does not enable the old proxy scheduling path and does not change
cache access return behavior.
