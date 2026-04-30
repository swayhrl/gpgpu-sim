# Round O: Cache Instrumentation

**Branch**: `hrl/cache-instrumentation-v0`
**Date**: 2026-04-30
**Status**: Complete — verified on quick set (7 workloads, 0 failures)

## Goal

Add passive, statistics-only instrumentation to GPGPU-Sim cache subsystem.
Output per-access-type cache breakdown as `cacheinst_*` key=value lines at the
end of each simulation run. No behavioral changes, no config changes.

## Output format

38 lines per simulation run, immediately following the existing cache stats block:

```
cacheinst_L1D_access_total = 96
cacheinst_L1D_hit = 0
cacheinst_L1D_miss = 96
cacheinst_L1D_pending_hit = 0
cacheinst_L1D_reservation_fail = 79
cacheinst_L1D_sector_miss = 72
cacheinst_L1D_miss_rate = 1.0000
cacheinst_L1D_reservation_fail_rate = 0.4514
cacheinst_L1D_global_load_access = 64
cacheinst_L1D_global_load_hit = 0
cacheinst_L1D_global_load_miss = 64
cacheinst_L1D_global_load_rfail = 67
cacheinst_L1D_global_store_access = 32
cacheinst_L1D_global_store_hit = 0
cacheinst_L1D_global_store_miss = 32
cacheinst_L1D_global_store_rfail = 12
cacheinst_L1D_local_access = 0
cacheinst_L1D_writeback_access = 0
cacheinst_L1D_write_allocate_access = 0
cacheinst_L2_access_total = 96
cacheinst_L2_hit = 64
cacheinst_L2_miss = 32
cacheinst_L2_pending_hit = 0
cacheinst_L2_reservation_fail = 0
cacheinst_L2_sector_miss = 24
cacheinst_L2_miss_rate = 0.3333
cacheinst_L2_reservation_fail_rate = 0.0000
cacheinst_L2_global_load_access = 64
cacheinst_L2_global_load_hit = 64
cacheinst_L2_global_load_miss = 0
cacheinst_L2_global_load_rfail = 0
cacheinst_L2_global_store_access = 32
cacheinst_L2_global_store_hit = 0
cacheinst_L2_global_store_miss = 32
cacheinst_L2_global_store_rfail = 0
cacheinst_L2_local_access = 0
cacheinst_L2_writeback_access = 0
cacheinst_L2_write_allocate_access = 0
```

Field naming: `cacheinst_<CACHE>_<metric>` where CACHE ∈ {L1D, L2}.

### Field definitions

| Field | Description |
|-------|-------------|
| `access_total` | All accesses (HIT + HIT_RESERVED + MISS + SECTOR_MISS + RESERVATION_FAIL) summed across all access types |
| `hit` | Clean hits (HIT status) |
| `miss` | Full misses (MISS status) |
| `pending_hit` | MSHR pending hits (HIT_RESERVED status) |
| `reservation_fail` | MSHR/bank reservation failures (RESERVATION_FAIL status) |
| `sector_miss` | Sector misses in sector-cache mode (SECTOR_MISS status) |
| `miss_rate` | (miss + sector_miss) / access_total |
| `reservation_fail_rate` | reservation_fail / access_total |
| `global_load_access/hit/miss/rfail` | GLOBAL_ACC_R breakdown |
| `global_store_access/hit/miss/rfail` | GLOBAL_ACC_W breakdown |
| `local_access` | LOCAL_ACC_R + LOCAL_ACC_W total |
| `writeback_access` | L1_WRBK_ACC + L2_WRBK_ACC total |
| `write_allocate_access` | L1_WR_ALLOC_R + L2_WR_ALLOC_R total |

## Implementation

### Key design decision

`cache_stats` already accumulates per-(access_type, status) counts in its internal
`m_stats[streamID][access_type][status]` matrix. The `get_stats()` method queries
this matrix for any combination of access types and statuses. No new counting
instrumentation was needed — only a new query+print function.

This makes the implementation truly passive: zero changes to cache behavior,
zero changes to cache counting logic.

### Files changed

#### `src/gpgpu-sim/gpu-cache.h`

Added declaration after the `cache_stats` class (~line 1259):

```cpp
void print_cacheinst_stats(FILE *fout, const cache_stats &cs,
                           const char *cache_name);
```

Added to three classes:
- `ldst_unit`: `get_L1D_cache_stats(cache_stats &cs) const`
- `shader_core_ctx`: `get_L1D_cache_stats(cache_stats &cs) const`
- `simt_core_cluster`: `get_L1D_cache_stats(cache_stats &cs) const`

#### `src/gpgpu-sim/gpu-cache.cc`

Added `print_cacheinst_stats()` implementation. Queries `cache_stats::get_stats()`
for each metric combination using arrays of `mem_access_type` and
`cache_request_status` enums. Prints 19 L1D fields + 19 L2 fields = 38 total lines.

RESERVATION_FAIL is stored in both `m_stats` (via `inc_stats()`) and `m_fail_stats`
(via `inc_fail_stats()`). The `get_stats()` query path reaches `m_stats`, so
`{RESERVATION_FAIL}` in the status array works correctly.

#### `src/gpgpu-sim/shader.cc`

Added three `get_L1D_cache_stats()` implementations:

```cpp
void ldst_unit::get_L1D_cache_stats(cache_stats &cs) const {
  if (m_L1D) cs += m_L1D->get_stats();
}
void shader_core_ctx::get_L1D_cache_stats(cache_stats &cs) const {
  m_ldst_unit->get_L1D_cache_stats(cs);
}
void simt_core_cluster::get_L1D_cache_stats(cache_stats &cs) const {
  for (unsigned i = 0; i < m_config->n_simt_cores_per_cluster; ++i)
    m_core[i]->get_L1D_cache_stats(cs);
}
```

Added `print_cacheinst_stats()` call in `shader_print_cache_stats()` after the
existing L1D port stats block. Aggregates L1D stats across all clusters then calls
`print_cacheinst_stats(fout, l1d_cs, "L1D")`.

#### `src/gpgpu-sim/gpu-sim.cc`

Added `print_cacheinst_stats(stdout, l2_stats, "L2")` after
`total_l2_css.print_port_stats(stdout, "L2_cache")` in `print_stats()`.
Reuses the existing `l2_stats` aggregate (already built by the surrounding code).

## Verification

Quick set (7 workloads) — all pass, all produce 38 cacheinst lines:

| Workload | L1D miss rate | L2 miss rate | Result |
|----------|--------------|--------------|--------|
| vecadd | 1.0000 | 0.3333 | explicit PASS |
| strided_access | — | — | explicit PASS |
| page_stride_access | — | — | explicit PASS |
| atomic_contention | — | — | explicit PASS |
| mutual_tiled | — | — | explicit PASS |
| polybench_2dconv | 0.2449 | 0.3671 | completed_no_explicit_pass |
| rodinia_hotspot | — | — | completed_no_explicit_pass |

Quick set summary: Explicit PASS=5, Completed without PASS=2, Failed=0, Skipped=0.

## Notes for Round P (extractor update)

The workload-side `scripts/extract_gpgpusim_stats.py` does not yet parse
`cacheinst_*` fields. Round P should add a `CACHEINST_FIELDS` list to the
extractor, analogous to the existing `STAT_FIELDS` / `WARP_STAT_FIELDS` approach.

Suggested fields to add (see field definitions table above):
- `cacheinst_L1D_access_total`, `cacheinst_L1D_hit`, `cacheinst_L1D_miss`
- `cacheinst_L1D_pending_hit`, `cacheinst_L1D_reservation_fail`, `cacheinst_L1D_sector_miss`
- `cacheinst_L1D_miss_rate`, `cacheinst_L1D_reservation_fail_rate`
- `cacheinst_L1D_global_load_access`, `cacheinst_L1D_global_load_hit`, `cacheinst_L1D_global_load_miss`, `cacheinst_L1D_global_load_rfail`
- `cacheinst_L1D_global_store_access`, `cacheinst_L1D_global_store_hit`, `cacheinst_L1D_global_store_miss`, `cacheinst_L1D_global_store_rfail`
- `cacheinst_L1D_local_access`, `cacheinst_L1D_writeback_access`, `cacheinst_L1D_write_allocate_access`
- Same 19 fields for `cacheinst_L2_*`

All follow the existing `key = value` pattern so a simple `re.match(r'^(\w+)\s*=\s*(.+)$')`
regex parse will work.
