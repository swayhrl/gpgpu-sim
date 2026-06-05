#!/usr/bin/env python3
"""W17A local audit for Mascar Table III workload availability."""

from __future__ import annotations

import csv
from pathlib import Path

ROOT = Path('/workspace/repos/gpgpu-sim_distribution')
COMMAND = ROOT / 'experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv'
AUDITED = ROOT / 'experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest_audited.csv'
OUT = ROOT / 'experiments/paper-mascar/workloads/matrix/W17'
AUDIT = ROOT / 'experiments/paper-mascar/workloads/audit/W17'

ALIASES = {
    'bp_1': ['backprop', 'bp'], 'bp_2': ['backprop', 'bp'], 'bfs': ['bfs'],
    'histo_1': ['histo', 'histogram'], 'histo_2': ['histo', 'histogram'], 'histo_3': ['histo', 'histogram'],
    'histogram': ['histogram', 'histo'], 'kmeans_1': ['kmeans'], 'kmeans_2': ['kmeans'],
    'lbm': ['lbm'], 'leuko_1': ['leukocyte', 'leuko'], 'leuko_2': ['leukocyte', 'leuko'], 'leuko_3': ['leukocyte', 'leuko'],
    'mri_q': ['mri-q', 'mriq'], 'mrig_1': ['mri-gridding', 'mrig', 'gridding'], 'mrig_2': ['mri-gridding', 'mrig', 'gridding'], 'mrig_3': ['mri-gridding', 'mrig', 'gridding'],
    'mummer': ['mummergpu', 'mummer'], 'particle': ['particlefilter', 'particle'],
    'sad_1': ['sad'], 'sad_2': ['sad'], 'spmv': ['spmv'], 'srad_1': ['srad'], 'srad_2': ['srad'],
    'stencil': ['stencil'], 'sgemm': ['sgemm'], 'tpacf': ['tpacf'], 'cutcp': ['cutcp'], 'lavamd': ['lavaMD', 'lavamd', 'lava'], 'pathfinder': ['pathfinder'],
}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline='') as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator='\n')
        w.writeheader(); w.writerows(rows)


def exists(p: str) -> bool:
    return bool(p) and Path(p).exists()


def any_file(base: Path, names: set[str]) -> bool:
    if not base.exists() or not base.is_dir():
        return False
    for p in base.rglob('*'):
        if any(skip in p.parts for skip in {'.git', '__pycache__', 'runs', 'results'}):
            continue
        if p.name in names:
            return True
    return False


def any_suffix(base: Path, suffixes: tuple[str, ...]) -> bool:
    if not base.exists() or not base.is_dir():
        return False
    for p in base.rglob('*'):
        if any(skip in p.parts for skip in {'.git', '__pycache__', 'runs', 'results'}):
            continue
        if p.is_file() and p.suffix in suffixes:
            return True
    return False


def has_executable(path: Path) -> bool:
    if path.is_file():
        return path.exists() and path.stat().st_mode & 0o111 != 0
    if path.is_dir():
        for p in path.rglob('*'):
            if any(skip in p.parts for skip in {'.git', '__pycache__', 'runs', 'results'}):
                continue
            if p.is_file() and p.stat().st_mode & 0o111 and p.suffix not in {'.sh', '.py', '.pl'} and p.name.lower() not in {'makefile'}:
                return True
    return False


def candidate_build(source: str, paper_id: str) -> str:
    if not source:
        return ''
    p = Path(source)
    if not p.exists():
        return ''
    if 'parboil/benchmarks' in source:
        if (p / 'src/cuda/Makefile').exists():
            return f'make -C {p / "src/cuda"}'
        if (p / 'src/cuda_base/Makefile').exists():
            return f'make -C {p / "src/cuda_base"}'
        if (p / 'src/cuda-base/Makefile').exists():
            return f'make -C {p / "src/cuda-base"}'
    if (p / 'Makefile').exists():
        return f'make -C {p}'
    if (p / 'makefile').exists():
        return f'make -C {p} -f makefile'
    for sub in ['CUDA', 'src', 'kernel']:
        if (p / sub / 'Makefile').exists():
            return f'make -C {p / sub}'
    return ''


def classify_gap(cmd: dict[str, str], aud: dict[str, str]) -> tuple[str, str, str]:
    ws = cmd.get('wrapper_status', '')
    av = cmd.get('availability_status', '')
    source = aud.get('local_path', '')
    binary = aud.get('local_binary', '')
    if ws == 'ready' and av == 'available':
        return 'already_ready', '', 'exact' if aud.get('phase_mapping_status') == 'exact' else aud.get('phase_mapping_status', 'inferred')
    if ws == 'placeholder_phase_unknown' or av == 'partial_phase_unknown':
        return 'phase_unknown', 'exact_phase_mapping_missing', 'app_level_pending_kernel_trace'
    if not source and 'missing_source' in (ws + av + aud.get('availability_status', '')):
        return 'missing_source', 'candidate_source_not_found', 'unavailable'
    if source and not exists(binary):
        return 'missing_binary', 'data_or_wrapper_may_also_be_missing', 'app_level_pending_kernel_trace'
    if not cmd.get('run_command'):
        return 'command_unknown', 'wrapper_placeholder', 'unavailable'
    return 'unknown', '', 'phase_unknown'


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True); AUDIT.mkdir(parents=True, exist_ok=True)
    cmd_rows = read_csv(COMMAND)
    aud_by_id = {r['paper_id']: r for r in read_csv(AUDITED)}
    candidates: list[dict[str, str]] = []
    gaps: list[dict[str, str]] = []
    actions: list[dict[str, str]] = []
    paths: list[str] = []
    for row in cmd_rows:
        pid = row['paper_id']; aud = aud_by_id.get(pid, {})
        source = aud.get('local_path', '')
        binary = aud.get('local_binary', '')
        data = aud.get('local_data_path', '')
        aliases = ALIASES.get(pid, [pid])
        src_path = Path(source) if source else Path()
        has_source = '1' if exists(source) else '0'
        has_binary = '1' if exists(binary) or (exists(source) and has_executable(src_path)) else '0'
        has_makefile = '1' if exists(source) and any_file(src_path, {'Makefile', 'makefile'}) else '0'
        has_cmake = '1' if exists(source) and any_file(src_path, {'CMakeLists.txt'}) else '0'
        has_build_script = '1' if exists(source) and any_suffix(src_path, ('.sh',)) else '0'
        has_data = '1' if exists(data) or (exists(source) and any(p.name.lower() in {'data', 'input', 'inputs', 'datasets'} for p in src_path.rglob('*') if p.is_dir())) else '0'
        if source:
            paths.append(f'{pid},source_dir,{source}')
            candidates.append({
                'paper_id': pid, 'paper_name': row['paper_name'], 'paper_type': row['paper_type'],
                'current_availability_status': row['availability_status'], 'current_wrapper_status': row['wrapper_status'],
                'current_phase_mapping_status': aud.get('phase_mapping_status', ''), 'alias_used': '|'.join(aliases),
                'candidate_kind': 'source_dir', 'candidate_path': source, 'has_source': has_source, 'has_binary': has_binary,
                'has_makefile': has_makefile, 'has_cmake': has_cmake, 'has_build_script': has_build_script, 'has_data': has_data,
                'likely_suite': aud.get('local_suite_guess', ''), 'confidence': 'high' if has_source == '1' else 'low',
                'notes': aud.get('notes', ''),
            })
        if binary:
            candidates.append({
                'paper_id': pid, 'paper_name': row['paper_name'], 'paper_type': row['paper_type'],
                'current_availability_status': row['availability_status'], 'current_wrapper_status': row['wrapper_status'],
                'current_phase_mapping_status': aud.get('phase_mapping_status', ''), 'alias_used': '|'.join(aliases),
                'candidate_kind': 'binary_file', 'candidate_path': binary, 'has_source': has_source, 'has_binary': '1' if exists(binary) else '0',
                'has_makefile': has_makefile, 'has_cmake': has_cmake, 'has_build_script': has_build_script, 'has_data': has_data,
                'likely_suite': aud.get('local_suite_guess', ''), 'confidence': 'high' if exists(binary) else 'medium',
                'notes': 'existing binary path from audited manifest',
            })
        primary, secondary, phase_status = classify_gap(row, aud)
        build_cmd = candidate_build(source, pid)
        if primary == 'already_ready':
            w17b = 'none'; w17c = 'keep_existing_ready_wrapper'
        elif primary == 'phase_unknown':
            w17b = 'none'; w17c = 'convert_placeholder_to_app_level_ready_phase_pending'
        elif primary == 'missing_binary' and build_cmd:
            w17b = 'attempt_native_build_then_arch_adjustment'; w17c = 'create_ready_wrapper_if_binary_and_data_available'
        elif primary == 'missing_source':
            w17b = 'search_local_source_aliases_only'; w17c = 'keep_unavailable_exit77'
        else:
            w17b = 'audit_manual'; w17c = 'keep_or_fix_placeholder'
        gaps.append({
            'paper_id': pid, 'paper_name': row['paper_name'], 'paper_type': row['paper_type'],
            'current_status': row['wrapper_status'], 'primary_gap': primary, 'secondary_gap': secondary,
            'candidate_source': source, 'candidate_binary': binary, 'candidate_data': data,
            'candidate_build_command': build_cmd, 'phase_mapping_status': phase_status,
            'recommended_W17B_action': w17b, 'recommended_W17C_action': w17c,
            'notes': row.get('notes', ''),
        })
        priority = '5'
        if primary == 'missing_binary' and source and build_cmd:
            priority = '1'
        elif primary == 'phase_unknown':
            priority = '4'
        elif primary == 'already_ready':
            priority = '9'
        action_type = w17b if primary == 'missing_binary' else w17c
        actions.append({
            'priority': priority, 'paper_id': pid, 'action_type': action_type,
            'action_detail': build_cmd or w17c, 'expected_risk': 'medium' if primary == 'missing_binary' else 'low',
            'expected_time': 'minutes' if primary in {'phase_unknown', 'already_ready'} else '10-30min',
            'can_attempt_in_W17B': '1' if primary == 'missing_binary' and build_cmd else '0',
            'can_attempt_in_W17C': '1' if primary in {'phase_unknown', 'already_ready', 'missing_binary'} else '0',
            'notes': f'{primary}; {secondary}',
        })
    write_csv(OUT / 'w17a_candidate_inventory.csv', candidates, ['paper_id','paper_name','paper_type','current_availability_status','current_wrapper_status','current_phase_mapping_status','alias_used','candidate_kind','candidate_path','has_source','has_binary','has_makefile','has_cmake','has_build_script','has_data','likely_suite','confidence','notes'])
    write_csv(OUT / 'w17a_gap_matrix.csv', gaps, ['paper_id','paper_name','paper_type','current_status','primary_gap','secondary_gap','candidate_source','candidate_binary','candidate_data','candidate_build_command','phase_mapping_status','recommended_W17B_action','recommended_W17C_action','notes'])
    write_csv(OUT / 'w17a_next_action_plan.csv', sorted(actions, key=lambda r: (int(r['priority']), r['paper_id'])), ['priority','paper_id','action_type','action_detail','expected_risk','expected_time','can_attempt_in_W17B','can_attempt_in_W17C','notes'])
    (AUDIT / 'w17a_candidate_paths.txt').write_text('\n'.join(paths) + '\n')
    if len(gaps) != 30:
        raise SystemExit(f'expected 30 gap rows, got {len(gaps)}')
    print(f'rows={len(gaps)}')
    print(f'candidates={len(candidates)}')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
