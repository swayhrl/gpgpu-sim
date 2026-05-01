# Risk Policy — L3-lite Paper Reproduction Supervisor

This document defines risk levels for paper reproduction stages and specifies
what the supervisor is and is not permitted to do automatically.

---

## High-Risk Stage Definitions

A stage is **high risk** if it involves any of the following:

| Category | Examples |
|----------|---------|
| Scheduler behavior change | Warp selection logic, issue priority, stall gating |
| Cache replacement change | Eviction policy, bypass decision, hit/miss behavior |
| Memory hierarchy change | Prefetch trigger, MSHR policy, coalescing behavior |
| `sim_cycle` may change on `feature_on` | Any mechanism that blocks or reorders instructions |
| Standard or extended validation | Full 13-workload or 19-workload run |
| Deadlock / hang / timeout | Any scenario where a warp can be permanently stalled |
| Paper trend judgment required | Deciding if cycle direction matches paper claim |

Additionally, these stage keys are unconditionally high risk regardless of content:

- `minimal_mechanism`
- `standard_validation`
- `behavior_change`

---

## Risk Levels

### Low Risk
- No scheduler or cache behavior change.
- `feature_off` baseline is provably unchanged.
- Examples: `reading`, `noop` (config-only), `telemetry` (passive probe), `final_report`.
- **Supervisor action**: `continue` — safe to auto-proceed.

### Medium Risk
- Code change that touches `src/`, but only adds passive observation (counters, telemetry).
- `feature_off` baseline must be verified unchanged before proceeding.
- Examples: `would_change` telemetry (read-only gate signal).
- **Supervisor action**: `continue` to execute, but `stop_after_completion=true` triggers `stop_for_review` after the stage finishes.

### High Risk
- First real behavior change (`minimal_mechanism`).
- Any stage that gates, reorders, or skips warp issue.
- Any standard or extended validation run.
- **Supervisor action**: `blocked_high_risk_stage` — never auto-continues. Human must explicitly paste the prompt.

---

## Version Control Reduces Risk — But Does Not Replace Judgment

Tagging each milestone means a broken mechanism can be reverted with:

```bash
git checkout <previous-tag>
```

However, version control does not:
- Verify that the mechanism matches the paper.
- Detect semantic correctness of gating logic.
- Prevent cycle direction from being wrong.

**Version control is a safety net, not a substitute for human review on high-risk stages.**

---

## High-Risk Stage Execution Rules

1. **Supervisor will not auto-continue** any high-risk stage.
2. A high-risk stage **may be executed** by pasting the prompt manually.
3. After completion, the supervisor **must stop** for review before proceeding to the next stage.
4. `stop_after_completion=true` enforces this in the job queue.
5. `requires_gpt_review=true` signals that the review packet should be sent to GPT before continuing.

---

## What the Supervisor Never Does

Regardless of risk level, the supervisor never:
- Commits, tags, or pushes.
- Calls Claude or GPT API automatically.
- Chains high-risk stages without human intervention.
- Proceeds after a timeout, deadlock, or unexpected `sim_cycle` change.
