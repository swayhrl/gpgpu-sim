#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
out=${EP_L2_LANE2_BUILD:-"$root/build/ft20261016_lane2_directed"}
mkdir -p "$out"
cuda_path=${CUDA_INSTALL_PATH:-/usr/local/cuda-11.8}
flags=(-std=c++11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare
       -Wno-unused-variable -Wno-deprecated-copy -O1 -g
       -I"$cuda_path/include" -I"$root/src" -I"$root")
if [[ ${EP_L2_SANITIZE:-0} == 1 ]]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
"${CXX:-g++}" "${flags[@]}" "$root/tests/ep_l2/test_tvd_directory.cc" -o "$out/test_tvd_directory"
"$out/test_tvd_directory"
