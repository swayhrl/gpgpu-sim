# DTC-L1 Core Repository Notes

This directory holds the simulator-core specification for reproducing the thesis design "Decoupled-Tag Cache" (IO-DTC and OO-DTC) in Accel-Sim/GPGPU-Sim.

## Authority

- `DTC_L1_SPEC.md` is the authoritative frozen M0 architecture specification.
- Cross-repository project state and executable M1-M4 Goal instructions live in:
  `swayhrl/accel-sim-framework@hrl/decoupled-l1-exp-m1m4-v0/docs/dtc_l1/`.

Do not duplicate or reinterpret frozen architecture decisions in ad-hoc source comments. If a source-level implementation constraint conflicts with the specification, report it through the Codex handoff and STOP rather than silently changing semantics.

## Source anchors

Official upstream base:

- `accel-sim/gpgpu-sim_distribution:dev`
- SHA `91880c53383d5a6a6742bfb1be2c5f34e39c7871`

Frozen M0 project anchor:

- `swayhrl/gpgpu-sim:hrl/decoupled-l1-v0`
- M0 documentation SHA `5e35de9914f1ad28647ef3a416d054b86f3e44a5`

Active implementation branch:

- `swayhrl/gpgpu-sim:hrl/decoupled-l1-m1m4-v0`

The M0 branch remains read-only.

## Current stage

M0 architecture freeze is complete. M1 through M4 are authorized as one continuous Goal-mode execution on the active implementation branch, with mandatory HARD gates and review packs at each major stage.

After M4 closeout, Codex must STOP for M5 review.
