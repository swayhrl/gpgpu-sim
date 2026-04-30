# experiments/paper-ccws/

This directory contains **lightweight experiment metadata** for the CCWS paper reproduction.

**Do not store large logs here.** Large run outputs and logs go to:
- `/workspace/experiments/gpgpu-sim/ccws/<timestamp>/` (not committed)
- `/workspace/repos/gpgpu-workloads/runs/` (workload runner output, not committed here)

---

## Purpose

Track the config matrix and result manifest for CCWS experiments across three feature states:
- `baseline`: using `cache-inst-v0` tag
- `feature_off`: CCWS branch with `gpgpu_enable_ccws=0`
- `feature_on`: CCWS branch with `gpgpu_enable_ccws=1`

---

## Files

| File | Description |
|------|-------------|
| `config_matrix.csv` | List of GPGPU-Sim config sets used; one row per config |
| `result_manifest.csv` | One row per completed run; links to output directories and summary CSVs |
| `run_notes.md` | Free-text observations per experiment milestone (create when results exist) |

---

## Related files

| Path | Description |
|------|-------------|
| `docs/papers/ccws_repro_plan.md` | Full reproduction plan |
| `docs/papers/ccws_reading_notes.md` | Paper reading notes |
| `configs/hrl-repro/SM7_QV100_ccws_off/` | feature_off config (create in Round S) |
| `configs/hrl-repro/SM7_QV100_ccws_on/` | feature_on config (create in Round S) |

---

## Status

| Stage | Status |
|-------|--------|
| Plan written | ✓ Round R |
| Branch created | — Round S |
| feature_off quick pass | — Round S |
| feature_on quick pass | — Round S+ |
| standard set results | — Round S+ |
