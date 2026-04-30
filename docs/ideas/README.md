# Self-Developed Idea Plans

Each self-developed idea has two files here:

```
docs/ideas/<idea-key>_plan.md        # idea plan (from idea_branch_template.md)
docs/ideas/<idea-key>_result_notes.md  # result notes after experiments
```

Template: `docs/idea_branch_template.md`

---

## Idea Key Naming Rules

- Lowercase only.
- No spaces, use `-` as separator.
- Short and descriptive.
- Examples: `l2-bypass`, `cache-policy-experiments`, `writeback-hazard-filter`, `adaptive-throttle`

---

## Development Stages

| Stage | Description |
|-------|-------------|
| `proposed` | Idea documented, not yet mapped |
| `mapped` | GPGPU-Sim module mapping written |
| `instrumented` | Instrumentation added, no behavior change |
| `prototyped` | Minimal behavior prototype done, feature flag default off |
| `quick_pass` | feature_off ≈ baseline confirmed on quick set |
| `standard_pass` | feature_on results stable on standard set |
| `integration_tested` | Tested in `hrl/integration/cache-ideas-v0` |
| `abandoned` | Idea dropped; documented why |

---

## Current Ideas

| Idea Key | Description | Stage | Branch |
|----------|-------------|-------|--------|
| _(Round V: cache-policy-experiments)_ | First self-developed cache policy idea — after ≥1 paper flow complete | `proposed` | — |

---

## Policy: Idea vs Paper

- An idea branch **must not modify a paper branch** directly.
- If an idea extends a paper, branch from that paper's stable tag: `git checkout -b hrl/idea/<idea-key>-from-<paper-key>-v0 <paper-key>-quick-pass`.
- Merging happens **only** in `hrl/integration/cache-ideas-v0` or `hrl/integration/cache-final-v0`.

---

## Timing

Self-developed cache policy experiments (first idea branch) will open **after at least one paper reproduction flow is complete end-to-end** (standard_pass milestone reached).
