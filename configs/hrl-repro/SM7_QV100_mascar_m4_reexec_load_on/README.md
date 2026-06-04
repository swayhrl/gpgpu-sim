# SM7_QV100_mascar_m4_reexec_load_on

Mascar M4B active load re-execution queue config.

- `gpgpu_enable_mascar=1`
- `gpgpu_mascar_enable_l1_saturation_probe=1`
- `gpgpu_mascar_enable_mp_owner_telemetry=1`
- `gpgpu_mascar_enable_mp_owner_scheduling=1`
- `gpgpu_mascar_enable_nonowner_hit_only_probe=1`
- `gpgpu_mascar_enable_nonowner_hit_only=1`
- `gpgpu_mascar_enable_reexec_queue_probe=1`
- `gpgpu_mascar_enable_reexec_queue=1`
- `gpgpu_mascar_reexec_loads_only=1`
- `gpgpu_mascar_reexec_owner_takeover=1`
- `gpgpu_mascar_reexec_queue_size=32`
- `gpgpu_mascar_reexec_issue_per_cycle=1`
- `gpgpu_mascar_reexec_nack_rotate_limit=128`
- `gpgpu_mascar_enable_scheduling=0`
- `gpgpu_mascar_enable_would_deprioritize=0`

This config enables the M4 active load-only re-execution queue. Non-owner MP
retries still use the M3 hit-only/NACK path; owner, EP, or no-owner retries use
normal L1D access. The old proxy scheduling path remains off.
