#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
AUDIT = ROOT / "experiments/paper-mascar/energy/W25E/audit"
PATTERNS = ["kernel_avg_power", "gpu_tot_avg_power", "kernel_max_power", "gpu_tot_max_power", "AccelWattch", "power", "gpuwattch", "energy", "total power", "average power"]
SUFFIXES = {".log", ".txt", ".out", ".err", ".csv", ".md"}
NAME_MARKERS = ("power", "watt", "accelwattch", "gpuwattch")


def text_hits(path: Path) -> list[str]:
    if not path.is_file():
        return []
    lower = path.name.lower()
    if path.suffix.lower() not in SUFFIXES and not any(m in lower for m in NAME_MARKERS):
        return []
    try:
        if path.stat().st_size > 20 * 1024 * 1024:
            return []
        text = path.read_text(errors="replace")
    except OSError:
        return []
    return [p for p in PATTERNS if p.lower() in text.lower()]


def scan_tree(label: str, path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    if not path.exists():
        return rows
    for p in sorted(path.rglob("*")):
        if not p.is_file():
            continue
        hits = text_hits(p)
        name_hit = [m for m in NAME_MARKERS if m in p.name.lower()]
        if hits or name_hit:
            rows.append({"source": label, "path": str(p), "kind": "file", "patterns": "|".join(sorted(set(hits + name_hit))), "size_bytes": str(p.stat().st_size)})
    return rows


def scan_tar(label: str, path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    if not path.exists():
        return rows
    with tarfile.open(path, "r:gz") as tf:
        for member in tf.getmembers():
            if not member.isfile():
                continue
            lower = member.name.lower()
            name_hit = [m for m in NAME_MARKERS if m in lower]
            if Path(member.name).suffix.lower() not in SUFFIXES and not name_hit:
                continue
            if member.size > 20 * 1024 * 1024:
                continue
            f = tf.extractfile(member)
            if f is None:
                continue
            text = f.read().decode("utf-8", errors="replace")
            hits = [p for p in PATTERNS if p.lower() in text.lower()]
            if hits or name_hit:
                rows.append({"source": label, "path": f"{path}:{member.name}", "kind": "tar_member", "patterns": "|".join(sorted(set(hits + name_hit))), "size_bytes": str(member.size)})
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--extra", action="append", default=[], help="extra file or directory to scan")
    args = parser.parse_args()
    AUDIT.mkdir(parents=True, exist_ok=True)
    roots = [
        ("w16_results", ROOT / "experiments/paper-mascar/energy/W16C"),
        ("w25_results", ROOT / "experiments/paper-mascar/energy/W25"),
        ("w25e_results", ROOT / "experiments/paper-mascar/energy/W25E/results"),
    ]
    rows: list[dict[str, str]] = []
    for label, path in roots:
        rows.extend(scan_tree(label, path))
    for p in Path("/workspace/tmp").glob("*w16*energy*.tar.gz"):
        rows.extend(scan_tar("w16_tmp_archive", p))
    for p in Path("/workspace/tmp").glob("*w25*energy*.tar.gz"):
        rows.extend(scan_tar("w25_tmp_archive", p))
    for extra in args.extra:
        p = Path(extra)
        rows.extend(scan_tree("extra", p) if p.is_dir() else scan_tar("extra_tar", p) if p.suffixes[-2:] == [".tar", ".gz"] else scan_tree("extra_parent", p.parent))
    with (AUDIT / "w25e_energy_output_hits.csv").open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["source", "path", "kind", "patterns", "size_bytes"], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    power_rows = [r for r in rows if "kernel_avg_power" in r["patterns"] or "gpu_tot_avg_power" in r["patterns"]]
    summary = ["# W25E Energy Output Summary", "", f"total_hits={len(rows)}", f"power_field_hits={len(power_rows)}", "", "## Interpretation", "", "W16 archives/results contain AccelWattch power report hits. W25 stable CSVs lacked power fields; W25 raw archive is scanned when available to determine whether artifacts existed outside collector scope.", ""]
    for r in power_rows[:20]:
        summary.append(f"- {r['source']}: {r['path']} ({r['patterns']})")
    (AUDIT / "w25e_energy_output_summary.md").write_text("\n".join(summary) + "\n", encoding="utf-8")
    print(f"energy_output_hits={len(rows)} power_field_hits={len(power_rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
