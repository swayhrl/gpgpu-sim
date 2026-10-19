#!/usr/bin/env python3
"""Frozen-source, bounded production-path RO qualification fixtures (no workload simulation)."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()


def check_four_mode_observations(text):
    """The tiny same-set A/B/A witness must pay for duplicate lower work."""
    cells={};current=None
    for line in text.splitlines():
        fields=line.split('|')
        if fields[0]=='RO_C_FOUR_MODE_BEGIN':
            current=tuple(int(f.split('=')[1]) for f in fields[1:])
        elif line.startswith('EPL2ROC1|LOWER_CONCURRENCY|') and current is not None:
            values=dict(f.split('=',1) for f in fields[2:])
            cells[current]=(int(values['peak_lower_transactions']),int(values['peak_independent_lower_lines']))
            current=None
    # 00 has no RO runtime. M-only cannot pass the occupied Tag. T-only
    # finds old A via the real line record; T+M issues A twice as well as B.
    expected={(0,1):(1,1),(1,0):(2,2),(1,1):(3,2)}
    if cells!=expected:raise RuntimeError(f'RO native lower-concurrency witness mismatch: {cells!r} != {expected!r}')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--source',required=True);a=p.parse_args();root=Path(__file__).resolve().parents[2]
    subprocess.run(['git','-C',str(root),'diff','--exit-code',a.source,'--','src','tests','CMakeLists.txt'],check=True,stdout=subprocess.DEVNULL)
    a.output.mkdir(parents=True,exist_ok=False);o=a.output.resolve();resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    command=b'kernel-fixture.traceg\n';(o/'kernelslist.g').write_bytes(command)
    ranges=b'first_line,last_line_inclusive\n0000000000000000,000000000003ff80\n';(o/'ranges.csv').write_bytes(ranges)
    m=dict(schema='EP_L2_RO_ORACLE_V1',status='SYNTHETIC_DIRECTED_FIXTURE',epoch='whole_commandlist',
        scope='TRACE_MODEL_CLOSED_WORLD',address_domain='CANONICAL_GPU_GLOBAL_VA_NO_TRANSLATION_ALIAS',
        unresolved_opcodes={},commandlist_sha256=sha(o/'kernelslist.g'),artifacts={'certified_ranges.csv':sha(o/'ranges.csv')})
    (o/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
    config=f'-gpgpu_ep_l2_ro_ranges {o}/ranges.csv\n-gpgpu_ep_l2_ro_ranges_sha256 {sha(o/"ranges.csv")}\n-gpgpu_ep_l2_ro_manifest {o}/manifest.json\n-gpgpu_ep_l2_ro_manifest_sha256 {sha(o/"manifest.json")}\n-gpgpu_ep_l2_ro_scope TRACE_MODEL_CLOSED_WORLD\n'
    (o/'binding.config').write_text(config)
    base=root/'configs/tested-cfgs/SM7_QV100/gpgpusim.config';overlay=root/'tests/ep_l2/ft_wbuf.config'
    runtime=a.build.resolve()/'ep_l2_ro_runtime';part=a.build.resolve()/'ep_l2_ro_partition'
    commands=[('runtime',[str(runtime),str(base),str(overlay),str(o/'binding.config'),str(o/'kernelslist.g')],None)]
    for d in [0,1]:
        for b in [0,1]:commands.append((f'partition_{d}{b}',[str(part),str(base),str(overlay),str(o/'binding.config'),str(o/'kernelslist.g'),str(d),str(b)],None))
    # Repair qualification: real warmup establishes nonzero native Tag offset;
    # independently count bank grants by physical GPU cycle for both budgets.
    for entries in [16]:
        for d in [0,1]:
            for b in [0,1]:
                commands.append((f'partition_warm_C{entries}_{d}{b}',
                    [str(part),str(base),str(overlay),str(o/'binding.config'),str(o/'kernelslist.g'),
                     str(d),str(b),'4096',str(entries)],None))
    for d in [0,1]:
        for b in [0,1]:
            commands.append((f'gpu_epoch_{d}{b}',[str(a.build.resolve()/'ep_l2_ro_epoch_entry'),
                str(base),str(overlay),str(o/'binding.config'),str(o/'kernelslist.g'),str(d),str(b)],None))
    for target in ['ro_transactions','ro_store_port','ro_oracle_certificate']:
        commands.append((target,[str(a.build.resolve()/('ep_l2_'+target))],None))
    (o/'texture_only.config').write_text(config+'-gpgpu_cache:dl2_texture_only 1\n')
    commands.append(('reject_texture_only',[str(a.build.resolve()/'ep_l2_ro_epoch_entry'),
        str(base),str(overlay),str(o/'texture_only.config'),str(o/'kernelslist.g'),'1','1'],
        'RO requires enabled L2 for all access types'))
    # A's mandatory affected-common suite owns the same-binary native OFF run.
    cases=[('range_hash',config.replace(sha(o/'ranges.csv'),'0'*64),'RO range SHA256 mismatch'),
           ('manifest_hash',config.replace(sha(o/'manifest.json'),'0'*64),'RO manifest SHA256 mismatch'),
           ('scope',config.replace('TRACE_MODEL_CLOSED_WORLD','KERNEL_ONLY'),'unverified oracle scope/identity')]
    wrong=dict(m,epoch='one_kernel');(o/'wrong_epoch.json').write_text(json.dumps(wrong)+'\n')
    cases.append(('epoch',config.replace(str(o/'manifest.json'),str(o/'wrong_epoch.json')).replace(sha(o/'manifest.json'),sha(o/'wrong_epoch.json')),'unsupported/incomplete epoch certificate'))
    for name,text,reason in cases:
        path=o/(name+'.config');path.write_text(text)
        commands.append(('reject_'+name,[str(runtime),str(base),str(overlay),str(path),str(o/'kernelslist.g')],reason))
    (o/'wrong_command.g').write_text('kernel-wrong.traceg\n')
    commands.append(('reject_command',[str(runtime),str(base),str(overlay),str(o/'binding.config'),str(o/'wrong_command.g')],'RO commandlist identity mismatch'))
    rows=[]
    for name,cmd,reason in commands:
        log=o/(name+'.log')
        with log.open('w') as f:r=subprocess.run(cmd,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=120)
        text=log.read_text();ok=(r.returncode!=0 and reason in text) if reason else r.returncode==0 and 'PASS' in text
        if ok and name=='runtime':check_four_mode_observations(text)
        row=dict(name=name,status='PASS' if ok else 'FAIL',exit_code=r.returncode,expected_rejection=reason,
            command=cmd,binary_sha256=sha(Path(cmd[0])),log_sha256=sha(log))
        rows.append(row);print(name,row['status'],flush=True)
        if not ok:raise RuntimeError(text[-4000:])
    summary=dict(schema='EPL2_MS_C_RO_PRODUCTION_DIRECTED_V1',core_source_commit=a.source,
        status='PRODUCER_DIRECTED_PASS_NOT_E_INDEPENDENT_QUALIFICATION',
        fixture='SYNTHETIC_SECTOR_VERSION_REFERENCE_NOT_SASS_NUMERICAL_OUTPUT',
        base_config_sha256=sha(base),overlay_config_sha256=sha(overlay),
        fixture_config_sha256=sha(o/'binding.config'),fixture_command_sha256=sha(o/'kernelslist.g'),
        manifest_sha256=sha(o/'manifest.json'),ranges_sha256=sha(o/'ranges.csv'),tests=rows)
    (o/'SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n')


if __name__=='__main__':main()
