# Idea Branch Template

_Copy this file to `docs/ideas/<idea-key>_plan.md` and fill in each section._

---

## 1. Idea Metadata

| Field | Value |
|-------|-------|
| **idea key** | `<idea-key>` (lowercase, no spaces, e.g. `l2-bypass`) |
| **branch name** | `hrl/idea/<idea-key>-v0` or `hrl/idea/<idea-key>-from-<paper-key>-v0` |
| **parent branch** | `hrl/repro-infra-v0` (default) or `<paper-key>-quick-pass` tag if extending a paper |
| **related paper** | `<paper-key>` if this idea extends or contrasts a reproduced paper; else `none` |
| **mechanism category** | e.g. cache replacement / bypassing / prefetching / throttling |
| **target modules** | e.g. `gpu-cache.h`, `shader.cc` |

---

## 2. Motivation

### Why this idea?
_What observation or gap in existing work motivates this idea?_

### Difference from related paper mechanisms
_If there is a related paper, what does this idea change or add beyond the paper's mechanism?_

### Expected improvement
_What bottleneck does this idea address? What metric(s) should improve?_

- Target metric:
- Target workloads:
- Hypothesis:

---

## 3. Mechanism Sketch

_High-level description of the mechanism. Can be informal — the goal is to capture the core decision logic before implementation._

Key decision logic:
1.
2.
3.

Key data structures:
-
-

---

## 4. Mapping to GPGPU-Sim

| Idea Component | GPGPU-Sim Module | Candidate Files | Expected Risk | Notes |
|----------------|-----------------|-----------------|--------------|-------|
| | | | low / med / high | |
| | | | | |

---

## 5. Feature Flag

```
-gpgpu_enable_<idea_key>   0   # Default off; must be 0 at commit time
```

Rules:
- Default **0**.
- `feature_off` (flag = 0) must produce results ≈ baseline.
- `feature_on` (flag = 1) enables the mechanism.
- **Never commit with flag defaulting to 1.**

---

## 6. Instrumentation Plan

All stats must use prefix `idea_<idea_key>_`:

| Stat | Description |
|------|-------------|
| `idea_<idea_key>_trigger_count` | How many times decision logic fired |
| `idea_<idea_key>_action_count` | How many times action was taken |
| `idea_<idea_key>_bypass_count` | Bypass events (if applicable) |
| `idea_<idea_key>_insert_count` | Insert events (if applicable) |
| `idea_<idea_key>_evict_count` | Eviction events from idea mechanism |

All stats gated by `gpgpu_enable_<idea_key>`.

---

## 7. Validation Plan

| Step | Action | Pass Criteria |
|------|--------|---------------|
| Baseline | Checkout `cache-inst-v0`, run quick set | Reference numbers |
| feature_off | This branch, flag = 0, quick set | sim_cycle ≈ baseline ±1% |
| feature_on | This branch, flag = 1, quick set | Mechanism triggers, stats > 0 |
| Standard set feature_on | Standard set, flag = 1 | Sensitive workloads improve |
| Focused workloads | `<focus_workloads>`, flag = 1 | Hypothesis-driven check |
| Comparison with related paper | Optional: compare vs `<paper-key>-quick-pass` results | Document delta |

---

## 8. Conflict / Integration Notes

_Which paper branches or other idea branches might conflict with this one?_

| Potential Conflict | Branch | Conflict Type | Resolution |
|-------------------|--------|--------------|------------|
| | | | Only resolve in `hrl/integration/*` |

**Rule**: Never merge a paper branch into an idea branch or vice versa. All merges happen only in `hrl/integration/cache-ideas-v0` or `hrl/integration/cache-final-v0`.

---

## 9. Result Notes

_Fill in after running experiments._

| Config | Workload | sim_cycle | IPC | L1D_miss | L2_miss | avgmfl | idea_trigger | notes |
|--------|---------|-----------|-----|---------|---------|--------|-------------|-------|
| feature_off | vecadd | | | | | | 0 | |
| feature_on | vecadd | | | | | | | |

Summary of findings:

---

## 10. Merge Decision

- [ ] **keep separate** — useful as standalone reference, not yet mature enough to integrate
- [ ] **merge to integration** — ready for `hrl/integration/cache-ideas-v0`
- [ ] **merge to cache-final** — selected for `hrl/integration/cache-final-v0`
- [ ] **abandon** — idea doesn't produce meaningful improvement, document why
- [ ] **revise** — partial result, needs refinement before decision

Notes:
