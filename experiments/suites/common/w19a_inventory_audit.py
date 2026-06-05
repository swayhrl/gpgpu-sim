#!/usr/bin/env python3
"""W19A inventory audit for local Rodinia and Parboil CUDA suites."""
from __future__ import annotations
import csv
import os
from pathlib import Path

WORKLOAD_ROOT = Path('/workspace/repos/gpgpu-workloads')
RODINIA_ROOT = WORKLOAD_ROOT / 'suites' / 'rodinia'
RODINIA_CUDA = RODINIA_ROOT / 'cuda'
RODINIA_WRAPPER = WORKLOAD_ROOT / 'suites' / 'rodinia-wrapper'
PARBOIL_ROOT = WORKLOAD_ROOT / 'suites' / 'parboil'
PARBOIL_BENCH = PARBOIL_ROOT / 'benchmarks'
PARBOIL_WRAPPER = WORKLOAD_ROOT / 'suites' / 'parboil-wrapper'
MANIFEST = WORKLOAD_ROOT / 'manifests' / 'workload_manifest.csv'
OUT_DIR = Path('experiments/suites/common')

FIELDS = [
    'suite','suite_workload_id','benchmark','source_dir','source_exists','cuda_source_count','makefile_path',
    'wrapper_dir','wrapper_exists','binary_path','binary_exists','binary_executable','data_path','data_exists',
    'data_file_count','existing_manifest_app','existing_manifest_status','run_command','availability_status',
    'blocker','next_action','notes'
]

RODINIA_ALIASES = {'srad': 'srad_v2'}
PARBOIL_ALIASES = {'mri-q': 'mri_q'}


def read_manifest() -> dict[tuple[str,str], dict[str,str]]:
    rows = {}
    if not MANIFEST.exists():
        return rows
    with MANIFEST.open(newline='') as f:
        for r in csv.DictReader(f):
            if r.get('suite') in {'rodinia','parboil'}:
                rows[(r['suite'], r['app'].replace('rodinia_','').replace('parboil_',''))] = r
                rows[(r['suite'], r['app'])] = r
    return rows


def count_cuda(path: Path) -> int:
    if not path.exists(): return 0
    return sum(1 for _ in path.rglob('*.cu'))


def find_makefile(path: Path) -> str:
    for name in ['Makefile','makefile','Makefile_nvidia']:
        p = path / name
        if p.exists(): return str(p)
    return ''


def executables(path: Path) -> list[Path]:
    if not path.exists(): return []
    bad_suffix = {'.sh','.cu','.c','.cc','.cpp','.h','.hpp','.cuh','.copy','.rules','.o','.ptx','.ptxas','.txt','.log','.config','.icnt','.mk'}
    bad_names = {'Makefile','makefile','Makefile_nvidia','README','README.txt','COPYING','LICENSE','run','run.sh','autorun.sh'}
    out=[]
    for p in path.iterdir():
        if p.is_file() and os.access(p, os.X_OK) and p.suffix not in bad_suffix and p.name not in bad_names and not p.name.startswith('_'):
            out.append(p)
    preferred = ['nn', 'gaussian', 'heartwall', 'myocyte.out', 'sc_gpu', 'pavle', '3D', 'b+tree.out']
    ranked = sorted(out, key=lambda x: (preferred.index(x.name) if x.name in preferred else 999, x.name))
    return ranked


def data_info(paths: list[Path]) -> tuple[str,bool,int]:
    best = ''
    count = 0
    for root in paths:
        if not root.exists():
            continue
        files = [p for p in root.rglob('*') if p.is_file() and '.git' not in p.parts and '.hg' not in p.parts]
        if files and not best:
            best = str(root)
        count += len(files)
    return best, count > 0, count


def manifest_lookup(rows, suite: str, benchmark: str):
    keys = [(suite, benchmark), (suite, f'{suite}_{benchmark}'), (suite, f'{suite}_{benchmark.replace("-","_")}')]
    if suite == 'rodinia' and benchmark == 'srad':
        keys += [(suite, 'rodinia_srad_v2'), (suite, 'srad_v2')]
    if suite == 'parboil' and benchmark == 'mri-q':
        keys += [(suite, 'parboil_mri_q'), (suite, 'mri_q')]
    for k in keys:
        if k in rows: return rows[k]
    return {}


def row_for(suite: str, benchmark: str, source: Path, wrapper: Path, manifest_rows) -> dict[str,str]:
    m = manifest_lookup(manifest_rows, suite, benchmark)
    wrapper_exists = wrapper.exists()
    exes = executables(wrapper)
    if m.get('binary_path') and Path(m['binary_path']).exists():
        binary = Path(m['binary_path'])
    elif exes:
        binary = exes[0]
    else:
        raw_exes = executables(source)
        binary = raw_exes[0] if raw_exes else None
    data_path, data_exists, data_count = data_info([wrapper/'data', source/'data', source/'input', source/'inputs', source/'datasets'])
    source_exists = source.exists()
    binary_exists = binary is not None and binary.exists()
    binary_exec = binary_exists and os.access(binary, os.X_OK)
    run_command = m.get('run_command','')
    if m.get('status') == 'ready' and binary_exec and run_command:
        status, blocker, next_action = 'ready', '', 'dry-run and smoke through W19C/W19D wrapper'
    elif source_exists and binary_exec:
        status, blocker, next_action = 'binary_available_command_unverified', 'command/data not normalized in workload manifest', 'derive small input command and promote after smoke'
    elif source_exists:
        status, blocker, next_action = 'source_available_missing_binary', 'no verified executable binary', 'try W19B build/recovery rounds'
    else:
        status, blocker, next_action = 'missing_source', 'source directory absent locally', 'locate local source/data without network fetch'
    return {
        'suite': suite,
        'suite_workload_id': f'{suite}_{benchmark.replace("-","_").replace("+","plus")}',
        'benchmark': benchmark,
        'source_dir': str(source) if source_exists else '',
        'source_exists': '1' if source_exists else '0',
        'cuda_source_count': str(count_cuda(source)),
        'makefile_path': find_makefile(source),
        'wrapper_dir': str(wrapper) if wrapper_exists else '',
        'wrapper_exists': '1' if wrapper_exists else '0',
        'binary_path': str(binary) if binary_exists else '',
        'binary_exists': '1' if binary_exists else '0',
        'binary_executable': '1' if binary_exec else '0',
        'data_path': data_path,
        'data_exists': '1' if data_exists else '0',
        'data_file_count': str(data_count),
        'existing_manifest_app': m.get('app',''),
        'existing_manifest_status': m.get('status',''),
        'run_command': run_command,
        'availability_status': status,
        'blocker': blocker,
        'next_action': next_action,
        'notes': 'W18 phase mapping retained as background only; not modified by W19',
    }


def write(path: Path, rows: list[dict[str,str]]):
    with path.open('w', newline='') as f:
        w=csv.DictWriter(f, fieldnames=FIELDS, lineterminator='\n')
        w.writeheader(); w.writerows(rows)


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    manifest_rows = read_manifest()
    rodinia = []
    for d in sorted(p for p in RODINIA_CUDA.iterdir() if p.is_dir() and not p.name.startswith('.') and p.name not in {'util'}):
        wrapper_name = RODINIA_ALIASES.get(d.name, d.name)
        rodinia.append(row_for('rodinia', d.name, d, RODINIA_WRAPPER / wrapper_name, manifest_rows))
    parboil = []
    for d in sorted(p for p in PARBOIL_BENCH.iterdir() if p.is_dir() and not p.name.startswith('.')):
        wrapper_name = d.name
        parboil.append(row_for('parboil', d.name, d, PARBOIL_WRAPPER / wrapper_name, manifest_rows))
    write(OUT_DIR/'rodinia_full_manifest.csv', rodinia)
    write(OUT_DIR/'parboil_full_manifest.csv', parboil)
    print(f'rodinia_rows={len(rodinia)} ready={sum(r["availability_status"]=="ready" for r in rodinia)}')
    print(f'parboil_rows={len(parboil)} ready={sum(r["availability_status"]=="ready" for r in parboil)}')
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
