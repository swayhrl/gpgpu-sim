#!/usr/bin/env python3
"""Mascar Table III wrapper around the common GPGPU-Sim matrix collector."""

from __future__ import annotations

import runpy
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
COMMON_COLLECTOR = REPO_ROOT / "experiments" / "common" / "gpgpusim_matrix" / "collect_gpgpusim_stats.py"


if __name__ == "__main__":
    sys.argv[0] = str(COMMON_COLLECTOR)
    runpy.run_path(str(COMMON_COLLECTOR), run_name="__main__")
