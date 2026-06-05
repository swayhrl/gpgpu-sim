#!/usr/bin/env python3
from __future__ import annotations
import csv
from collections import defaultdict
from pathlib import Path

ROOT = Path('/workspace/repos/gpgpu-sim_distribution')
BASE = ROOT / 'experiments/paper-mascar/workloads'
W24 = BASE / 'matrix/W24'
AUDIT = BASE / 'audit/W24'
DOCS = ROOT / 'docs/papers'
W24.mkdir(parents=True, exist_ok=True)
AUDIT.mkdir(parents=True, exist_ok=True)
DOCS.mkdir(parents=True, exist_ok=True)

CONFIG_ROWS = [
    {'config_id':'baseline_off','config_path':'configs/hrl-repro/SM7_QV100_mascar_baseline_off','config_role':'baseline','enabled':'1','notes':'baseline Mascar disabled'},
    {'config_id':'m2_owner_sched','config_path':'configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on','config_role':'scheduling_only','enabled':'1','notes':'M2 owner scheduling'},
    {'config_id':'m3_hitonly_nack','config_path':'configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on','config_role':'scheduling_hitonly','enabled':'1','notes':'M3 hit-only NACK'},
    {'config_id':'m4_reexec_load','config_path':'configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on','config_role':'scheduling_hitonly_reexec','enabled':'1','notes':'M4 load re-execution'},
    {'config_id':'m3diag_on','config_path':'configs/hrl-repro/SM7_QV100_mascar_m3diag_on','config_role':'diagnostic_disabled','enabled':'0','notes':'disabled diagnostic row'},
]
ALL_FIELDS = ['paper_id','paper_name','paper_type','paper_suite','paper_app','paper_kernel_or_phase','paper_inst_per_l1_miss','wrapper_path','wrapper_status','availability_status','phase_mapping_status_w18','phase_mapping_confidence_w18','local_launch_index','local_kernel_name','measurement_scope','selected_for_w24','selection_reason','correctness_status_prior','notes']
RUN_PLAN_FIELDS = ['run_id','config_id','config_path','config_role','paper_id','paper_name','paper_type','wrapper_path','measurement_scope','phase_mapping_confidence_w18','selected_for_run','timeout_sec','expected_activation','duplicate_physical_group','notes']
RUNNER_FIELDS = ['paper_id','paper_name','paper_type','availability','wrapper_path','wrapper_status','build_required','build_command','run_working_dir','run_command','input_size','timeout_sec','dry_run_status','notes']
PHASE_FIELDS = ['paper_id','paper_name','phase_mapping_status_w18','phase_mapping_confidence_w18','local_launch_index','local_kernel_name','measurement_scope','evidence','notes']
GROUP_FIELDS = ['duplicate_physical_group','paper_ids','wrapper_path','run_command','row_count','notes']

def read_csv(path: Path) -> list[dict[str,str]]:
    if not path.exists():
        return []
    with path.open(newline='') as f:
        return list(csv.DictReader(f))

def write_csv(path: Path, rows: list[dict[str,str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator='\n', extrasaction='ignore')
        w.writeheader(); w.writerows(rows)

def measurement_scope(wrapper_status: str, confidence: str, phase_after: str) -> str:
    if wrapper_status != 'ready':
        return 'unavailable'
    if confidence == 'exact':
        return 'exact_kernel_run'
    if confidence == 'inferred_order' or phase_after == 'mapped_by_launch_order':
        return 'app_run_inferred_phase'
    return 'app_run_unmapped'

def expected_activation(config_id: str, paper_type: str) -> str:
    if config_id == 'baseline_off': return 'none'
    if config_id == 'm2_owner_sched': return 'M2_possible'
    if config_id == 'm3_hitonly_nack': return 'M2_M3_possible'
    if config_id == 'm4_reexec_load': return 'M2_M3_M4_possible'
    return 'disabled'

def main() -> int:
    canonical = read_csv(BASE / 'mascar_table_iii_workload_manifest.csv')
    command = {r['paper_id']: r for r in read_csv(BASE / 'mascar_table_iii_command_manifest.csv')}
    ready_w17 = {r['paper_id']: r for r in read_csv(BASE / 'matrix/W17/w17_ready_manifest.csv')}
    unavail_w17 = {r['paper_id']: r for r in read_csv(BASE / 'matrix/W17/w17_unavailable_manifest.csv')}
    phase = {r['paper_id']: r for r in read_csv(BASE / 'matrix/W18/table_iii_phase_mapping_proposed_manifest.csv')}
    if not canonical:
        raise SystemExit('missing canonical Table III manifest')
    if len(canonical) != 30:
        raise SystemExit(f'canonical manifest expected 30 rows, got {len(canonical)}')
    write_csv(W24 / 'w24_tableiii_config_matrix.csv', CONFIG_ROWS, ['config_id','config_path','config_role','enabled','notes'])
    all_rows=[]; phase_rows=[]; runner_rows=[]
    physical_key_to_group={}; group_rows=[]
    for c in canonical:
        pid=c['paper_id']
        cmd=command.get(pid,{})
        w17=ready_w17.get(pid,{})
        unavailable=unavail_w17.get(pid,{})
        ph=phase.get(pid,{})
        wrapper_status = cmd.get('wrapper_status') or w17.get('wrapper_status') or 'unavailable'
        availability = cmd.get('availability_status') or w17.get('availability_status') or unavailable.get('blocker') or c.get('availability_status','unknown')
        confidence = ph.get('mapping_confidence') or c.get('phase_mapping_status','unknown')
        phase_after = ph.get('phase_mapping_status_after_w18') or c.get('phase_mapping_status','unknown')
        scope = measurement_scope(wrapper_status, confidence, phase_after)
        selected = '1' if wrapper_status == 'ready' else '0'
        reason = 'ready_wrapper' if selected == '1' else (unavailable.get('blocker') or availability or 'unavailable')
        notes = '; '.join(x for x in [cmd.get('notes',''), w17.get('notes',''), ph.get('notes',''), unavailable.get('notes','')] if x)
        row = {
            'paper_id': pid,
            'paper_name': c.get('paper_name',''),
            'paper_type': c.get('paper_type',''),
            'paper_suite': c.get('paper_suite',''),
            'paper_app': c.get('paper_app',''),
            'paper_kernel_or_phase': c.get('paper_kernel_or_phase',''),
            'paper_inst_per_l1_miss': c.get('paper_inst_per_l1_miss',''),
            'wrapper_path': cmd.get('wrapper_path') or w17.get('wrapper_path',''),
            'wrapper_status': wrapper_status,
            'availability_status': availability,
            'phase_mapping_status_w18': phase_after,
            'phase_mapping_confidence_w18': confidence,
            'local_launch_index': ph.get('local_launch_index',''),
            'local_kernel_name': ph.get('local_kernel_name',''),
            'measurement_scope': scope,
            'selected_for_w24': selected,
            'selection_reason': reason,
            'correctness_status_prior': w17.get('correctness_status') or c.get('run_command_status','unknown'),
            'notes': notes,
        }
        all_rows.append(row)
        phase_rows.append({'paper_id':pid,'paper_name':c.get('paper_name',''),'phase_mapping_status_w18':phase_after,'phase_mapping_confidence_w18':confidence,'local_launch_index':ph.get('local_launch_index',''),'local_kernel_name':ph.get('local_kernel_name',''),'measurement_scope':scope,'evidence':ph.get('evidence',''),'notes':ph.get('notes','')})
        if selected == '1':
            key=(row['wrapper_path'], cmd.get('run_command',''))
            if key not in physical_key_to_group:
                physical_key_to_group[key]=f'phys_{len(physical_key_to_group)+1:02d}'
            runner_rows.append({
                'paper_id': pid,
                'paper_name': c.get('paper_name',''),
                'paper_type': c.get('paper_type',''),
                'availability': availability,
                'wrapper_path': row['wrapper_path'],
                'wrapper_status': 'ready',
                'build_required': cmd.get('build_required','no'),
                'build_command': cmd.get('build_command',''),
                'run_working_dir': cmd.get('run_working_dir',''),
                'run_command': cmd.get('run_command',''),
                'input_size': cmd.get('input_size','tiny'),
                'timeout_sec': '1800',
                'dry_run_status': cmd.get('dry_run_status','unknown'),
                'notes': f"measurement_scope={scope}; phase_confidence={confidence}; {notes}",
            })
    ready_rows=[r for r in all_rows if r['selected_for_w24']=='1']
    unavailable_rows=[r for r in all_rows if r['selected_for_w24']!='1']
    # physical groups after selected rows are known
    reverse=defaultdict(list)
    for rr in runner_rows:
        key=(rr['wrapper_path'], rr['run_command'])
        reverse[physical_key_to_group[key]].append(rr)
    for gid, rs in sorted(reverse.items()):
        group_rows.append({'duplicate_physical_group':gid,'paper_ids':';'.join(r['paper_id'] for r in rs),'wrapper_path':rs[0]['wrapper_path'],'run_command':rs[0]['run_command'],'row_count':str(len(rs)),'notes':'same wrapper/command; W24 runs separately for clarity' if len(rs)>1 else 'unique physical command'})
    run_plan=[]
    configs=[r for r in CONFIG_ROWS if r['enabled']=='1']
    for rr in ready_rows:
        cmd=command.get(rr['paper_id'],{})
        key=(rr['wrapper_path'], cmd.get('run_command',''))
        gid=physical_key_to_group.get(key,'')
        for cfg in configs:
            run_plan.append({'run_id':f"{cfg['config_id']}__{rr['paper_id']}",'config_id':cfg['config_id'],'config_path':cfg['config_path'],'config_role':cfg['config_role'],'paper_id':rr['paper_id'],'paper_name':rr['paper_name'],'paper_type':rr['paper_type'],'wrapper_path':rr['wrapper_path'],'measurement_scope':rr['measurement_scope'],'phase_mapping_confidence_w18':rr['phase_mapping_confidence_w18'],'selected_for_run':'1','timeout_sec':'1800','expected_activation':expected_activation(cfg['config_id'], rr['paper_type']),'duplicate_physical_group':gid,'notes':'app-level sweep row; inferred_order is not exact phase mapping' if rr['measurement_scope']=='app_run_inferred_phase' else 'app-level sweep row'})
    write_csv(W24 / 'w24_tableiii_manifest_all.csv', all_rows, ALL_FIELDS)
    write_csv(W24 / 'w24_tableiii_manifest_ready.csv', ready_rows, ALL_FIELDS)
    write_csv(W24 / 'w24_tableiii_manifest_unavailable.csv', unavailable_rows, ALL_FIELDS)
    write_csv(W24 / 'w24_tableiii_phase_mapping_used.csv', phase_rows, PHASE_FIELDS)
    write_csv(W24 / 'w24_tableiii_run_plan.csv', run_plan, RUN_PLAN_FIELDS)
    write_csv(W24 / 'w24_tableiii_runner_workload_manifest.csv', runner_rows, RUNNER_FIELDS)
    write_csv(W24 / 'w24_physical_run_groups.csv', group_rows, GROUP_FIELDS)
    report = f"""# Mascar W24A Table III Run Plan\n\n## Goal\nRun a refreshed current-simulator Table III sweep over currently ready rows only.\n\n## Inputs used\nCanonical Table III workload manifest, W17 ready/unavailable manifests, W18 proposed phase mapping, and current command manifest.\n\n## W18 phase mapping status\nW18 inferred launch-order mapping is used where present. `inferred_order` remains inferred and is not exact.\n\n## Config matrix\nEnabled configs: baseline_off, m2_owner_sched, m3_hitonly_nack, m4_reexec_load.\n\n## Selected ready rows\nSelected ready rows: {len(ready_rows)}. Paper IDs: {', '.join(r['paper_id'] for r in ready_rows)}.\n\n## Unavailable rows\nUnavailable rows: {len(unavailable_rows)}. They remain in coverage manifest and are not actual-run.\n\n## Measurement scope caveat\nRows with inferred launch-order mapping are measured at app-run level and marked `app_run_inferred_phase`; this is not strict per-kernel paper phase timing.\n\n## Run row counts\nActual planned rows: {len(run_plan)} = {len(ready_rows)} ready rows x {len(configs)} enabled configs.\n\n## Physical duplicate groups\nPhysical groups: {len(group_rows)}. Duplicate commands are run separately for clarity.\n\n## W24B execution instructions\nRun `bash experiments/paper-mascar/workloads/matrix/W24/run_w24_tableiii_refreshed_sweep.sh` after dry-run validation.\n"""
    (DOCS / 'mascar_w24a_tableiii_run_plan.md').write_text(report)
    print(f'all_rows={len(all_rows)} ready_rows={len(ready_rows)} unavailable_rows={len(unavailable_rows)} run_plan_rows={len(run_plan)} physical_groups={len(group_rows)}')
    return 0
if __name__ == '__main__':
    raise SystemExit(main())
