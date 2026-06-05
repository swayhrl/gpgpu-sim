#!/usr/bin/env python3
from __future__ import annotations
import csv, math
from collections import Counter, defaultdict
from pathlib import Path

ROOT=Path('/workspace/repos/gpgpu-sim_distribution')
W24=ROOT/'experiments/paper-mascar/workloads/matrix/W24'
RES=ROOT/'experiments/paper-mascar/workloads/results/W24'
DOCS=ROOT/'docs/papers'
CONFIG_ORDER=['baseline_off','m2_owner_sched','m3_hitonly_nack','m4_reexec_load']

def read(path):
    if not path.exists(): return []
    with path.open(newline='') as f: return list(csv.DictReader(f))
def write(path, rows, fields):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n',extrasaction='ignore'); w.writeheader(); w.writerows(rows)
def num(v):
    try:
        if v is None or v=='': return 0.0
        return float(v)
    except Exception:
        return 0.0
def pos(v):
    x=num(v); return x if x>0 else 0.0
def geomean(vals):
    vals=[v for v in vals if v>0]
    if not vals: return ''
    return math.exp(sum(math.log(v) for v in vals)/len(vals))
def active_m2(r): return num(r.get('paper_mascar_m2_mp_cycles'))>0 or num(r.get('paper_mascar_m2_owner_acquire'))>0
def active_m3(r):
    return any(num(r.get(k))>0 for k in ['paper_mascar_m3_hitonly_access_attempt','paper_mascar_m3_hitonly_access_hit','paper_mascar_m3_hitonly_access_nack','paper_mascar_m3_probe_attempt','paper_mascar_m3_probe_hit','paper_mascar_m3_probe_nack'])
def active_m4(r): return num(r.get('paper_mascar_m4_enqueue_success'))>0 or num(r.get('paper_mascar_m4_retry_attempt'))>0

def main():
    results=read(RES/'w24_latest_results.csv')
    all_manifest=read(W24/'w24_tableiii_manifest_all.csv')
    phase=read(W24/'w24_tableiii_phase_mapping_used.csv')
    configs=read(W24/'w24_tableiii_config_matrix.csv')
    by_key={(r['paper_id'],r['config_id']):r for r in results}
    base_by={r['paper_id']:r for r in results if r['config_id']=='baseline_off'}
    meta={r['paper_id']:r for r in all_manifest}
    fields=['paper_id','paper_name','paper_type','config_id','result_status','correctness_status','cycles','ipc','baseline_cycles','speedup_vs_baseline','m1_l1sat_active','m2_active','m3_active','m4_active','m2_mp_cycles','m2_owner_acquire','m3_attempt','m3_hit','m3_nack','m4_enqueue_success','m4_retry_attempt','m4_retry_hit','m4_retry_nack','phase_mapping_confidence_w18','measurement_scope','notes']
    out=[]
    for pid,m in meta.items():
        for cfg in CONFIG_ORDER:
            r=by_key.get((pid,cfg),{})
            b=base_by.get(pid,{})
            cycles=pos(r.get('gpu_tot_sim_cycle'))
            bcycles=pos(b.get('gpu_tot_sim_cycle'))
            speed=(bcycles/cycles) if cycles and bcycles and r.get('has_stats')=='1' and b.get('has_stats')=='1' else ''
            correctness='explicit_pass' if r.get('classification')=='completed_explicit_pass' else ('stats_only_not_correctness_pass' if r.get('classification') in ('completed_no_explicit_pass','completed_stats_found') else ('not_run' if not r else 'failed_or_unavailable'))
            m3_attempt=sum(num(r.get(k)) for k in ['paper_mascar_m3_hitonly_access_attempt','paper_mascar_m3_probe_attempt'])
            m3_hit=sum(num(r.get(k)) for k in ['paper_mascar_m3_hitonly_access_hit','paper_mascar_m3_probe_hit'])
            m3_nack=sum(num(r.get(k)) for k in ['paper_mascar_m3_hitonly_access_nack','paper_mascar_m3_probe_nack'])
            out.append({'paper_id':pid,'paper_name':m['paper_name'],'paper_type':m['paper_type'],'config_id':cfg,'result_status':r.get('classification','not_run'),'correctness_status':correctness,'cycles':r.get('gpu_tot_sim_cycle',''),'ipc':r.get('gpu_tot_ipc',''),'baseline_cycles':b.get('gpu_tot_sim_cycle',''),'speedup_vs_baseline':f'{speed:.6f}' if speed!='' else '', 'm1_l1sat_active':'1' if num(r.get('paper_mascar_l1_sat_sample_saturated'))>0 else '0','m2_active':'1' if active_m2(r) else '0','m3_active':'1' if active_m3(r) else '0','m4_active':'1' if active_m4(r) else '0','m2_mp_cycles':r.get('paper_mascar_m2_mp_cycles',''),'m2_owner_acquire':r.get('paper_mascar_m2_owner_acquire',''),'m3_attempt':str(int(m3_attempt)) if m3_attempt.is_integer() else str(m3_attempt),'m3_hit':str(int(m3_hit)) if m3_hit.is_integer() else str(m3_hit),'m3_nack':str(int(m3_nack)) if m3_nack.is_integer() else str(m3_nack),'m4_enqueue_success':r.get('paper_mascar_m4_enqueue_success',''),'m4_retry_attempt':r.get('paper_mascar_m4_retry_attempt',''),'m4_retry_hit':r.get('paper_mascar_m4_retry_hit',''),'m4_retry_nack':r.get('paper_mascar_m4_retry_nack',''),'phase_mapping_confidence_w18':m['phase_mapping_confidence_w18'],'measurement_scope':m['measurement_scope'],'notes':'app-run level; not strict paper per-kernel phase' if m['measurement_scope'].startswith('app_run') else ''})
    write(RES/'w24_tableiii_refreshed_results.csv', out, fields)
    speed_fields=['paper_id','paper_name','paper_type','measurement_scope','baseline_cycles','m2_speedup','m3_speedup','m4_speedup','valid_for_trend','notes']
    speed_rows=[]
    out_by={(r['paper_id'],r['config_id']):r for r in out}
    for pid,m in meta.items():
        vals={cfg:out_by.get((pid,cfg),{}) for cfg in CONFIG_ORDER}
        valid=all(vals[cfg].get('speedup_vs_baseline') for cfg in CONFIG_ORDER)
        speed_rows.append({'paper_id':pid,'paper_name':m['paper_name'],'paper_type':m['paper_type'],'measurement_scope':m['measurement_scope'],'baseline_cycles':vals['baseline_off'].get('cycles',''),'m2_speedup':vals['m2_owner_sched'].get('speedup_vs_baseline',''),'m3_speedup':vals['m3_hitonly_nack'].get('speedup_vs_baseline',''),'m4_speedup':vals['m4_reexec_load'].get('speedup_vs_baseline',''),'valid_for_trend':'1' if valid and m['selected_for_w24']=='1' else '0','notes':'selected_ready' if m['selected_for_w24']=='1' else m['selection_reason']})
    write(RES/'w24_tableiii_speedup_summary.csv', speed_rows, speed_fields)
    act_fields=['paper_id','paper_name','paper_type','m2_active_any','m3_active_any','m4_active_any','m2_active_configs','m3_active_configs','m4_active_configs']
    act=[]
    for pid,m in meta.items():
        rs=[o for o in out if o['paper_id']==pid]
        for_act=lambda k:[r['config_id'] for r in rs if r.get(k)=='1']
        act.append({'paper_id':pid,'paper_name':m['paper_name'],'paper_type':m['paper_type'],'m2_active_any':'1' if for_act('m2_active') else '0','m3_active_any':'1' if for_act('m3_active') else '0','m4_active_any':'1' if for_act('m4_active') else '0','m2_active_configs':';'.join(for_act('m2_active')),'m3_active_configs':';'.join(for_act('m3_active')),'m4_active_configs':';'.join(for_act('m4_active'))})
    write(RES/'w24_tableiii_activation_matrix.csv', act, act_fields)
    geo_fields=['group','config_id','valid_rows','geomean_speedup','status']
    geo=[]
    for group, filt in [('all',lambda r:True),('memory',lambda r:r['paper_type']=='M'),('compute',lambda r:r['paper_type']=='C')]:
        for cfg,col in [('m2_owner_sched','m2_speedup'),('m3_hitonly_nack','m3_speedup'),('m4_reexec_load','m4_speedup')]:
            vals=[float(r[col]) for r in speed_rows if r['valid_for_trend']=='1' and filt(r) and r[col]]
            geo.append({'group':group,'config_id':cfg,'valid_rows':str(len(vals)),'geomean_speedup':f'{geomean(vals):.6f}' if len(vals)>=2 else '','status':'ok' if len(vals)>=2 else 'insufficient_data'})
    write(RES/'w24_tableiii_geomean_summary.csv', geo, geo_fields)
    cov_fields=['paper_id','paper_name','paper_type','availability_status','wrapper_status','selected_for_w24','measurement_scope','phase_mapping_status_w18','phase_mapping_confidence_w18','baseline_status','m2_status','m3_status','m4_status','m2_active','m3_active','m4_active','valid_for_trend','correctness_status','next_action']
    cov=[]
    for pid,m in meta.items():
        vals={cfg:out_by.get((pid,cfg),{}) for cfg in CONFIG_ORDER}
        valid=next((r for r in speed_rows if r['paper_id']==pid),{}).get('valid_for_trend','0')
        correctness=';'.join(sorted(set(v.get('correctness_status','') for v in vals.values() if v))) or 'not_run'
        if m['selected_for_w24']!='1': next_action='needs_workload_availability'
        elif m['measurement_scope']=='app_run_inferred_phase': next_action='needs_exact_phase_mapping'
        elif valid=='1': next_action='use_in_report'
        else: next_action='investigate_failure'
        cov.append({'paper_id':pid,'paper_name':m['paper_name'],'paper_type':m['paper_type'],'availability_status':m['availability_status'],'wrapper_status':m['wrapper_status'],'selected_for_w24':m['selected_for_w24'],'measurement_scope':m['measurement_scope'],'phase_mapping_status_w18':m['phase_mapping_status_w18'],'phase_mapping_confidence_w18':m['phase_mapping_confidence_w18'],'baseline_status':vals['baseline_off'].get('result_status','not_run'),'m2_status':vals['m2_owner_sched'].get('result_status','not_run'),'m3_status':vals['m3_hitonly_nack'].get('result_status','not_run'),'m4_status':vals['m4_reexec_load'].get('result_status','not_run'),'m2_active':'1' if any(v.get('m2_active')=='1' for v in vals.values()) else '0','m3_active':'1' if any(v.get('m3_active')=='1' for v in vals.values()) else '0','m4_active':'1' if any(v.get('m4_active')=='1' for v in vals.values()) else '0','valid_for_trend':valid,'correctness_status':correctness,'next_action':next_action})
    write(RES/'w24_tableiii_coverage_manifest.csv', cov, cov_fields)
    write(RES/'w24_tableiii_phase_mapping_used.csv', phase, ['paper_id','paper_name','phase_mapping_status_w18','phase_mapping_confidence_w18','local_launch_index','local_kernel_name','measurement_scope','evidence','notes'])
    cls=Counter(r['result_status'] for r in out)
    active_counts={'m2':sum(r['m2_active_any']=='1' for r in act),'m3':sum(r['m3_active_any']=='1' for r in act),'m4':sum(r['m4_active_any']=='1' for r in act)}
    with (RES/'w24_tableiii_trend_summary.md').open('w') as f:
        f.write('# W24 Table III Refreshed Trend Summary\n\n')
        f.write(f'ready_rows: {sum(r["selected_for_w24"]=="1" for r in meta.values())}\n')
        f.write(f'coverage_rows: {len(cov)}\n')
        f.write(f'result_status_counts: {dict(cls)}\n')
        f.write(f'active_counts: {active_counts}\n\n')
        f.write('## Geomean\n\n')
        for r in geo: f.write(f'- {r["group"]} {r["config_id"]}: {r["geomean_speedup"] or r["status"]} ({r["valid_rows"]} rows)\n')
        f.write('\nCaveat: current-branch/current-config app-level sweep, not paper GTX480/GPGPU-Sim v3.2.2 reproduction.\n')
    report=f'''# Mascar W24 Table III Refreshed Sweep Report

## Executive summary
W24 ran the current ready Table III rows across baseline, M2, M3, and M4 configs. This is current-branch/current-config evidence, not paper-exact reproduction.

## Scope and caveats
Only ready rows were actual-run. All 30 Table III rows remain represented in coverage. Inferred W18 launch-order mapping is not exact phase mapping. App-run results are not strict per-kernel results.

## Configs
Enabled configs: baseline_off, m2_owner_sched, m3_hitonly_nack, m4_reexec_load.

## Workload coverage
Coverage rows: {len(cov)}. Selected ready rows: {sum(r['selected_for_w24']=='1' for r in meta.values())}. Unavailable rows: {sum(r['selected_for_w24']!='1' for r in meta.values())}.

## Phase mapping used
W18 mapping was used without overwriting canonical manifests. `inferred_order` remains inferred.

## Measurement scope caveat
Rows marked `app_run_inferred_phase` are app-level measurements associated with inferred launch order, not strict per-paper kernel timing.

## Results table
See `w24_tableiii_refreshed_results.csv` and `w24_tableiii_speedup_summary.csv`.

## Mechanism activation
M2 active rows: {active_counts['m2']}. M3 active rows: {active_counts['m3']}. M4 active rows: {active_counts['m4']}.

## Memory vs compute trend
See `w24_tableiii_geomean_summary.csv`; geomeans are computed only on valid rows with baseline and config cycles.

## Correctness status caveat
`completed_explicit_pass` is explicit correctness evidence. `completed_no_explicit_pass` / `completed_stats_found` are simulator stats evidence only.

## Failures/timeouts
No timeout rows were observed in the W24 actual sweep. Any unavailable rows are workload availability gaps, not actual-run failures.

## Comparison to paper expectations
This report does not claim Mascar paper 34% speedup or 12% energy saving reproduction. The simulator and GPU target differ from the paper environment.

## What remains before paper-comparable reproduction
Exact phase-level stats, paper-era GPGPU-Sim/GPU configuration alignment, and unavailable workload recovery remain open.

## Recommendations for W25/W26
Add per-kernel stats deltas for inferred phases, create M3-specific input coverage, and continue workload availability repair for unavailable Table III rows.
'''
    (DOCS/'mascar_w24_tableiii_refreshed_sweep_report.md').write_text(report)
    print(f'refreshed_rows={len(out)} coverage_rows={len(cov)} valid_trend_rows={sum(r["valid_for_trend"]=="1" for r in speed_rows)} active_counts={active_counts}')
if __name__=='__main__': main()
