# Mascar W26A Framework Release Audit

W26A audited reusable paper-reproduction components after W24 refreshed Table III and W25 integrated energy work. The audit checks runner, collector, manifest, status taxonomy, kernel trace, and documentation surfaces without modifying Mascar M1-M4 mechanism behavior.

Outputs:

- `experiments/framework_release/W26/w26_framework_component_audit.csv`
- `experiments/framework_release/W26/w26_framework_gap_list.csv`

Known limits remain explicit: W25 energy is a current-simulator availability/trend pipeline, not a GPUWattch GTX480 reproduction, and smoke completion is not a correctness oracle unless the workload emits an explicit pass marker.
