#!/usr/bin/env python3
from __future__ import annotations
import csv, math
from collections import defaultdict, Counter
from pathlib import Path
ROOT=Path('/workspace/repos/gpgpu-sim_distribution')
W25=ROOT/'experiments/paper-mascar/energy/W25'; MAT=W25/'matrix'; RES=W25/'results'; DOC=ROOT/'docs/papers'
CONFIGS=('energy_baseline_off','energy_m4_reexec_load')
def read(p):
    if not Path(p).exists(): return []
    with open(p,newline='') as f: return list(csv.DictReader(f))
def write(p, rows, fields):
    Path(p).parent.mkdir(parents=True, exist_ok=True)
    with open(p,'w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n',extrasaction='ignore'); w.writeheader(); w.writerows(rows)
def num(v):
    try: return float(v) if v not in (None,'') else 0.0
    except Exception: return 0.0
def ratio(a,b): return (a/b) if a>0 and b>0 else ''
def gm(vals):
    vals=[v for v in vals if v>0]
    return math.exp(sum(math.log(v) for v in vals)/len(vals)) if vals else ''
def main():
    results=read(RES/'w25_energy_actual_results.csv')
    workloads=read(MAT/'w25_energy_workload_manifest.csv')
    w16_map=read(ROOT/'experiments/paper-mascar/energy/W16A/e1_energy_stat_map.csv')
    by={(r['paper_id'],r['config_id']):r for r in results}
    rows=[]
    fields=['workload_id','paper_id','paper_name','paper_type','baseline_status','m4_status','baseline_cycles','m4_cycles','cycle_ratio','baseline_ipc','m4_ipc','ipc_ratio','baseline_kernel_avg_power','m4_kernel_avg_power','baseline_gpu_tot_avg_power','m4_gpu_tot_avg_power','power_ratio_kernel','power_ratio_gpu_total','baseline_derived_energy','m4_derived_energy','derived_energy_ratio','derived_energy_saving','m2_active_prior','m3_active_prior','m4_active_prior','correctness_status','notes']
    for w in workloads:
        pid=w['paper_id']; b=by.get((pid,'energy_baseline_off'),{}); m=by.get((pid,'energy_m4_reexec_load'),{})
        bc=num(b.get('gpu_tot_sim_cycle')); mc=num(m.get('gpu_tot_sim_cycle'))
        bi=num(b.get('gpu_tot_ipc')); mi=num(m.get('gpu_tot_ipc'))
        bkp=num(b.get('kernel_avg_power')); mkp=num(m.get('kernel_avg_power'))
        bgp=num(b.get('gpu_tot_avg_power') or b.get('power_total_avg')); mgp=num(m.get('gpu_tot_avg_power') or m.get('power_total_avg'))
        # W25 current results contain no power fields; do not derive energy without power.
        bde=''; mde=''; er=''; es=''
        correctness='explicit_pass' if b.get('classification')=='completed_explicit_pass' and m.get('classification')=='completed_explicit_pass' else 'stats_only_not_correctness_pass'
        notes='energy_unavailable_no_power_fields' if not (bkp or mkp or bgp or mgp or b.get('energy_total') or m.get('energy_total')) else 'current_simulator_power_fields_found'
        rows.append({'workload_id':w['workload_id'],'paper_id':pid,'paper_name':w['paper_name'],'paper_type':w['paper_type'],'baseline_status':b.get('classification','missing'),'m4_status':m.get('classification','missing'),'baseline_cycles':b.get('gpu_tot_sim_cycle',''),'m4_cycles':m.get('gpu_tot_sim_cycle',''),'cycle_ratio':f'{ratio(mc,bc):.6f}' if ratio(mc,bc)!='' else '','baseline_ipc':b.get('gpu_tot_ipc',''),'m4_ipc':m.get('gpu_tot_ipc',''),'ipc_ratio':f'{ratio(mi,bi):.6f}' if ratio(mi,bi)!='' else '','baseline_kernel_avg_power':b.get('kernel_avg_power',''),'m4_kernel_avg_power':m.get('kernel_avg_power',''),'baseline_gpu_tot_avg_power':b.get('gpu_tot_avg_power') or b.get('power_total_avg',''),'m4_gpu_tot_avg_power':m.get('gpu_tot_avg_power') or m.get('power_total_avg',''),'power_ratio_kernel':f'{ratio(mkp,bkp):.6f}' if ratio(mkp,bkp)!='' else '','power_ratio_gpu_total':f'{ratio(mgp,bgp):.6f}' if ratio(mgp,bgp)!='' else '','baseline_derived_energy':bde,'m4_derived_energy':mde,'derived_energy_ratio':er,'derived_energy_saving':es,'m2_active_prior':w['prior_m2_active'],'m3_active_prior':w['prior_m3_active'],'m4_active_prior':w['prior_m4_active'],'correctness_status':correctness,'notes':notes})
    write(RES/'w25_energy_trend_results.csv', rows, fields)
    valid_power=[r for r in rows if r['power_ratio_gpu_total']]
    valid_energy=[r for r in rows if r['derived_energy_ratio']]
    groups=[]
    def add(group, subset):
        prs=[float(r['power_ratio_gpu_total']) for r in subset if r['power_ratio_gpu_total']]
        ers=[float(r['derived_energy_ratio']) for r in subset if r['derived_energy_ratio']]
        crs=[float(r['cycle_ratio']) for r in subset if r['cycle_ratio']]
        groups.append({'group':group,'workloads':str(len(subset)),'valid_power_rows':str(len(prs)),'avg_power_ratio_gpu_total':f'{sum(prs)/len(prs):.6f}' if prs else '','valid_energy_rows':str(len(ers)),'geomean_energy_ratio':f'{gm(ers):.6f}' if ers else '','avg_cycle_ratio':f'{sum(crs)/len(crs):.6f}' if crs else '','status':'energy_unavailable' if not ers else 'ok'})
    add('all', rows); add('memory',[r for r in rows if r['paper_type']=='M']); add('compute',[r for r in rows if r['paper_type']=='C']); add('m4_active_prior',[r for r in rows if r['m4_active_prior']=='1']); add('m4_inactive_prior',[r for r in rows if r['m4_active_prior']!='1'])
    write(RES/'w25_energy_ratio_summary.csv', groups, ['group','workloads','valid_power_rows','avg_power_ratio_gpu_total','valid_energy_rows','geomean_energy_ratio','avg_cycle_ratio','status'])
    avail=[]
    for r in rows:
        avail.append({'workload_id':r['workload_id'],'baseline_status':r['baseline_status'],'m4_status':r['m4_status'],'has_power_fields':'1' if (r['baseline_gpu_tot_avg_power'] or r['baseline_kernel_avg_power'] or r['m4_gpu_tot_avg_power'] or r['m4_kernel_avg_power']) else '0','has_direct_energy':'0','has_derived_energy':'1' if r['derived_energy_ratio'] else '0','availability_status':'energy_unavailable_no_power_fields' if not r['derived_energy_ratio'] else 'energy_available','next_action':'inspect energy config power output or restore AccelWattch power fields' if not r['derived_energy_ratio'] else 'use_trend'})
    write(RES/'w25_energy_availability_matrix.csv', avail, ['workload_id','baseline_status','m4_status','has_power_fields','has_direct_energy','has_derived_energy','availability_status','next_action'])
    with (RES/'w25_energy_trend_summary.md').open('w') as f:
        f.write('# W25 Energy Trend Summary\n\n')
        f.write(f'workloads: {len(rows)}\nactual_rows: {len(results)}\n')
        f.write(f'power_field_rows: {sum(a["has_power_fields"]=="1" for a in avail)}\nderived_energy_rows: {sum(a["has_derived_energy"]=="1" for a in avail)}\n')
        f.write('\nEnergy is unavailable in W25 actual results because no power/energy fields were parsed. Cycle ratios are retained as performance context only.\n')
    report=f'''# Mascar W25 Integrated Energy Sweep Report\n\n## Executive summary\nW25 ran a bounded current-simulator energy sweep on {len(rows)} workloads and 2 energy configs. All actual runs completed with stats, but no power/energy fields were parsed, so energy trend is unavailable for W25.\n\n## Scope and caveat\nThis is current-simulator energy infrastructure validation, not paper GPUWattch GTX480 energy reproduction and not a claim of 12% paper energy saving.\n\n## Workloads and configs\nWorkloads: {', '.join(r['workload_id'] for r in rows)}. Configs: energy_baseline_off and energy_m4_reexec_load.\n\n## Energy/power fields found\nPower/energy field rows: {sum(a['has_power_fields']=='1' for a in avail)} / {len(rows)} workloads.\n\n## Derived energy method\nW16 uses avg power x runtime from cycles when power fields exist. W25 did not derive energy because current results had no reliable power fields.\n\n## Baseline vs M4 table\nSee `w25_energy_trend_results.csv`.\n\n## Group trend summary\nSee `w25_energy_ratio_summary.csv`; energy ratio groups are marked unavailable.\n\n## Relation to W24 performance/activation\nSelection prioritized W24 M2/M4-active rows and W16 stable energy workloads. W25 preserves W24 activation history in the trend CSV.\n\n## Differences from paper GPUWattch energy evaluation\nCurrent configs and simulator environment differ from GPGPU-Sim v3.2.2 GTX480/Fermi GPUWattch.\n\n## Limitations\nNo current W25 power fields were emitted or parsed, so only cycle/ipc context is available. completed_stats_found/completed_no_explicit_pass are not correctness pass.\n\n## Recommendations for future energy runs\nRestore/verify AccelWattch power field emission for energy configs, then rerun the same bounded workload set.\n'''
    (DOC/'mascar_w25_integrated_energy_sweep_report.md').write_text(report)
    print(f'w25_analysis workloads={len(rows)} power_rows={sum(a["has_power_fields"]=="1" for a in avail)} energy_rows={sum(a["has_derived_energy"]=="1" for a in avail)}')
if __name__=='__main__': main()
