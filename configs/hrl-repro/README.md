# configs/hrl-repro/

This directory contains per-experiment GPGPU-Sim config sets for paper reproduction and idea development.

**Do not modify** `configs/tested-cfgs/` directly. All experiment configs are copies created here.

---

## Naming Convention

| Pattern | Purpose |
|---------|---------|
| `SM7_QV100_cache_inst/` | Config used with cache instrumentation baseline |
| `SM7_QV100_<paper_key>_off/` | Config for feature_off validation (should ≈ baseline) |
| `SM7_QV100_<paper_key>_on/` | Config for feature_on run |
| `SM7_QV100_<idea_key>_off/` | Config for idea feature_off validation |
| `SM7_QV100_<idea_key>_on/` | Config for idea feature_on run |

Base config: `configs/tested-cfgs/SM7_QV100/` (Volta QV100, best validated).

---

## How to Create a Config Set

```bash
# Copy base config for a new paper
mkdir -p configs/hrl-repro/SM7_QV100_<paper_key>_off
cp configs/tested-cfgs/SM7_QV100/gpgpusim.config \
   configs/hrl-repro/SM7_QV100_<paper_key>_off/
cp configs/tested-cfgs/SM7_QV100/config_volta_islip.icnt \
   configs/hrl-repro/SM7_QV100_<paper_key>_off/

# Then edit gpgpusim.config to set:
# -gpgpu_enable_<paper_key> 0
```

---

## Required README.md per Config Dir

Each config subdirectory should contain a `README.md` with:

```markdown
# Config: SM7_QV100_<paper_key>_off

| Field | Value |
|-------|-------|
| Base config | configs/tested-cfgs/SM7_QV100 |
| Branch | hrl/paper/<paper_key>-repro-v0 |
| Commit | <commit-sha> |
| Feature flag | -gpgpu_enable_<paper_key> 0 |
| Changed parameters | (list any non-default changes) |
| Expected behavior | feature_off: sim results should match baseline |
```

---

## feature_off Config Requirement

The `_off` config must enable verification that **feature_off ≈ baseline**.  
This means:
- Same cache geometry as baseline.
- Feature flag explicitly set to 0.
- No other behavioral parameters changed from baseline.

If `feature_off` diverges from `baseline`, there is a bug in the implementation — do not proceed to `feature_on` testing.

---

## When to defer config creation

If the config files are large or the paper has many parameter variants, delay copying until the implementation reaches the "config knobs only" commit stage.  
Until then, only this `README.md` is needed.
