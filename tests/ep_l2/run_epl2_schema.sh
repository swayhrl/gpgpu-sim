#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); out=${TMPDIR:-/tmp}/test_epl2_schema
${CXX:-g++} -std=c++11 -O2 "$root/tests/ep_l2/test_epl2_schema.cc" -o "$out"; "$out"
