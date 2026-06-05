#!/usr/bin/env python3
import csv
from pathlib import Path

ROOT=Path('/workspace/repos/gpgpu-sim_distribution')
COMMON=ROOT/'experiments/suites/common'
W22=ROOT/'experiments/suites/matrix/W22'
W23=ROOT/'experiments/suites/matrix/W23'
for p in (W22,W23): p.mkdir(parents=True, exist_ok=True)

def read(path):
    with open(path,newline='') as f: return list(csv.DictReader(f))
def write(path, rows, fields):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path,'w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=fields,lineterminator='\n'); w.writeheader(); w.writerows(rows)

base=read(COMMON/'full_suite_command_manifest.csv')
by_id={r['app_id']:dict(r) for r in base}
probe=[]
# Candidate 1: W21 command candidate; requires GPGPU-Sim stats smoke before final ready.
r=by_id['rodinia_huffman']
r.update({
 'current_ready':'1','wrapper_status':'w22_smoke_candidate','availability_status':'w22_smoke_candidate',
 'run_command':'cd /workspace/repos/gpgpu-workloads/suites/rodinia/cuda/huffman && source /workspace/repos/load_gpgpusim.sh && ./pavle',
 'binary_path':'/workspace/repos/gpgpu-workloads/suites/rodinia/cuda/huffman/pavle','data_path':'generated_internal_or_none',
 'input_scale':'tiny','default_timeout_sec':'1800','blocker':'needs_w22_collector_smoke',
 'next_action':'run W22 candidate smoke and promote only if stats found','notes':'W21 candidate; no external dataset required by command probe'
})
probe.append(dict(r))
# Candidate 2: W21 recovered Parboil simple_mm binary; source has internal random inputs and no external dataset.
r=by_id['parboil_simple_mm']
r.update({
 'current_ready':'1','wrapper_status':'w22_smoke_candidate','availability_status':'w22_smoke_candidate',
 'run_command':'cd /workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/simple_mm/build/cuda_base_w21fix2 && source /workspace/repos/load_gpgpusim.sh && ./simple_mm',
 'binary_path':'/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/simple_mm/build/cuda_base_w21fix2/simple_mm','data_path':'generated_internal',
 'input_scale':'tiny','default_timeout_sec':'1800','blocker':'needs_w22_collector_smoke',
 'next_action':'run W22 candidate smoke and promote only if stats found','notes':'W21 binary recovered; source allocates random matrices internally'
})
probe.append(dict(r))
fields=list(base[0].keys())
write(W22/'w22_candidate_command_manifest.csv', probe, fields)
# Initial optimistic manifest: base ready + candidates for smoke validation.
init=[]
for row in base:
    if row['current_ready']=='1': init.append(dict(row))
for row in probe: init.append(dict(row))
write(W22/'w22_smoke_candidate_command_manifest.csv', init, fields)
# Blocker followup for W21 recovered but not candidate.
rec=read(COMMON/'w21_new_ready_candidates.csv')
block=[]
for row in rec:
    if row['app_id'] in {'rodinia_huffman','parboil_simple_mm'}: continue
    block.append({
      'app_id':row['app_id'],'suite_id':row['suite_id'],'benchmark_name':row['benchmark_name'],
      'w21_status':row['w21_status'],'binary_paths':row['new_binary_paths'],
      'w22_status':'not_smoke_ready','blocker':'missing external data or validated args',
      'next_action':'create/import tiny dataset and command, then rerun candidate smoke'
    })
write(W22/'w22_binary_data_wrapper_manifest.csv', block, ['app_id','suite_id','benchmark_name','w21_status','binary_paths','w22_status','blocker','next_action'])
print('candidate_rows',len(probe),'initial_smoke_ready_rows',len(init),'blocker_rows',len(block))
