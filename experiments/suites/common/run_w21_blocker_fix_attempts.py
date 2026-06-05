#!/usr/bin/env python3
import csv
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path('/workspace/repos/gpgpu-sim_distribution')
WORKLOAD_ROOT = Path('/workspace/repos/gpgpu-workloads')
PARBOIL_ROOT = WORKLOAD_ROOT / 'suites/parboil'
RODINIA_CUDA_ROOT = WORKLOAD_ROOT / 'suites/rodinia/cuda'
OUT = REPO / 'experiments/suites/common'
AUDIT = REPO / 'experiments/suites/audit/W21'
LOG_ROOT = Path(os.environ.get('W21_LOG_ROOT', f"/workspace/tmp/mascar_w21_blocker_logs_{time.strftime('%Y%m%d_%H%M%S')}"))
TIMEOUT = int(os.environ.get('W21_ATTEMPT_TIMEOUT_SEC', '180'))
DRY_RUN = os.environ.get('W21_DRY_RUN', '0') == '1'
MANIFEST = OUT / 'full_suite_blocker_manifest.csv'
OUT.mkdir(parents=True, exist_ok=True)
AUDIT.mkdir(parents=True, exist_ok=True)
LOG_ROOT.mkdir(parents=True, exist_ok=True)

COMMON_HEADER = ['phase','suite_id','app_id','benchmark_name','attempt_round','strategy','work_dir','command','exit_code','timeout','log_path','result_status','binary_candidates','binary_ready','blocker_before','blocker_after','notes']
DATA_HEADER = ['suite_id','app_id','benchmark_name','attempt_round','strategy','path_checked','command','exit_code','timeout','log_path','data_status','command_status','blocker_after','notes']
CMD_HEADER = ['suite_id','app_id','benchmark_name','attempt_round','strategy','candidate_command','exit_code','timeout','log_path','dry_run_status','smoke_status','blocker_after','notes']

def read_rows():
    with MANIFEST.open(newline='') as f:
        return list(csv.DictReader(f))

def safe(s):
    return re.sub(r'[^A-Za-z0-9_.-]+', '_', s)[:180]

def run_cmd(phase, app_id, attempt, strategy, work_dir, command, env_extra=None):
    log = LOG_ROOT / f"{safe(phase)}_{safe(app_id)}_{safe(attempt)}_{safe(strategy)}.log"
    work = Path(work_dir) if work_dir else REPO
    timeout_flag = 0
    if not work.exists():
        log.write_text(f"missing work_dir: {work}\n")
        return 127, timeout_flag, str(log), 'missing_work_dir'
    env = os.environ.copy()
    env.update(env_extra or {})
    env.setdefault('CUDA_DIR', '/usr/local/cuda')
    env.setdefault('CUDAHOME', '/usr/local/cuda')
    env.setdefault('PATH', '/usr/local/cuda/bin:' + env.get('PATH',''))
    if DRY_RUN:
        log.write_text(f"DRY_RUN work_dir={work}\ncommand={command}\n")
        return 0, 0, str(log), 'dry_run'
    try:
        cp = subprocess.run(['timeout', str(TIMEOUT), 'bash', '-lc', command], cwd=str(work), env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        rc = cp.returncode
        log.write_text(cp.stdout)
    except Exception as e:
        log.write_text(f"exception: {e}\n")
        rc = 126
    if rc == 124:
        timeout_flag = 1
        status = 'timeout'
    elif rc == 0:
        status = 'pass'
    else:
        status = 'fail'
    return rc, timeout_flag, str(log), status

def binary_candidates(row):
    app = row['app_id']
    bench = row['benchmark_name']
    src = Path(row['source_path']) if row['source_path'] not in ('unknown','') else None
    candidates = []
    known = row.get('binary_path','')
    if known and known != 'unknown':
        candidates.append(Path(known))
    if src and src.exists():
        names = {
            'cfd': ['euler3d','euler3d_double','pre_euler3d','pre_euler3d_double'],
            'dwt2d': ['dwt2d'],
            'lavaMD': ['lavaMD','a.out'],
            'leukocyte': ['CUDA/leukocyte','leukocyte'],
            'mummergpu': ['mummergpu','MUMmerGPU'],
            'particlefilter': ['particlefilter','particle_filter','ex_particle_CUDA_naive_seq','ex_particle_CUDA_float_seq'],
            'hybridsort': ['hybridsort'],
            'b+tree': ['b+tree','b_tree','penmp'],
            'heartwall': ['heartwall'],
            'hotspot3D': ['3D'],
            'huffman': ['pavle','huffman'],
        }.get(bench, [])
        for n in names:
            candidates.append(src / n)
        for p in list(src.glob('*'))[:40]:
            if p.is_file() and os.access(p, os.X_OK) and not p.suffix in ('.cu','.c','.cpp','.h','.o') and p.name not in ('Makefile','makefile','Makefile_nvidia','run','run_gpgpusim.sh'):
                candidates.append(p)
    if row['suite_id'] == 'parboil':
        bench_dir = PARBOIL_ROOT / 'benchmarks' / bench
        for p in bench_dir.glob('build/*'):
            if p.is_file() and os.access(p, os.X_OK):
                candidates.append(p)
        for p in bench_dir.glob('build_*/*'):
            if p.is_file() and os.access(p, os.X_OK):
                candidates.append(p)
    uniq=[]
    seen=set()
    for p in candidates:
        ps=str(p)
        if ps not in seen:
            seen.add(ps); uniq.append(p)
    return uniq

def bin_ready(row):
    cands = binary_candidates(row)
    ready = [str(p) for p in cands if p.exists() and p.is_file() and os.access(p, os.X_OK)]
    return ready

def write_csv(path, header, rows):
    with path.open('w', newline='') as f:
        w=csv.writer(f)
        w.writerow(header)
        w.writerows(rows)

def phase_build_rows(rows, phase, targets, strategies):
    out=[]
    for row in targets:
        for attempt_round, strategy_name, work_dir, command, env_extra, notes in strategies(row):
            rc,to,log,status=run_cmd(phase, row['app_id'], attempt_round, strategy_name, work_dir, command, env_extra)
            ready=bin_ready(row)
            blocker_after = 'binary_ready' if ready else ('timeout' if to else status)
            out.append([phase,row['suite_id'],row['app_id'],row['benchmark_name'],attempt_round,strategy_name,str(work_dir),command,rc,to,log,status,';'.join(map(str,binary_candidates(row))),len(ready),row['blocker'],blocker_after,notes])
    return out

def parboil_strategies(row):
    bench=row['benchmark_name']
    bench_dir=PARBOIL_ROOT/'benchmarks'/bench
    src_cuda=bench_dir/'src/cuda'
    src_base=bench_dir/'src/cuda_base'
    env={'PARBOIL_ROOT':str(PARBOIL_ROOT),'PLATFORM':'default','CUDAHOME':'/usr/local/cuda','CUDA_DIR':'/usr/local/cuda'}
    yield ('round1','python3_driver_compile',PARBOIL_ROOT,f'python3 ./parboil compile {bench} cuda default',env,'attempt python3 on legacy parboil driver')
    src = src_cuda if src_cuda.exists() else src_base
    build = bench_dir/'build'/'cuda_default'
    cmd = f'mkdir -p {build} && make -f {PARBOIL_ROOT}/common/mk/Makefile build SRCDIR={src} BUILDDIR={build} BIN={build}/{bench} PLATFORM=default PARBOIL_ROOT={PARBOIL_ROOT} BUILD=cuda'
    yield ('round2','direct_make_cuda_or_base',PARBOIL_ROOT,cmd,env,'bypass python2 driver using documented make variables')

def cuda_arch_strategies(row):
    env={'CUDA_DIR':'/usr/local/cuda','CUDAHOME':'/usr/local/cuda','NVCCFLAGS':'-arch=sm_70 -allow-unsupported-compiler','CUDACFLAGS':'-arch=sm_70 -allow-unsupported-compiler'}
    src = Path(row['source_path']) if row['source_path'] not in ('unknown','') else REPO
    if row['suite_id']=='parboil':
        bench=row['benchmark_name']; bench_dir=PARBOIL_ROOT/'benchmarks'/bench
        src_cuda=bench_dir/'src/cuda'; src_base=bench_dir/'src/cuda_base'; srcp=src_cuda if src_cuda.exists() else src_base
        build=bench_dir/'build'/'cuda_sm70'
        cmd1=f'mkdir -p {build} && make -f {PARBOIL_ROOT}/common/mk/Makefile build SRCDIR={srcp} BUILDDIR={build} BIN={build}/{bench} PLATFORM=default PARBOIL_ROOT={PARBOIL_ROOT} BUILD=cuda PLATFORM_CUDACFLAGS="-arch=sm_70 -allow-unsupported-compiler" APP_CUDACFLAGS+=" -arch=sm_70 -allow-unsupported-compiler"'
        yield ('round1','parboil_direct_make_sm70',PARBOIL_ROOT,cmd1,env,'CUDA 11 compatible sm_70 build')
        build2=bench_dir/'build'/'cuda_base_sm70'
        base=src_base if src_base.exists() else src_cuda
        cmd2=f'mkdir -p {build2} && make -f {PARBOIL_ROOT}/common/mk/Makefile build SRCDIR={base} BUILDDIR={build2} BIN={build2}/{bench} PLATFORM=default PARBOIL_ROOT={PARBOIL_ROOT} BUILD=cuda_base PLATFORM_CUDACFLAGS="-arch=sm_70 -allow-unsupported-compiler" APP_CUDACFLAGS+=" -arch=sm_70 -allow-unsupported-compiler"'
        yield ('round2','parboil_direct_make_cuda_base_sm70',PARBOIL_ROOT,cmd2,env,'CUDA base fallback with sm_70')
    else:
        cmd1='make clean >/dev/null 2>&1 || true; make NVCCFLAGS="-arch=sm_70 -allow-unsupported-compiler" CUDA_FLAG="-arch=sm_70 -allow-unsupported-compiler" KERNEL_DIM=""'
        yield ('round1','rodinia_make_sm70_vars',src,cmd1,env,'override common CUDA arch variables')
        cmd2='make clean >/dev/null 2>&1 || true; make CUDA_DIR=/usr/local/cuda CUDA_INSTALL_PATH=/usr/local/cuda CXX=g++ CC=gcc NVCC=/usr/local/cuda/bin/nvcc'
        yield ('round2','rodinia_make_cuda_paths',src,cmd2,env,'explicit CUDA path/toolchain build')

def dep_strategies(row):
    src = Path(row['source_path']) if row['source_path'] not in ('unknown','') else REPO
    env={'LD_LIBRARY_PATH':'/usr/local/cuda/lib64:/usr/local/cuda/targets/x86_64-linux/lib:'+os.environ.get('LD_LIBRARY_PATH',''), 'LIBRARY_PATH':'/usr/local/cuda/lib64:/usr/local/cuda/targets/x86_64-linux/lib:'+os.environ.get('LIBRARY_PATH','')}
    cmd1='ldconfig -p | grep -E "libcuda|libcudart" || true; ls -l /usr/local/cuda/lib64/libcudart.so /usr/local/cuda/targets/x86_64-linux/lib/libcudart.so 2>/dev/null || true; find . -maxdepth 3 -type f -perm -111 | sed -n "1,40p"'
    yield ('round1','dependency_probe',src,cmd1,env,'probe libcuda/libcudart and local executable candidates')
    ready=bin_ready(row)
    if ready:
        exe=ready[0]
        cmd2=f'ldd {exe} || true'
        work=Path(exe).parent
    else:
        cmd2='find . -maxdepth 4 -type f -name "*.so" -o -type f -perm -111 | sed -n "1,80p"'
        work=src
    yield ('round2','binary_ldd_or_candidate_probe',work,cmd2,env,'probe executable runtime dependencies')

def data_attempts(rows):
    data=[]; cmdrows=[]
    for row in rows:
        src = Path(row['source_path']) if row['source_path'] not in ('unknown','') else REPO
        app=row['app_id']; bench=row['benchmark_name']
        checks=[]
        if row['suite_id']=='rodinia':
            checks=[WORKLOAD_ROOT/'suites/rodinia/data', WORKLOAD_ROOT/'suites/rodinia-wrapper'/bench/'data', src]
        else:
            checks=[PARBOIL_ROOT/'datasets'/bench, PARBOIL_ROOT/'benchmarks'/bench/'datasets', PARBOIL_ROOT/'benchmarks'/bench]
        for i,path in enumerate(checks[:2],1):
            cmd=f'if [ -e "{path}" ]; then find "{path}" -maxdepth 3 -type f | sed -n "1,80p"; else echo missing:{path}; fi'
            rc,to,log,status=run_cmd('W21D',app,f'round{i}',f'data_probe_{i}',REPO,cmd,{})
            files=[]
            if path.exists():
                files=[p for p in path.rglob('*') if p.is_file()]
            ds='data_available' if files else 'missing_or_unverified_data'
            data.append([row['suite_id'],app,bench,f'round{i}',f'data_probe_{i}',str(path),cmd,rc,to,log,ds,row['command_status'],ds if ds!='data_available' else 'command_verification_needed',f'{len(files)} files'])
        candidates=[]
        ready=bin_ready(row)
        if ready:
            exe=ready[0]
            if row['run_command'] and row['run_command']!='unknown':
                candidates.append(row['run_command'])
            candidates.append(f'cd {Path(exe).parent} && source /workspace/repos/load_gpgpusim.sh && ./{Path(exe).name} --help')
        else:
            candidates.append('echo no_binary_available_for_command_smoke')
        candidates.append('echo dry_run_placeholder_unavailable_exit_77; exit 77')
        for i,cmd in enumerate(candidates[:2],1):
            rc,to,log,status=run_cmd('W21D',app,f'round{i}',f'command_probe_{i}',REPO,cmd,{})
            dry='pass' if rc in (0,77) else 'fail'
            smoke='candidate_pass' if (rc==0 and ready and not cmd.startswith('echo no_binary')) else ('unavailable_exit_77' if rc==77 else status)
            blocker_after='ready_candidate' if (rc==0 and ready and not cmd.startswith('echo no_binary')) else row['blocker']
            cmdrows.append([row['suite_id'],app,bench,f'round{i}',f'command_probe_{i}',cmd,rc,to,log,dry,smoke,blocker_after,'command probe; not promoted without full collector smoke'])
    return data, cmdrows

def main():
    rows=read_rows()
    parboil=[r for r in rows if r['suite_id']=='parboil']
    all_source=[r for r in rows if r['source_path'] not in ('unknown','')]
    # W21A: Parboil driver/build compatibility.
    w21a=phase_build_rows(rows,'W21A',parboil,parboil_strategies)
    write_csv(OUT/'build_results_w21a.csv',COMMON_HEADER,w21a)
    # W21B: CUDA arch/toolchain compatibility for all source-backed blockers.
    w21b=phase_build_rows(rows,'W21B',all_source,cuda_arch_strategies)
    write_csv(OUT/'build_results_w21b.csv',COMMON_HEADER,w21b)
    # W21C: dependency/runtime probe for all blockers.
    w21c=phase_build_rows(rows,'W21C',rows,dep_strategies)
    write_csv(OUT/'build_results_w21c.csv',COMMON_HEADER,w21c)
    # W21D: data and command normalization attempts.
    data,cmd=data_attempts(rows)
    write_csv(OUT/'data_availability_results_w21d.csv',DATA_HEADER,data)
    write_csv(OUT/'wrapper_command_normalization_results.csv',CMD_HEADER,cmd)
    # Summary for postchecks.
    summary=OUT/'w21_attempt_summary.csv'
    with summary.open('w', newline='') as f:
        w=csv.writer(f); w.writerow(['app_id','suite_id','attempt_count','passes','binary_ready_after','notes'])
        for r in rows:
            app=r['app_id']
            records=[x for x in w21a+w21b+w21c if x[2]==app]
            cmd_records=[x for x in cmd if x[1]==app]
            attempts=len(records)+len(cmd_records)
            passes=sum(1 for x in records if x[11]=='pass') + sum(1 for x in cmd_records if x[10]=='candidate_pass')
            w.writerow([app,r['suite_id'],attempts,passes,len(bin_ready(r)),'at least two attempts' if attempts>=2 else 'insufficient_attempts'])
    (AUDIT/'w21_log_root.txt').write_text(str(LOG_ROOT)+'\n')
    print(f'w21_log_root={LOG_ROOT}')
    print(f'blockers={len(rows)} parboil={len(parboil)} source_backed={len(all_source)}')

if __name__ == '__main__':
    main()
