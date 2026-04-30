# CCWS SWL Audit (Round S, Stage S1)

_Audited: 2026-04-30. Source: GPGPU-Sim 4.2.0, branch `hrl/paper/ccws-repro-v0`._

---

## 1. What `swl_scheduler` Does

`swl_scheduler` (Static Wavefront Limiter) is a subclass of `scheduler_unit` that restricts the
number of warps eligible for issue each cycle to a configurable maximum.

**Location**: `shader.h:619`, `shader.cc:1678`

**Config format** (parsed in `create_schedulers()` at `shader.cc:187`):
```
warp_limiting:<prioritization>:<num_warps_to_limit>
```

Example: `warp_limiting:2:8` → GTO prioritization, limit to 8 warps per scheduler.

---

## 2. Source Walk

### Constructor (`shader.cc:1678`)

```cpp
swl_scheduler::swl_scheduler(shader_core_stats *stats,
                              shader_core_ctx *shader,
                              Scoreboard *scoreboard, simt_stack **simt,
                              std::vector<shd_warp_t *> *warp,
                              register_set *sp_out, register_set *dp_out,
                              register_set *sfu_out, register_set *int_out,
                              register_set *tensor_core_out,
                              std::vector<register_set *> spec_cores_out,
                              register_set *mem_out, int id,
                              char *config_string)
    : scheduler_unit(stats, shader, scoreboard, simt, warp, sp_out, dp_out,
                     sfu_out, int_out, tensor_core_out, spec_cores_out,
                     mem_out, id) {
  int ret = sscanf(config_string, "warp_limiting:%d:%d",
                   (int *)&m_prioritization, &m_num_warps_to_limit);
  assert(2 == ret);
  assert(GTO == m_prioritization);  // only GTO supported
}
```

**Key**: Only GTO prioritization is currently asserted. Using LRR or two-level with `warp_limiting`
would assert-fail at startup.

### `order_warps()` (`shader.cc:1699`)

```cpp
void swl_scheduler::order_warps() {
  order_by_priority(m_next_cycle_prioritized_warps, m_supervised_warps,
                    m_last_supervised_issued_iter,
                    MIN(m_num_warps_to_limit, m_supervised_warps.size()),
                    ORDERING_GREEDY_THEN_PRIORITY_FUNC,
                    sort_warps_by_oldest_dynamic_id);
}
```

This limits the ordered warp list to `m_num_warps_to_limit` entries before the issue cycle
iterates them. Warps beyond the limit are simply not considered for issue until the next cycle.

---

## 3. Relationship to CCWS Paper SWL

| Aspect | GPGPU-Sim `swl_scheduler` | Paper SWL (§3.2) |
|--------|--------------------------|-----------------|
| Mechanism | Cap `m_next_cycle_prioritized_warps` length before issue | Same: limit warps considered for issue |
| Limit granularity | Static per-config, per-scheduler | Same: static count k |
| Prioritization | GTO-only (asserted) | Paper uses GTO baseline |
| Load-only gating | No — all instructions blocked equally | No — SWL is not load-only |
| Dynamic adjustment | None — pure static limit | None |
| Config syntax | `warp_limiting:2:<k>` | N/A (command-line param) |

**Verdict**: The existing `swl_scheduler` directly corresponds to the paper's SWL baseline.
Semantics match: GTO-ordered, static limit, all-instruction throttle.

---

## 4. Decision: Reuse vs. Extend vs. Reimplement

**Decision: Reuse `swl_scheduler` as-is for the SWL comparison arm.**

CCWS itself will NOT use `swl_scheduler`. CCWS implements dynamic per-load gating via the LSS
Can Issue bit, which is a fundamentally different mechanism from `swl_scheduler`'s static warp
count cap. The two can coexist:

- `gpgpu_ccws_enable_swl=1` → activate `swl_scheduler` mode via the existing `warp_limiting`
  config string (for sweep comparison)
- `gpgpu_enable_ccws=1` → activate the new CCWS LLS/VTA/Can-Issue logic (in future stages)

No modification to `swl_scheduler` is required for the CCWS reproduction.

---

## 5. Current SM7_QV100 Scheduler Config

File: `configs/tested-cfgs/SM7_QV100/gpgpusim.config`, line 134:

```
-gpgpu_scheduler lrr
```

SM7_QV100 uses `lrr` (least recently run), not `gto` or `warp_limiting`. This means the current
baseline is LRR-scheduled, and activating SWL requires changing to `warp_limiting:2:<k>`. The CCWS
paper uses GTO as its non-CCWS baseline, so LRR vs GTO is a known divergence — documented in
repro plan §13.

---

## 6. Multiple Schedulers Per Core

SM7_QV100 config has `gpgpu_num_sched_per_core = 2` (two `scheduler_unit` instances per SM).
Each scheduler independently maintains `m_num_warps_to_limit` through its own `swl_scheduler`
instance. CCWS state (LLS array, VTA) will also need to be per-scheduler when implemented in S5.

---

## 7. Files Audited

| File | Lines | Purpose |
|------|-------|---------|
| `src/gpgpu-sim/shader.h` | 568–637 | `two_level_active_scheduler`, `swl_scheduler` class defs |
| `src/gpgpu-sim/shader.cc` | 187–215 | `create_schedulers()` dispatch |
| `src/gpgpu-sim/shader.cc` | 1678–1712 | `swl_scheduler` constructor + `order_warps()` |
