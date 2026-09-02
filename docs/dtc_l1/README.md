# DTC-L1 Core Repository Notes

This directory holds the simulator-core specification for reproducing the thesis design "Decoupled-Tag Cache" (IO-DTC and OO-DTC) in Accel-Sim/GPGPU-Sim.

## Authority

- `DTC_L1_SPEC.md` is the authoritative M0 architecture specification for the core implementation.
- Cross-repository project state and executable stage instructions live in:
  `swayhrl/accel-sim-framework@hrl/decoupled-l1-exp-v0/docs/dtc_l1/chatgpt_handoff/`.

Do not duplicate or reinterpret frozen architecture decisions in ad-hoc source comments. If a source-level implementation constraint forces a change, report it through the Codex handoff and wait for a specification update.

## Source anchor

This branch was created from `accel-sim/gpgpu-sim_distribution:dev` at:

`91880c53383d5a6a6742bfb1be2c5f34e39c7871`

The project branch is:

`hrl/decoupled-l1-v0`

## Current stage

M0 architecture freeze is complete at the mechanism-model level. No M1 implementation is authorized merely by the presence of this document; follow the framework repository's `CODEX_NEXT_STAGE.md`.
