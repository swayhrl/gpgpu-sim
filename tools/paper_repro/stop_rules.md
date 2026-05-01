# L3-lite Supervisor Stop Rules

This document defines when the supervisor should pause and require human sign-off,
versus when it may flag a job as `continue` (safe to auto-proceed).

---

## Stages that may auto-continue

The supervisor will emit `action: continue` for these stage keys:

| Stage key          | Rationale |
|--------------------|-----------|
| `reading`          | No code changes; pure note-taking |
| `noop`             | Config-only, feature flag off; baseline must be unchanged |
| `telemetry`        | Passive probes; feature_off regression enforced |
| `would_change`     | Read-only telemetry gate; no scheduler mutation |
| `final_report`     | Documentation only |
| `workload_audit`   | Workload survey; no src changes |

For these stages, the supervisor generates a prompt and sets `action: continue`.
The human may paste the prompt into Claude Code without further review, but is
encouraged to glance at the `gpt_review_packet.md` first.

---

## Stages that always require human review

| Stage key               | Why |
|-------------------------|-----|
| `minimal_mechanism`     | First real behavior change; deadlock risk; must verify feature_off |
| `standard_validation`   | Full workload run; cycle direction may be wrong |
| `behavior_change`       | Any unplanned scheduler / cache policy mutation |

---

## Repo-state blockers (override everything)

| Condition                          | Action                      |
|------------------------------------|-----------------------------|
| `git status --short` is non-empty  | `blocked_dirty_repo`        |
| `round_state.yaml` missing         | `blocked_missing_round_state` |
| `round_state.status` not `done`, `complete`, or `pending` | `stop_for_review` |
| Unexpected `src/` diff             | `stop_for_review`           |

---

## Timeout rule

If a stage has been running more than `max_minutes_per_stage` (default 10 min),
Claude Code should output a checkpoint summary and stop.
The supervisor does NOT enforce this at runtime — it is a prompt-level instruction
embedded in each generated prompt.md via `stage_guard.sh`.

---

## Scope creep rule

If Claude begins touching files outside the expected scope for a stage
(e.g., modifying `src/` during a `reading` or `telemetry` stage),
the supervisor's stop rules will catch it on the next cycle via `stop_on_src_diff`.

Manual rule: if Claude's output suggests expanding scope beyond the current stage,
stop and require human confirmation before continuing.

---

## What the supervisor never does

- Does not auto-commit, tag, or push.
- Does not auto-continue `minimal_mechanism` or `standard_validation` regardless of any flag.
- Does not call Claude or GPT API on its own.
- Does not kill processes.
