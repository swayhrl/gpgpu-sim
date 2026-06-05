#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
configs=(
  "configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off"
  "configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on"
)
for cfg in "${configs[@]}"; do
  dir="${repo_root}/${cfg}"
  test -d "${dir}"
  test -f "${dir}/gpgpusim.config"
  test -f "${dir}/accelwattch_ptx_sim.xml"
  grep -q -- '^-power_simulation_enabled 1' "${dir}/gpgpusim.config"
  grep -q -- '^-power_simulation_mode 0' "${dir}/gpgpusim.config"
  grep -q -- '^-accelwattch_xml_file /workspace/repos/gpgpu-sim_distribution/configs/tested-cfgs/SM7_QV100/accelwattch_ptx_sim.xml' "${dir}/gpgpusim.config"
  grep -q -- '^-power_per_cycle_dump 0' "${dir}/gpgpusim.config"
  printf 'ok,%s\n' "${cfg}"
done
