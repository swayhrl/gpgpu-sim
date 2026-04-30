# Paper Reproduction Plans

Each reproduced paper has two files here:

```
docs/papers/<paper-key>_repro_plan.md    # repro plan (from paper_repro_template.md)
docs/papers/<paper-key>_result_notes.md  # result notes after experiments
```

Template: `docs/paper_repro_template.md`

---

## Paper Key Naming Rules

- Lowercase only.
- No spaces, no special characters (use `-` as separator if needed).
- Short and memorable.
- Examples: `ccws`, `daws`, `pcal`, `linebacker`, `medic`, `rrip`, `bnp`

---

## Reproduction Stages

| Stage | Description |
|-------|-------------|
| `planned` | Paper identified, not yet mapped to GPGPU-Sim |
| `mapped` | GPGPU-Sim module mapping written in repro plan |
| `instrumented` | Instrumentation added, no behavior change |
| `implemented` | Minimal behavior implementation done, feature flag default off |
| `quick_pass` | feature_off ≈ baseline confirmed on quick set |
| `standard_pass` | feature_on results stable on standard set |
| `merged` | Merged into `hrl/integration/cache-papers-v0` |
| `abandoned` | Not reproducible in GPGPU-Sim; documented why |

---

## Current Papers

| Paper Key | Title / Description | Stage | Branch |
|-----------|---------------------|-------|--------|
| _(Round R: TBD)_ | First cache paper — to be selected | `planned` | — |

---

## Notes

- Do not put PDF files in this directory unless they are CC-licensed and small.
- Record citation / DOI link in the repro plan metadata instead.
- result_notes.md is written **after** experiments — do not create it before running.
