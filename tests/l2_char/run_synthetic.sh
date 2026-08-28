#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build_dir=${TMPDIR:-/tmp}/l2-char-synthetic-$$
trap 'rm -rf "$build_dir"' EXIT
mkdir -p "$build_dir"

${CXX:-g++} -std=c++0x -Wall -Wextra -Werror \
  -I"$repo_root/src/gpgpu-sim" \
  "$repo_root/tests/l2_char/test_corrected_l2_rules.cc" \
  -o "$build_dir/test_corrected_l2_rules"
"$build_dir/test_corrected_l2_rules"
