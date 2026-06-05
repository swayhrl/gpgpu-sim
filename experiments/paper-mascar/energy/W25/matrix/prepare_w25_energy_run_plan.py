#!/usr/bin/env python3
from __future__ import annotations
import csv
from pathlib import Path

ROOT=Path('/workspace/repos/gpgpu-sim_distribution')
W25=ROOT/'experiments/paper-mascar/energy/W25'
MAT=W25/'matrix'; RES=W25/'results'; AUD=W25/'audit'; DOC=ROOT/'docs/papers'
for p in (MAT,RES,AUD,DOC): p.mkdir(parents=True, exist_ok=True)
INPUTS={
 'w24_refreshed':'experiments/paper-mascar/workloads/results/W24/w24_tableiii_refreshed_results.csv',
 'w24_coverage':'experiments/paper-mascar/workloads/results/W24/w24_tableiii_coverage_manifest.csv',
 'w24_activation':'experiments/paper-mascar/workloads/results/W24/w24_tableiii_activation_matrix.csv',
 'w24_speedup':'experiments/paper-mascar/workloads/results/W24/w24_tableiii_speedup_summary.csv',
 'w24_report':'docs/papers/mascar_w24_tableiii_refreshed_sweep_report.md',
 'w16_trend':'experiments/paper-mascar/energy/W16D/w16_energy_trend_summary.csv',
 'w16_ratio':'experiments/paper-mascar/energy/W16D/w16_energy_ratio_by_workload.csv',
 'w16_stat_map':'experiments/paper-mascar/energy/W16A/e1_energy_stat_map.csv',
 'command_manifest':'experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv',
}
CONFIGS=[
 {'config_id':'energy_baseline_off','config_path':'configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off','config_role':'energy_baseline','enabled':'1','notes':'current-simulator AccelWattch baseline'},
 {'config_id':'energy_m4_reexec_load','config_path':'configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on','config_role':'energy_m4','enabled':'1','notes':'current-simulator AccelWattch M4 load reexec'},
 {'config_id':'baseline_off','config_path':'configs/hrl-repro/SM7_QV100_mascar_baseline_off','config_role':'non_energy_reference','enabled':'0','notes':'disabled reference'},
 {'config_id':'m4_reexec_load','config_path':'configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on','config_role':'non_energy_reference','enabled':'0','notes':'disabled reference'},
]
WORKLOAD_FIELDS=['workload_id','paper_id','paper_name','paper_type','suite_id','wrapper_path','wrapper_status','selected_for_w25','selection_reason','prior_correctness_status','prior_m2_active','prior_m3_active','prior_m4_active','timeout_sec','notes']
RUNNER_FIELDS=['paper_id','paper_name','paper_type','availability','wrapper_path','wrapper_status','build_required','build_command','run_working_dir','run_command','input_size','timeout_sec','dry_run_status','notes']
RUN_PLAN_FIELDS=['run_id','config_id','config_path','config_role','workload_id','paper_id','wrapper_path','selected_for_run','timeout_sec','expected_energy_fields','notes']

def read_csv(rel):
    p=ROOT/rel
    if not p.exists(): return []
    with p.open(newline='') as f: return list(csv.DictReader(f))
def write_csv(path, rows, fields):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n',extrasaction='ignore'); w.writeheader(); w.writerows(rows)

def exists(rel): return (ROOT/rel).exists()
def main():
    input_rows=[]
    for name,rel in INPUTS.items():
        status='found' if exists(rel) else 'missing'
        rows=''
        if status=='found' and rel.endswith('.csv'):
            rows=str(len(read_csv(rel)))
        input_rows.append({'input_name':name,'path':rel,'status':status,'rows':rows,'notes':'priority_input' if name.startswith('w24') else ''})
    write_csv(AUD/'w25a_input_status.csv', input_rows, ['input_name','path','status','rows','notes'])
    fallback=not all(exists(INPUTS[k]) for k in ['w24_refreshed','w24_coverage','w24_activation','w24_speedup'])
    write_csv(MAT/'w25_energy_config_matrix.csv', CONFIGS, ['config_id','config_path','config_role','enabled','notes'])
    coverage={r['paper_id']:r for r in read_csv(INPUTS['w24_coverage'])}
    active={r['paper_id']:r for r in read_csv(INPUTS['w24_activation'])}
    cmd={r['paper_id']:r for r in read_csv(INPUTS['command_manifest'])}
    selected_ids=['bp_2','srad_1','bp_1','spmv','mri_q','pathfinder'] if not fallback else ['spmv','mri_q','pathfinder']
    reasons={
     'bp_2':'W24 M2/M4 active memory row','srad_1':'W24 M2/M4 active memory row; srad_2 duplicate omitted','bp_1':'W24 M2/M4 active compute row',
     'spmv':'W16 energy-stable and W24 ready memory row','mri_q':'W16 energy-stable and W24 ready compute row','pathfinder':'W16 energy-stable and W24 ready compute row'}
    workloads=[]; runner=[]
    for pid in selected_ids:
        c=coverage.get(pid,{})
        cm=cmd.get(pid,{})
        a=active.get(pid,{})
        if cm.get('wrapper_status')!='ready' and c.get('wrapper_status')!='ready':
            continue
        row={'workload_id':pid,'paper_id':pid,'paper_name':c.get('paper_name') or cm.get('paper_name') or pid,'paper_type':c.get('paper_type') or cm.get('paper_type',''),'suite_id':cm.get('run_command','').split()[1].split('_')[0] if cm.get('run_command','').startswith('bash scripts/run_one.sh') else 'tableiii','wrapper_path':cm.get('wrapper_path',''),'wrapper_status':'ready','selected_for_w25':'1','selection_reason':reasons.get(pid,'selected stable workload'),'prior_correctness_status':c.get('correctness_status','unknown'),'prior_m2_active':a.get('m2_active_any','0'),'prior_m3_active':a.get('m3_active_any','0'),'prior_m4_active':a.get('m4_active_any','0'),'timeout_sec':'1800','notes':'current-simulator energy trend; not paper GTX480 GPUWattch'}
        workloads.append(row)
        runner.append({'paper_id':pid,'paper_name':row['paper_name'],'paper_type':row['paper_type'],'availability':'available','wrapper_path':row['wrapper_path'],'wrapper_status':'ready','build_required':cm.get('build_required','no'),'build_command':cm.get('build_command',''),'run_working_dir':cm.get('run_working_dir',''),'run_command':cm.get('run_command',''),'input_size':cm.get('input_size','tiny'),'timeout_sec':'1800','dry_run_status':cm.get('dry_run_status','unknown'),'notes':row['notes']+'; '+row['selection_reason']})
    write_csv(MAT/'w25_energy_workload_manifest.csv', workloads, WORKLOAD_FIELDS)
    write_csv(MAT/'w25_energy_command_manifest.csv', runner, RUNNER_FIELDS)
    enabled=[c for c in CONFIGS if c['enabled']=='1' and (ROOT/c['config_path']).exists()]
    run_plan=[]
    for w in workloads:
        for cfg in enabled:
            run_plan.append({'run_id':f"{cfg['config_id']}__{w['workload_id']}",'config_id':cfg['config_id'],'config_path':cfg['config_path'],'config_role':cfg['config_role'],'workload_id':w['workload_id'],'paper_id':w['paper_id'],'wrapper_path':w['wrapper_path'],'selected_for_run':'1','timeout_sec':w['timeout_sec'],'expected_energy_fields':'power_total_avg|gpu_tot_avg_power|kernel_avg_power','notes':w['selection_reason']})
    write_csv(MAT/'w25_energy_run_plan.csv', run_plan, RUN_PLAN_FIELDS)
    summary=[]
    for w in workloads:
        summary.append({'workload_id':w['workload_id'],'paper_id':w['paper_id'],'paper_type':w['paper_type'],'selection_reason':w['selection_reason'],'prior_m2_active':w['prior_m2_active'],'prior_m3_active':w['prior_m3_active'],'prior_m4_active':w['prior_m4_active'],'fallback_mode':'1' if fallback else '0'})
    write_csv(MAT/'w25_energy_selection_summary.csv', summary, ['workload_id','paper_id','paper_type','selection_reason','prior_m2_active','prior_m3_active','prior_m4_active','fallback_mode'])
    report=f'''# Mascar W25A Integrated Energy Run Plan\n\n## Goal\nRun a bounded current-simulator energy trend sweep comparing energy baseline and M4 re-execution configs.\n\n## Paper energy caveat\nThis is not GPUWattch GTX480/Fermi paper-equivalent energy reproduction and does not claim the paper 12% energy saving.\n\n## W16 energy pipeline status\nW16 found current-simulator power fields and derived energy as average power times runtime from cycles.\n\n## W24 inputs used\nFallback mode: {1 if fallback else 0}. W24 refreshed results, coverage, activation, and speedup files were {'not all available' if fallback else 'available and used'}.\n\n## Selected workloads\n{', '.join(w['workload_id'] for w in workloads)}\n\n## Config matrix\nEnabled configs: {', '.join(c['config_id'] for c in enabled)}.\n\n## Run row count\n{len(run_plan)} rows = {len(workloads)} workloads x {len(enabled)} configs.\n\n## Risks and assumptions\nEnergy/power field availability depends on current AccelWattch config output. completed_stats_found is not correctness pass.\n\n## W25B execution plan\nRun dry-run, actual sweep with timeout, collect with common collector, then analyze current-simulator trend.\n'''
    (DOC/'mascar_w25a_energy_integrated_run_plan.md').write_text(report)
    print(f'fallback_mode={1 if fallback else 0} workloads={len(workloads)} enabled_configs={len(enabled)} run_plan_rows={len(run_plan)}')
if __name__=='__main__': main()
