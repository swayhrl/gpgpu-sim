# Experiments Directory

This directory contains lightweight experiment metadata and run notes.  
**Do not put large logs here.** Large outputs go to `/workspace/experiments/gpgpu-sim/` (not committed).

---

## Directory Layout

```
experiments/
  paper-<paper-key>/        # Per-paper experiment metadata
    README.md
    run_notes.md
    config_matrix.csv
    result_manifest.csv
  idea-<idea-key>/          # Per-idea experiment metadata
    README.md
    run_notes.md
    config_matrix.csv
    result_manifest.csv
```

Create subdirectories only when a paper / idea branch reaches the "quick-pass" milestone.  
Do not create empty placeholder directories.

---

## Large Output Paths (not committed)

```
/workspace/experiments/gpgpu-sim/<paper-key>/<timestamp>/    # paper run logs
/workspace/experiments/gpgpu-sim/idea-<idea-key>/<timestamp>/  # idea run logs
/workspace/repos/gpgpu-workloads/runs/                        # workload run outputs
```

Commit only: `result_manifest.csv` (summary rows) and `run_notes.md` (text observations).

---

## result_manifest.csv Fields

| Field | Description |
|-------|-------------|
| `run_id` | Unique identifier for the run (e.g. `ccws-feature-on-quick-20260430`) |
| `type` | `paper` or `idea` |
| `key` | Paper key or idea key |
| `branch` | Git branch name |
| `commit` | Git commit SHA |
| `config` | Config directory used (relative to `configs/hrl-repro/`) |
| `workload_set` | `quick` / `standard` / `extended` |
| `workload` | Individual workload name |
| `feature_state` | `baseline` / `feature_off` / `feature_on` |
| `output_dir` | Absolute path to log directory (not committed) |
| `summary_csv` | Path to extracted CSV (relative or absolute) |
| `status` | `pass` / `fail` / `partial` / `skip` |
| `notes` | Free-text observation |

### Example row

```csv
run_id,type,key,branch,commit,config,workload_set,workload,feature_state,output_dir,summary_csv,status,notes
ccws-foff-quick-20260501,paper,ccws,hrl/paper/ccws-repro-v0,abc1234,SM7_QV100_ccws_off,quick,vecadd,feature_off,/workspace/experiments/gpgpu-sim/ccws/20260501-120000,runs/ccws_foff_quick.csv,pass,sim_cycle=5569 matches baseline
```

---

## config_matrix.csv Fields

| Field | Description |
|-------|-------------|
| `config_dir` | Config directory name under `configs/hrl-repro/` |
| `base_config` | Source config (e.g. `configs/tested-cfgs/SM7_QV100`) |
| `branch` | Branch this config was created for |
| `commit` | Commit at time of config snapshot |
| `feature_flag` | `-gpgpu_enable_<key>` value |
| `changed_params` | Comma-separated list of changed parameters |
| `description` | Human-readable purpose of this config |
