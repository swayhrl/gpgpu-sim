# SM7_QV100_mascar_m4_reexec_probe_on

Mascar M4A passive load re-execution queue probe config.

- `gpgpu_enable_mascar=1`
- `gpgpu_mascar_enable_l1_saturation_probe=1`
- `gpgpu_mascar_enable_mp_owner_telemetry=1`
- `gpgpu_mascar_enable_mp_owner_scheduling=1`
- `gpgpu_mascar_enable_nonowner_hit_only_probe=1`
- `gpgpu_mascar_enable_nonowner_hit_only=1`
- `gpgpu_mascar_enable_reexec_queue_probe=1`
- `gpgpu_mascar_enable_reexec_queue=0`
- `gpgpu_mascar_reexec_loads_only=1`
- `gpgpu_mascar_reexec_queue_size=32`
- `gpgpu_mascar_reexec_issue_per_cycle=1`
- `gpgpu_mascar_enable_scheduling=0`
- `gpgpu_mascar_enable_would_deprioritize=0`

This config preserves M3 active hit-only/NACK behavior and adds only passive
M4 would-enqueue counters. The active re-execution queue is off.
