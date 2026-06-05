#!/usr/bin/env python3
"""Generate W19 unified Rodinia/Parboil suite command manifest."""
from __future__ import annotations
import csv
from pathlib import Path

OUT=Path('experiments/suites/common')
WORKLOAD_MANIFEST=Path('/workspace/repos/gpgpu-workloads/manifests/workload_manifest.csv')
ENV='/workspace/repos/load_gpgpusim.sh'
FIELDS=['suite_workload_id','suite','benchmark','availability_status','wrapper_status','source_dir','binary_path','run_working_dir','run_command','timeout_sec','input_size','expected_verification','blocker','next_action','notes']
RECOVERED={
 'rodinia_gaussian': {'cmd':'./gaussian -s 16','timeout':'900','verify':'completed_no_explicit_pass','notes':'W19B raw_make pass; size-based input avoids missing matrix data'},
 'rodinia_myocyte': {'cmd':'./myocyte.out 100 1 0','timeout':'1200','verify':'completed_no_explicit_pass','notes':'W19B raw_make pass; command from local run script without external data'},
 'rodinia_nn': {'cmd':'./nn filelist_4 -r 5 -lat 30 -lng 90','timeout':'900','verify':'completed_no_explicit_pass','notes':'W19B raw_make pass; filelist_4 exists locally'},
 'rodinia_streamcluster': {'cmd':'./sc_gpu 10 20 32 256 256 10 none output.txt 1','timeout':'1200','verify':'completed_no_explicit_pass','notes':'W19B raw_make pass; normalized smaller smoke input than raw run script'},
}

def read_csv(p):
    with p.open(newline='') as f: return list(csv.DictReader(f))
manifest_by_app={}
for r in read_csv(WORKLOAD_MANIFEST):
    if r['suite'] in {'rodinia','parboil'}:
        manifest_by_app[r['app']]=r
rows=[]
for f in [OUT/'rodinia_full_manifest.csv', OUT/'parboil_full_manifest.csv']:
    for r in read_csv(f):
        row={k:'' for k in FIELDS}
        row.update({'suite_workload_id':r['suite_workload_id'],'suite':r['suite'],'benchmark':r['benchmark'],'source_dir':r['source_dir'],'binary_path':r['binary_path']})
        app=r['existing_manifest_app']
        if r['availability_status']=='ready' and app in manifest_by_app:
            m=manifest_by_app[app]
            row.update({
                'availability_status':'ready_existing_manifest',
                'wrapper_status':'ready',
                'run_working_dir':m['config_local'] or r['wrapper_dir'] or r['source_dir'],
                'run_command':m['run_command'],
                'timeout_sec':'1800',
                'input_size':m['expected_runtime'],
                'expected_verification':m['verification'],
                'next_action':'dry-run and smoke validation',
                'notes':'Existing workload_manifest ready row; W18 background preserved but not modified',
            })
        elif r['suite_workload_id'] in RECOVERED and r['binary_executable']=='1':
            meta=RECOVERED[r['suite_workload_id']]
            row.update({
                'availability_status':'ready_build_recovered_candidate',
                'wrapper_status':'ready',
                'run_working_dir':r['source_dir'],
                'run_command':f"cd {r['source_dir']} && source {ENV} && {meta['cmd']}",
                'timeout_sec':meta['timeout'],
                'input_size':'tiny',
                'expected_verification':meta['verify'],
                'next_action':'smoke validation before promotion to canonical workload manifest',
                'notes':meta['notes'],
            })
        else:
            row.update({
                'availability_status':r['availability_status'],
                'wrapper_status':'placeholder_unavailable',
                'timeout_sec':'1800',
                'blocker':r['blocker'],
                'next_action':r['next_action'],
                'notes':'Placeholder retained; not eligible for W19D smoke until binary/command/data are resolved',
            })
        rows.append(row)
with (OUT/'suite_command_manifest.csv').open('w', newline='') as f:
    w=csv.DictWriter(f, fieldnames=FIELDS, lineterminator='\n')
    w.writeheader(); w.writerows(rows)
print('rows',len(rows),'ready',sum(r['wrapper_status']=='ready' for r in rows),'build_recovered',sum(r['availability_status']=='ready_build_recovered_candidate' for r in rows))
