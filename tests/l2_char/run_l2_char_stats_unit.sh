#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
out=${TMPDIR:-/tmp}/l2_char_stats_unit.$$
trap 'rm -f "$out"' EXIT
g++ -std=c++0x -I"$root/src/gpgpu-sim" \
  "$root/tests/l2_char/test_l2_char_stats.cc" \
  "$root/src/gpgpu-sim/l2-char-stats.cc" -o "$out"
"$out"
echo "C12 PASS: exact nearest-rank occupancy percentiles"
