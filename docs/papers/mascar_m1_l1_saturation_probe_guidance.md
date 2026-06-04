# Mascar M1 Detailed Guidance: Passive L1 Saturation Probe

## Stage position

This is M1 of the Mascar paper-like reproduction effort.

M0 documented that the current branch is an approximate/proxy Mascar implementation based on scheduler-side `m_mem_out` stall streaks. M1 must add a passive L1-side saturation probe so later M2 can switch from proxy scheduling to paper-like EP/MP owner scheduling.

M1 must not implement EP/MP mode.
M1 must not implement owner warp scheduling.
M1 must not implement WST/WRC.
M1 must not implement non-owner hit-only or miss-NACK behavior.
M1 must not implement a re-execution queue.
M1 must not change simulator behavior when the new probe is off.
M1 should only add config knobs, passive cache/LSU probes, stats, docs, and one probe config.

## Branch and repository

Repository:
  /workspace/repos/gpgpu-sim_distribution

Required branch:
  hrl/paper/mascar-repro-v0

Use origin only for normal sync.
Do not fetch upstream.

## Hard constraints

1. Default behavior must remain unchanged.
2. `gpgpu_enable_mascar=0` must preserve baseline behavior.
3. New M1 knobs must default off or non-invasive.
4. Do not change cache request accept/reject behavior.
5. Do not change scheduler issue order.
6. Do not change `mascar_skip_gate_warp()` policy except if a compile-safe signature update is absolutely required; prefer no change.
7. Do not implement EP/MP or owner warp yet.
8. Do not implement re-execution yet.
9. Do not run full benchmark suites.
10. Do not commit M1 implementation outputs.
11. Do not use `git add .` or `git add -A`.

## Target paper mechanism for M1

The Mascar paper uses an L1-side memory saturation signal. It is asserted when L1-side structures such as MSHRs and the miss queue are almost full, so the scheduler can later switch into MP mode before the memory subsystem completely blocks forward progress.

In this GPGPU-Sim branch, the current approximate Mascar implementation uses `m_mem_out` / scheduler-side stall streaks. M1 must add passive visibility into actual L1D resource pressure.

## Required implementation

### A. New config knobs

Add these fields to `shader_core_config` near existing Mascar knobs:

- `int gpgpu_mascar_enable_l1_saturation_probe`
- `unsigned gpgpu_mascar_l1_saturation_margin`

Register them in `shader_core_config::reg_options()` near existing Mascar options.

Suggested defaults:

- `-gpgpu_mascar_enable_l1_saturation_probe` default `"0"`
- `-gpgpu_mascar_l1_saturation_margin` default `"1"`

Meaning:

- probe enabled only when both `gpgpu_enable_mascar` and `gpgpu_mascar_enable_l1_saturation_probe` are nonzero.
- margin is the almost-full distance. For example, miss queue is almost full when `used + margin >= limit`.

### B. Cache-side read-only accessors

Add minimal read-only helpers; do not expose mutable state.

In `mshr_table`, add public const helpers such as:

- `unsigned entries_used() const`
- `unsigned entries_limit() const`

Do not change `probe`, `full`, `add`, `mark_ready`, or request behavior.

In `baseline_cache`, add a read-only Mascar probe helper, for example:

- `void mascar_l1_saturation_snapshot(new_addr_type addr, unsigned margin, bool &mshr_full_for_addr, bool &mshr_entries_almost_full, bool &miss_queue_full, bool &miss_queue_almost_full, unsigned &mshr_used, unsigned &mshr_limit, unsigned &missq_used, unsigned &missq_limit) const`

Implementation guidance:

- `mshr_addr = m_config.mshr_addr(addr)`
- `mshr_full_for_addr = m_mshrs.full(mshr_addr)`
- `mshr_entries_almost_full = (m_mshrs.entries_used() + margin >= m_mshrs.entries_limit())`, guarding zero limits if necessary
- `missq_used = m_miss_queue.size()`
- `missq_limit = m_config.m_miss_queue_size`
- `miss_queue_full = (missq_used >= missq_limit)`
- `miss_queue_almost_full = (missq_used + margin >= missq_limit)`, guarding zero limits if necessary

This helper must be passive and must not modify cache state.

### C. LSU / L1D sampling

Record L1D saturation samples at the L1D access attempt points, not in the scheduler.

Relevant paths to inspect:

- `ldst_unit::process_memory_access_queue_l1cache()`
- `ldst_unit::L1_latency_queue_cycle()`
- `ldst_unit::process_cache_access()`

Recommended minimal design:

- Add M1 counters to `ldst_unit`, not to cache state.
- Add helper method in `ldst_unit`, for example:
  - `void mascar_sample_l1_saturation(l1_cache *cache, new_addr_type addr)`
  - `void mascar_note_l1_reservation_fail()`
  - `void get_mascar_l1_saturation_stats(...) const`

Counters should be unsigned long long.

Required counters:

- `m_mascar_l1_sat_sample`
- `m_mascar_l1_sat_sample_saturated`
- `m_mascar_l1_sat_mshr_full`
- `m_mascar_l1_sat_mshr_almost_full`
- `m_mascar_l1_sat_missq_full`
- `m_mascar_l1_sat_missq_almost_full`
- `m_mascar_l1_sat_reservation_fail`

Optional but useful:

- `m_mascar_l1_sat_mshr_used_sum`
- `m_mascar_l1_sat_mshr_used_max`
- `m_mascar_l1_sat_missq_used_sum`
- `m_mascar_l1_sat_missq_used_max`

Sampling rule:

- If `!m_config->gpgpu_enable_mascar`, do nothing.
- If `!m_config->gpgpu_mascar_enable_l1_saturation_probe`, do nothing.
- Otherwise sample before `m_L1D->access()` or `cache->access()` for L1D.
- Increment `m_mascar_l1_sat_sample` for each L1D sample.
- Compute saturated if any of:
  - `mshr_full_for_addr`
  - `mshr_entries_almost_full`
  - `miss_queue_full`
  - `miss_queue_almost_full`
- Increment corresponding reason counters.
- If actual `cache->access()` returns `RESERVATION_FAIL`, increment `m_mascar_l1_sat_reservation_fail`.
- Do not alter the returned cache status.

### D. Stats aggregation and printing

Add aggregation similar to existing `get_mascar_stats`.

Possible design:

- Add `ldst_unit::get_mascar_l1_saturation_stats(...) const`.
- Add `shader_core_ctx::get_mascar_l1_saturation_stats(...) const`, forwarding to `m_ldst_unit`.
- Add `simt_core_cluster::get_mascar_l1_saturation_stats(...) const`.
- In `gpgpu_sim::print_stats`, print new fields near existing `paper_mascar_*` lines.

Required printed stat names:

- `paper_mascar_l1_sat_probe_enabled`
- `paper_mascar_l1_sat_sample`
- `paper_mascar_l1_sat_sample_saturated`
- `paper_mascar_l1_sat_mshr_full`
- `paper_mascar_l1_sat_mshr_almost_full`
- `paper_mascar_l1_sat_missq_full`
- `paper_mascar_l1_sat_missq_almost_full`
- `paper_mascar_l1_sat_reservation_fail`

If optional sum/max counters are implemented, print:

- `paper_mascar_l1_sat_mshr_used_sum`
- `paper_mascar_l1_sat_mshr_used_max`
- `paper_mascar_l1_sat_missq_used_sum`
- `paper_mascar_l1_sat_missq_used_max`

### E. Config

Add one config directory:

- `configs/hrl-repro/SM7_QV100_mascar_l1sat_probe_on/`

Copy from a nearby Mascar config, but set:

- `-gpgpu_enable_mascar 1`
- `-gpgpu_mascar_enable_l1_saturation_probe 1`
- `-gpgpu_mascar_l1_saturation_margin 1`
- `-gpgpu_mascar_enable_scheduling 0`
- `-gpgpu_mascar_enable_would_deprioritize 0`

Existing approximate scheduling must remain disabled in this config.

Add a short README in that directory explaining it is passive L1 saturation telemetry only.

### F. Documentation

Create:

- `docs/papers/mascar_m1_l1_saturation_probe.md`

Required sections:

1. Goal
2. Code changes
3. New knobs
4. New stats
5. Why this is closer to paper Mascar than the M0 proxy
6. Explicitly not implemented in M1
7. Validation performed
8. Risks and follow-up for M2

### G. Postcheck and review pack

Create:

- `experiments/paper-mascar/m1_postcheck.md`

It must include:

- start_iso
- end_iso
- elapsed_sec computed with `start_ts=$(date +%s)` and `end_ts=$(date +%s)`
- branch and HEAD
- git status before and after
- list of changed files
- exact build/check commands run
- whether build passed
- whether any smoke run was performed
- printed stats grep result if a smoke run was performed
- review pack path
- warnings

Also create:

- `experiments/paper-mascar/m1_symbol_grep.txt`
- `experiments/paper-mascar/m1_diff_name_status.txt`

Review pack path:

- `/workspace/tmp/mascar_m1_review_pack_YYYYMMDD_HHMMSS.tar.gz`

Include all changed files and postcheck/helper files.

## Validation guidance

Required minimum:

1. Compile check if normal build environment is available.
2. Grep check that all new options and printed stats exist.
3. Verify no config enables scheduling in the new passive config.
4. Verify `gpgpu_enable_mascar=0` remains default.
5. Do not run full benchmark suites.

If a short existing smoke runner is obvious from repository docs, run at most one tiny existing workload with:
- baseline/off config
- l1sat_probe_on config

If smoke run is not obvious or would be long, skip it and document why. Do not spend more than 10 minutes searching for a benchmark runner.

## Stop conditions

Stop and report without forcing through if:

1. Cache helper requires invasive mutation of cache behavior.
2. Build breaks in unrelated CCWS/DAWS code.
3. You cannot add passive stats without changing request status behavior.
4. Implementation exceeds M1 scope and starts becoming scheduler/owner/re-exec work.
5. Total elapsed time exceeds 55 minutes.

## Final report to GPT

At the end, report only:

1. elapsed_sec
2. review pack path
3. git status --short
4. files GPT should review
5. build/smoke status

Do not commit M1 implementation outputs.
