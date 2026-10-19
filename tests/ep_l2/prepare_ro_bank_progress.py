#!/usr/bin/env python3
"""Materialize immutable production headers/methods for the tiny RO Bank fixture.
No compiler, registry claim, simulator, trace read or scientific qualification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def function(text, signature):
    start=text.index(signature)
    mask=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', lambda m:' '*len(m[0]),text)
    begin=mask.index('{',start);depth=0
    for n in range(begin,len(mask)):
        if mask[n]=='{':depth+=1
        elif mask[n]=='}':
            depth-=1
            if not depth:return text[start:n+1]
    raise ValueError('unclosed '+signature)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',required=True);p.add_argument('--store-source');p.add_argument('--runtime-source')
    p.add_argument('--output',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[2]
    a.source=subprocess.check_output(['git','-C',str(root),'rev-parse',a.source+'^{commit}'],text=True).strip()
    a.output.mkdir(parents=True,exist_ok=False);out=a.output.resolve();binding={}
    def read(name,source=None):
        sha=source or a.source;path='src/gpgpu-sim/'+name
        data=subprocess.check_output(['git','-C',str(root),'show',sha+':'+path])
        binding[name]={'source':sha,'path':path,'sha256':hashlib.sha256(data).hexdigest()}
        return data.decode()
    for name in ['gpu-cache.h','ep-l2-ro-transactions.h','ep-l2-ro-oracle.h','ep-l2-ro-store-port.h']:
        source=a.store_source if name=='ep-l2-ro-store-port.h' else None
        (out/name).write_text(read(name,source))
    cache=read('gpu-cache.cc');ro_mshr=read('ep-l2-ro-mshr.inc');methods=[];spans={}
    for sig in ['unsigned mshr_table::ro_targets_on_line','unsigned mshr_table::targets_on_line',
                'bool mshr_table::reserve_ro_descriptor','bool mshr_table::ro_descriptor_live',
                'void mshr_table::release_ro_descriptor','bool mshr_table::reserve_ro_line',
                'void mshr_table::bind_ro_line','bool mshr_table::lookup_ro_line',
                'void mshr_table::release_ro_line','void mshr_table::ro_line_issue',
                 'void mshr_table::ro_line_ready',
                 'mem_fetch *mshr_table::ro_descriptor_request',
                 'mem_access_sector_mask_t mshr_table::ro_descriptor_sectors',
                 'unsigned mshr_table::num_targets_used']:
        body=function(cache if sig=='unsigned mshr_table::num_targets_used' else ro_mshr,sig);methods.append(body);spans[sig]=hashlib.sha256(body.encode()).hexdigest()
    (out/'mshr_methods.inc').write_text('\n'.join(methods)+'\n')
    rt=read('ep-l2-ro-runtime.inc',a.runtime_source);methods=[]
    for sig in ['  bool has_response()','  bool prepare(uint64_t cycle)','  void commit(uint64_t cycle)']:
        body=function(rt,sig);methods.append(body);spans[sig]=hashlib.sha256(body.encode()).hexdigest()
    (out/'runtime_methods.inc').write_text('\n'.join(methods)+'\n')
    ft=read('ep-l2-ft-cache.inc',a.runtime_source);body=function(ft,'bool l2_cache::ft_response_ready(')
    spans['l2_cache::ft_response_ready']=hashlib.sha256(body.encode()).hexdigest()
    # Only class qualifiers are renamed for the surrounding scheduler facade.
    body=body.replace('l2_cache::ft_response_ready','scheduler_cache::ft_response_ready').replace('baseline_cache::','native_queue_facade::')
    (out/'response_scheduler.inc').write_text(body+'\n')
    fixture=root/'tests/ep_l2/test_ro_bank_progress.cc';(out/'test_ro_bank_progress.cc').write_bytes(fixture.read_bytes())
    x={'schema':'RO_BANK_SOURCE_BOUND_CPP_FIXTURE_V1','base_source':a.source,'headers':binding,'exact_method_sha256':spans,
       'fixture_sha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),
       'scope':'Actual payload-store/transaction/store-port headers and14 actual MSHR definitions; exact runtime has_response/prepare/commit and response scheduler bodies.',
       'facades':'Synthetic physical clock, Tag callbacks, native ready flag and requester packet copy sink. Runtime response adapter forwards to actual transactions. Scheduler class qualifiers only renamed. Not a full cache/partition/DRAM simulation.',
       'oracle':'Inline synthetic directed ranges only; no frozen real oracle input read/changed.'}
    (out/'SOURCE_BINDING.json').write_text(json.dumps(x,indent=2)+'\n');print(out)


if __name__=='__main__':main()
