#!/usr/bin/env python3
import json
import pathlib
import subprocess
import sys

BASELINE = "57bb71ecd015b6ec0ab32e45b0815e5beaf69172"


def function(text: str, marker: str) -> str:
    start = text.find(marker)
    if start < 0:
        raise RuntimeError(f"missing function marker: {marker}")
    brace = text.find("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]
    raise RuntimeError(f"unterminated function: {marker}")


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[3]
    current = (root / "src/gpgpu-sim/gpu-cache.cc").read_text()
    baseline = subprocess.check_output(
        ["git", "-C", str(root), "show", f"{BASELINE}:src/gpgpu-sim/gpu-cache.cc"],
        text=True,
    )
    subprocess.check_call(["git", "-C", str(root), "merge-base", "--is-ancestor", BASELINE, "HEAD"])

    marker = "enum cache_request_status tag_array::probe(new_addr_type addr, unsigned &idx,\n                                           mem_access_sector_mask_t mask"
    baseline_probe = function(baseline, marker)
    candidate_probe = function(current, marker)
    opt_in = (
        "  if (m_config.oracle_elastic_enabled())\n"
        "    return oracle_probe(addr, idx, mask, is_write, probe_mode, mf);\n"
    )
    if candidate_probe.count(opt_in) != 1:
        raise RuntimeError("candidate probe lacks one centralized opt-in branch")
    if candidate_probe.replace(opt_in, "") != baseline_probe:
        raise RuntimeError("feature-OFF baseline probe body drift")

    exact_functions = [
        "enum cache_request_status data_cache::process_tag_probe(",
        "enum cache_request_status data_cache::access(new_addr_type addr, mem_fetch *mf,",
    ]
    for item in exact_functions:
        if function(current, item) != function(baseline, item):
            raise RuntimeError(f"baseline cache pipeline drift: {item}")

    changed = subprocess.check_output(
        ["git", "-C", str(root), "diff", "--name-only", BASELINE, "HEAD"], text=True
    ).splitlines()
    allowed_source = {
        "src/gpgpu-sim/gpu-cache.cc", "src/gpgpu-sim/gpu-cache.h",
        "src/gpgpu-sim/gpu-sim.cc", "src/gpgpu-sim/gpu-sim.h",
        "src/gpgpu-sim/l2cache.cc", "src/gpgpu-sim/oracle_elastic_residency.cc",
        "src/gpgpu-sim/oracle_elastic_residency.h",
    }
    unexpected = [path for path in changed if path.startswith("src/") and
                  not path.startswith("src/gpgpu-sim/tests/") and path not in allowed_source]
    if unexpected:
        raise RuntimeError(f"unexpected functional source paths: {unexpected}")

    print(json.dumps({
        "status": "PASS",
        "baseline": BASELINE,
        "baseline_probe_body_preserved": True,
        "data_cache_probe_and_access_preserved": True,
        "centralized_opt_in_branch_count": 1,
        "unexpected_functional_source_paths": [],
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"OFF_PATH_SOURCE_NEUTRALITY_FAIL: {error}", file=sys.stderr)
        raise
