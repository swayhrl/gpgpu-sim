# DAWS Experiments

Paper: Divergence-Aware Warp Scheduling (Rogers, O'Connor, Aamodt — MICRO 2013)
Branch: hrl/paper/daws-repro-v0
Config root: configs/hrl-repro/

## Directory Structure

- config_matrix.csv     — all configs used across rounds
- result_manifest.csv   — all experiment results
- round_state.yaml      — current round state
- focused_daws_validation.csv  (created in AUTO-7)
- final_summary.csv            (created in AUTO-8)

## Config Naming Convention

SM7_QV100_daws_noop_off         — feature_off baseline
SM7_QV100_daws_telemetry        — telemetry only (no gating)
SM7_QV100_daws_would_throttle   — would-throttle telemetry
SM7_QV100_daws_throttle_conservative  — throttle with conservative threshold
SM7_QV100_daws_throttle_aggressive    — throttle with aggressive threshold
