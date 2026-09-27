#!/usr/bin/env python3
"""Directed source contract for disabled PTX/source-line statistics."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
STATS = (ROOT / "src/cuda-sim/ptx-stats.cc").read_text()
EXEC = (ROOT / "src/cuda-sim/cuda-sim.cc").read_text()

methods = (
    "ptx_file_line_stats_add_latency",
    "ptx_file_line_stats_add_dram_traffic",
    "ptx_file_line_stats_add_smem_bank_conflict",
    "ptx_file_line_stats_add_uncoalesced_gmem",
    "ptx_file_line_stats_add_inflight_memory_insn",
    "ptx_file_line_stats_sub_inflight_memory_insn",
    "ptx_file_line_stats_add_warp_divergence",
)
for method in methods:
    pattern = rf"void ptx_stats::{method}\([^{{]*\) \{{\n  if \(!enable_ptx_file_line_stats\) return;"
    if not re.search(pattern, STATS, re.MULTILINE):
        raise SystemExit(f"missing disabled fast-path guard: {method}")

if not re.search(
    r"if \(!\(this->m_functionalSimulationMode\) &&\n"
    r"\s*m_gpu->gpgpu_ctx->stats->enable_ptx_file_line_stats\)\n"
    r"\s*ptx_file_line_stats_add_exec_count\(pI\);",
    EXEC,
):
    raise SystemExit("execution-count hot path is not guarded")

if not re.search(
    r'"-enable_ptx_file_line_stats"[^;]*"1"\);', STATS, re.DOTALL
):
    raise SystemExit("default enable_ptx_file_line_stats behavior changed")

print("PTX_STATS_DISABLE_GUARD_TEST_PASS")
