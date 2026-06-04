# Mascar M3A Notes

M3A passive non-owner hit-only probe completed before M3B active behavior.

- Added default-off M3 knobs: `gpgpu_mascar_enable_nonowner_hit_only_probe`, `gpgpu_mascar_enable_nonowner_hit_only`, `gpgpu_mascar_nonowner_hit_only_loads_only`, and `gpgpu_mascar_nonowner_nack_release_threshold`.
- Added read-only `data_cache::mascar_l1_hit_only_probe()` using tag-array probe mode only; it does not call `process_tag_probe()`, allocate MSHRs, update LRU, emit events, or update cache stats.
- Added passive L1D sampling before normal L1D access in both `process_memory_access_queue_l1cache()` and `L1_latency_queue_cycle()`.
- Passive stats count HIT as would-hit and treat MISS, SECTOR_MISS, HIT_RESERVED, RESERVATION_FAIL, and MSHR_HIT as conservative would-NACK classes.
- Added `configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/`, with active hit-only disabled and old proxy scheduling disabled.

M3A validation:

- `git diff --check`: pass.
- `source setup_environment release && make -j2`: pass.
- No smoke run at M3A checkpoint.
