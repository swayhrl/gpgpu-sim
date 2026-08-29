#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
"$root/tests/ep_l2/run_descriptor_mshr.sh"
"$root/tests/ep_l2/run_wad.sh"
"$root/tests/ep_l2/run_payload_store.sh"
"$root/tests/ep_l2/run_payload_banked.sh"
"$root/tests/ep_l2/run_epl2_schema.sh"
"$root/tests/ep_l2/run_descriptor_mshr_integrated.sh"
echo "EP-L2 C3-C7 production closeout regressions: PASS"
