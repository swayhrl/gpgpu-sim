#!/usr/bin/env python3
"""Collect and summarize W20 full-suite runner outputs."""
from __future__ import annotations
import argparse
import csv
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
COMMON_COLLECTOR = REPO / 'experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py'
FULL_MANIFEST = REPO / 'experiments/suites/common/full_suite_manifest.csv'
RESULT_FIELDS_EXTRA = ['suite_id','benchmark_name','availability_status','current_ready','command_status','smoke_status_w20','correctness_status_w20','suite_classification','suite_correctness_pass']


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline='') as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator='\n')
        writer.writeheader(); writer.writerows(rows)


def suite_classification(row: dict[str, str]) -> tuple[str, str, str]:
    raw = row.get('classification', '')
    prelim = row.get('result_status_prelim', '')
    current_ready = row.get('current_ready', '')
    explicit = row.get('explicit_pass', '') == '1'
    if prelim == 'skipped_placeholder':
        return 'skipped_placeholder', 'not_run', '0'
    if raw == 'no_stats_exit0' and row.get('exit_code') == '0':
        return 'dry_run_or_no_stats_exit0', 'not_correctness_pass', '0'
    if raw == 'completed_explicit_pass' or explicit:
        return 'completed_explicit_pass', 'explicit_pass', '1'
    if raw in {'completed_stats_found', 'completed_no_explicit_pass'}:
        return raw, 'no_explicit_pass', '0'
    if raw:
        return raw, 'failed_or_unavailable', '0'
    return 'unknown', 'unknown', '0'


def summarize_markdown(rows: list[dict[str, str]], out: Path) -> None:
    by_raw = Counter(r.get('classification','') for r in rows)
    by_suite = Counter(r.get('suite_classification','') for r in rows)
    with out.open('w') as f:
        f.write('# W20 Full-Suite Summary\n\n')
        f.write(f'rows: {len(rows)}\n\n')
        f.write('## Raw Collector Classification\n\n')
        for k,v in sorted(by_raw.items()): f.write(f'- {k}: {v}\n')
        f.write('\n## Suite Classification\n\n')
        for k,v in sorted(by_suite.items()): f.write(f'- {k}: {v}\n')
        f.write('\nCorrectness pass requires explicit pass evidence. completed_no_explicit_pass is not counted as correctness pass.\n')


def write_status_matrix(rows: list[dict[str, str]], out: Path) -> None:
    fields = ['app_id','suite_id','benchmark_name','availability_status','current_ready','classification','suite_classification','suite_correctness_pass']
    matrix = []
    for r in rows:
        matrix.append({
            'app_id': r.get('paper_id',''),
            'suite_id': r.get('suite_id',''),
            'benchmark_name': r.get('benchmark_name',''),
            'availability_status': r.get('availability_status',''),
            'current_ready': r.get('current_ready',''),
            'classification': r.get('classification',''),
            'suite_classification': r.get('suite_classification',''),
            'suite_correctness_pass': r.get('suite_correctness_pass',''),
        })
    write_csv(out, matrix, fields)


def write_suite_summary(rows: list[dict[str, str]], out: Path) -> None:
    fields = ['suite_id','rows','current_ready_rows','completed_explicit_pass','completed_no_explicit_pass','failed_or_unavailable','dryrun_or_no_stats']
    by_suite: dict[str, list[dict[str,str]]] = defaultdict(list)
    for r in rows: by_suite[r.get('suite_id','unknown')].append(r)
    out_rows=[]
    for suite, rs in sorted(by_suite.items()):
        out_rows.append({
            'suite_id': suite,
            'rows': str(len(rs)),
            'current_ready_rows': str(sum(r.get('current_ready')=='1' for r in rs)),
            'completed_explicit_pass': str(sum(r.get('suite_classification')=='completed_explicit_pass' for r in rs)),
            'completed_no_explicit_pass': str(sum(r.get('suite_classification')=='completed_no_explicit_pass' for r in rs)),
            'failed_or_unavailable': str(sum(r.get('suite_correctness_status','')=='failed_or_unavailable' or r.get('suite_classification') in {'timeout','crash_assert','simulator_error_no_stats','nonzero_no_stats'} for r in rs)),
            'dryrun_or_no_stats': str(sum(r.get('suite_classification')=='dry_run_or_no_stats_exit0' for r in rs)),
        })
    write_csv(out, out_rows, fields)


def write_blocker_ready(full_manifest: list[dict[str,str]], rows: list[dict[str,str]], outdir: Path) -> None:
    blocker_fields = ['availability_status','count','app_ids','next_action']
    blockers = defaultdict(list)
    for r in full_manifest:
        if r.get('current_ready') != '1': blockers[r.get('availability_status','unknown')].append(r)
    write_csv(outdir / 'blocker_summary.csv', [
        {'availability_status': k, 'count': str(len(v)), 'app_ids': ';'.join(r['app_id'] for r in v), 'next_action': 'see full_suite_blocker_manifest.csv'}
        for k,v in sorted(blockers.items())
    ], blocker_fields)
    ready_fields = ['app_id','suite_id','benchmark_name','smoke_result','correctness_status','gpu_tot_sim_cycle']
    by_app = {r.get('paper_id',''): r for r in rows}
    ready_rows=[]
    for r in full_manifest:
        if r.get('current_ready') == '1':
            rr = by_app.get(r['app_id'], {})
            ready_rows.append({
                'app_id': r['app_id'], 'suite_id': r['suite_id'], 'benchmark_name': r['benchmark_name'],
                'smoke_result': rr.get('suite_classification', 'not_run_in_this_collection'),
                'correctness_status': rr.get('correctness_status_w20', r.get('correctness_status','unknown')),
                'gpu_tot_sim_cycle': rr.get('gpu_tot_sim_cycle',''),
            })
    write_csv(outdir / 'ready_summary.csv', ready_rows, ready_fields)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_outdir', type=Path)
    args = parser.parse_args()
    if not (args.run_outdir / 'run_manifest.csv').exists():
        raise SystemExit(f'missing run_manifest.csv in {args.run_outdir}')
    subprocess.run([sys.executable, str(COMMON_COLLECTOR), str(args.run_outdir)], check=True)
    raw_rows = read_csv(args.run_outdir / 'results.csv')
    full = read_csv(FULL_MANIFEST)
    full_by_id = {r['app_id']: r for r in full}
    rows=[]
    for r in raw_rows:
        meta = full_by_id.get(r.get('paper_id',''), {})
        out = dict(r)
        out.update({
            'suite_id': meta.get('suite_id','unknown'),
            'benchmark_name': meta.get('benchmark_name','unknown'),
            'availability_status': meta.get('availability_status','unknown'),
            'current_ready': meta.get('current_ready','0'),
            'command_status': meta.get('command_status','unknown'),
        })
        sc, correctness, pass_flag = suite_classification(out)
        out['smoke_status_w20'] = out.get('classification','unknown')
        out['correctness_status_w20'] = correctness
        out['suite_classification'] = sc
        out['suite_correctness_pass'] = pass_flag
        rows.append(out)
    fields = list(raw_rows[0].keys()) + [f for f in RESULT_FIELDS_EXTRA if f not in raw_rows[0]] if raw_rows else RESULT_FIELDS_EXTRA
    write_csv(args.run_outdir / 'results.csv', rows, fields)
    summarize_markdown(rows, args.run_outdir / 'summary.md')
    write_status_matrix(rows, args.run_outdir / 'status_matrix.csv')
    write_suite_summary(rows, args.run_outdir / 'suite_summary.csv')
    write_blocker_ready(full, rows, args.run_outdir)
    print(f'rows={len(rows)}')
    print(f'results={args.run_outdir / "results.csv"}')
    print(f'summary={args.run_outdir / "summary.md"}')
    print(f'status_matrix={args.run_outdir / "status_matrix.csv"}')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
