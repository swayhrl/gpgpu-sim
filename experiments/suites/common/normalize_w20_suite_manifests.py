#!/usr/bin/env python3
"""Normalize W19 Rodinia/Parboil manifests into W20 full-suite manifests."""
from __future__ import annotations
import csv
from collections import Counter, defaultdict
from pathlib import Path

OUT = Path('experiments/suites/common')
RODINIA = OUT / 'rodinia_full_manifest.csv'
PARBOIL = OUT / 'parboil_full_manifest.csv'
W19_COMMAND = OUT / 'suite_command_manifest.csv'
W19_SMOKE = OUT / 'w19_smoke_results.csv'
BINARY = OUT / 'binary_recovery_results.csv'
DATA = OUT / 'data_availability_results.csv'
TABLE_CMD = Path('experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv')

FIELDS = [
    'suite_id','suite_name','app_id','app_name','benchmark_name','variant','source_path','binary_path','data_path',
    'build_command','run_command','wrapper_path','wrapper_status','availability_status','build_status','data_status',
    'command_status','smoke_status','correctness_status','input_scale','default_timeout_sec','current_ready',
    'table_iii_related','table_iii_paper_ids','phase_mapping_status','blocker','next_action','notes'
]
COMMAND_FIELDS = [
    'app_id','app_name','suite_id','benchmark_name','current_ready','wrapper_status','availability_status','run_command',
    'source_path','binary_path','data_path','input_scale','default_timeout_sec','blocker','next_action','notes'
]
QUALITY_FIELDS = ['check_name','status','count','detail','recommended_action']

TABLE_ALIAS = {
    'backprop': ['bp_1','bp_2'],
    'bfs': ['bfs'],
    'histo': ['histo_1','histo_2','histo_3'],
    'kmeans': ['kmeans_1','kmeans_2'],
    'pathfinder': ['pathfinder'],
    'srad': ['srad_1','srad_2'],
    'srad_v2': ['srad_1','srad_2'],
    'spmv': ['spmv'],
    'mri-q': ['mri_q'],
    'mri_q': ['mri_q'],
    'sgemm': ['sgemm'],
    'stencil': ['stencil'],
    'cutcp': ['cutcp'],
    'lbm': ['lbm'],
    'mri-gridding': ['mrig_1','mrig_2','mrig_3'],
    'sad': ['sad_1','sad_2'],
    'tpacf': ['tpacf'],
}


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline='') as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


def safe(value: str | None) -> str:
    return value if value not in {None, ''} else 'unknown'


def classify_availability(w19_status: str, smoke: dict[str, str], binary_status: str) -> tuple[str, str, str, str]:
    smoke_class = smoke.get('classification', '')
    if smoke_class == 'completed_explicit_pass':
        return 'ready', 'command_verified', 'completed_explicit_pass', 'explicit_pass'
    if smoke_class:
        return 'failed_smoke', 'command_unverified', smoke_class, 'failed'
    if w19_status == 'ready':
        return 'command_verified', 'command_verified', 'not_run_w20', 'unknown'
    if w19_status == 'binary_available_command_unverified' or binary_status == 'binary_ready':
        return 'binary_available_command_unverified', 'command_unverified', 'not_run', 'unknown'
    if w19_status == 'source_available_missing_binary':
        return 'source_available_missing_binary', 'unknown', 'not_run', 'unknown'
    return w19_status or 'unknown', 'unknown', 'not_run', 'unknown'


def table_ids(benchmark: str) -> list[str]:
    key = benchmark.replace('_', '-')
    ids = TABLE_ALIAS.get(benchmark, TABLE_ALIAS.get(key, []))
    return ids


def main() -> int:
    base_rows = read_csv(RODINIA) + read_csv(PARBOIL)
    command_by_id = {r['suite_workload_id']: r for r in read_csv(W19_COMMAND)}
    smoke_by_id = {r['suite_workload_id']: r for r in read_csv(W19_SMOKE)}
    binary_by_id = {r['suite_workload_id']: r for r in read_csv(BINARY)}
    data_by_id = {r['suite_workload_id']: r for r in read_csv(DATA)}
    table_present = {r.get('paper_id','') for r in read_csv(TABLE_CMD)}

    rows: list[dict[str, str]] = []
    for src in base_rows:
        app_id = src['suite_workload_id']
        cmd = command_by_id.get(app_id, {})
        smoke = smoke_by_id.get(app_id, {})
        binary = binary_by_id.get(app_id, {})
        data = data_by_id.get(app_id, {})
        availability, command_status, smoke_status, correctness = classify_availability(
            src.get('availability_status', ''), smoke, binary.get('recovery_status', '')
        )
        current_ready = '1' if smoke.get('classification') == 'completed_explicit_pass' else '0'
        ids = [i for i in table_ids(src['benchmark']) if not table_present or i in table_present]
        blocker = src.get('blocker', '')
        if availability == 'failed_smoke':
            blocker = f"W19 smoke failed: {smoke.get('classification','unknown')}"
        elif availability == 'source_available_missing_binary' and not blocker:
            blocker = 'source exists but no verified executable binary'
        elif availability == 'binary_available_command_unverified' and not blocker:
            blocker = 'binary exists but command/data/smoke are unverified'
        row = {
            'suite_id': src['suite'],
            'suite_name': 'Rodinia' if src['suite'] == 'rodinia' else 'Parboil',
            'app_id': app_id,
            'app_name': app_id,
            'benchmark_name': src['benchmark'],
            'variant': 'default',
            'source_path': safe(src.get('source_dir')),
            'binary_path': safe(src.get('binary_path')),
            'data_path': safe(src.get('data_path')),
            'build_command': safe(src.get('makefile_path')),
            'run_command': safe(cmd.get('run_command') or src.get('run_command')),
            'wrapper_path': 'generated_by_run_full_suite_matrix',
            'wrapper_status': 'ready' if current_ready == '1' else 'placeholder_unavailable',
            'availability_status': availability,
            'build_status': binary.get('recovery_status') or ('ready_existing_manifest' if src.get('availability_status') == 'ready' else 'unknown'),
            'data_status': data.get('data_status') or ('data_available' if src.get('data_exists') == '1' else 'missing_or_unverified_data'),
            'command_status': command_status,
            'smoke_status': smoke_status,
            'correctness_status': correctness,
            'input_scale': cmd.get('input_size') or 'unknown',
            'default_timeout_sec': cmd.get('timeout_sec') or '1200',
            'current_ready': current_ready,
            'table_iii_related': '1' if ids else '0',
            'table_iii_paper_ids': ';'.join(ids) if ids else 'none',
            'phase_mapping_status': 'inferred_order_or_existing' if ids else 'not_applicable',
            'blocker': blocker or 'none',
            'next_action': src.get('next_action') or cmd.get('next_action') or 'unknown',
            'notes': src.get('notes') or cmd.get('notes') or 'unknown',
        }
        rows.append(row)

    ready_rows = [r for r in rows if r['current_ready'] == '1']
    blocker_rows = [r for r in rows if r['current_ready'] != '1']
    command_rows = [
        {field: r[field] for field in COMMAND_FIELDS}
        for r in rows
    ]
    write_csv(OUT / 'full_suite_manifest.csv', rows, FIELDS)
    write_csv(OUT / 'full_suite_command_manifest.csv', command_rows, COMMAND_FIELDS)
    write_csv(OUT / 'full_suite_ready_manifest.csv', ready_rows, FIELDS)
    write_csv(OUT / 'full_suite_blocker_manifest.csv', blocker_rows, FIELDS)

    dup_app = sum(c > 1 for c in Counter(r['app_id'] for r in rows).values())
    wrapper_counts = Counter(r['wrapper_path'] for r in rows if r['wrapper_path'] != 'unknown')
    quality = [
        ('row_count_total','pass',len(rows),'all normalized rows','none'),
        ('rodinia_rows','pass',sum(r['suite_id']=='rodinia' for r in rows),'Rodinia rows','none'),
        ('parboil_rows','pass',sum(r['suite_id']=='parboil' for r in rows),'Parboil rows','none'),
        ('ready_rows','pass',len(ready_rows),'current smoke-ready rows','use as W20 baseline'),
        ('missing_binary_rows','warn',sum(r['availability_status'] in {'source_available_missing_binary','missing_binary'} for r in rows),'rows without verified binary','W21 build recovery'),
        ('missing_source_rows','pass',sum(r['availability_status']=='missing_source' for r in rows),'missing source rows','audit local source if nonzero'),
        ('failed_build_rows','warn',sum(r['build_status']=='missing_binary' for r in rows),'build/binary recovery missing rows','W21 build recovery'),
        ('command_unverified_rows','warn',sum(r['command_status']=='command_unverified' for r in rows),'command unverified rows','normalize command and smoke'),
        ('rows_missing_wrapper_path','pass',sum(r['wrapper_path']=='unknown' for r in rows),'rows without wrapper path','none'),
        ('rows_missing_notes','pass',sum(r['notes']=='unknown' for r in rows),'rows with unknown notes','fill notes later'),
        ('duplicate_app_ids','pass' if dup_app==0 else 'fail',dup_app,'duplicate app_id groups','fix app_id'),
        ('duplicate_wrapper_paths','warn',sum(c>1 for c in wrapper_counts.values()),'generated wrapper path is shared by design','runner generates per-row wrappers'),
        ('table_iii_related_rows','pass',sum(r['table_iii_related']=='1' for r in rows),'rows related to Mascar Table III','do not overwrite Table III manifests'),
    ]
    write_csv(OUT / 'full_suite_manifest_quality_report.csv', [
        {'check_name': a, 'status': b, 'count': str(c), 'detail': d, 'recommended_action': e}
        for a,b,c,d,e in quality
    ], QUALITY_FIELDS)
    print(f"full_suite_rows={len(rows)}")
    print(f"ready_rows={len(ready_rows)}")
    print(f"blocker_rows={len(blocker_rows)}")
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
