# DTC-L1 Core Repository Notes

This directory holds the simulator-core specification for reproducing the thesis design "Decoupled-Tag Cache" (IO-DTC and OO-DTC) in Accel-Sim/GPGPU-Sim.

## Authority

- `DTC_L1_SPEC.md` is the authoritative frozen M0 architecture specification.
- M5 coordination, experiment matrix, problem-resolution policy, and handoffs live in:
  `swayhrl/accel-sim-framework@hrl/decoupled-l1-exp-m5-v0/docs/dtc_l1/`.
- Researcher-approved M5 activation/interpretation is:
  `docs/dtc_l1/m5/M5_V1_APPROVAL.md` in the Framework repository.

Do not duplicate or reinterpret frozen architecture decisions in ad-hoc source comments. M5 may repair implementation/modeling fidelity bugs, add counters, parameterize frozen knobs, and improve workload integration, but must not change frozen architecture semantics merely to obtain thesis-like performance.

## Source anchors

Official upstream base:

- `accel-sim/gpgpu-sim_distribution:dev`
- SHA `91880c53383d5a6a6742bfb1be2c5f34e39c7871`

Frozen M0 project anchor:

- `swayhrl/gpgpu-sim:hrl/decoupled-l1-v0`
- M0 documentation SHA `5e35de9914f1ad28647ef3a416d054b86f3e44a5`

Validated M1-M4 parent:

- `swayhrl/gpgpu-sim:hrl/decoupled-l1-m1m4-v0`
- final SHA `cdeec769fd0c1be12b45d58536ecb81074d4b415`

Active M5 implementation branch:

- `swayhrl/gpgpu-sim:hrl/decoupled-l1-m5-v0`

M0 and validated M1-M4 branches remain read-only experimental anchors.

## Current stage

M1-M4 are complete and independently reviewed. M5 v1 is authorized on the dedicated M5 branches.

Current progression begins at M5.0A fidelity/reproducibility lock and continues automatically through workload recovery, platform/metric lock, pilot triage, Figures 4.2/4.5/4.7/4.8/4.9/4.10, and causal synthesis.

Terminal compute state:

`M5_COMPUTE_READY_FOR_REVIEW`.

Read `AGENTS.md` before modifying Core source. Ordinary M5 implementation/workload problems are solved under the Framework `M5_PROBLEM_RESOLUTION_POLICY.md`; pause only for a genuine researcher-decision boundary or final compute review.
