#!/usr/bin/env python3
"""Propose incremental dirty/fixed hooks on A949 CLEAN_ONLY; never apply files."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
from pending_adapter import extend as pending
from fixed_adapter import extend as fixed

BASE='94996ec59b4a0264b64146932977bfe6ea4f195b'
FILES=['src/gpgpu-sim/'+p for p in ('gpu-cache.h','gpu-cache.cc','gpu-sim.cc','ep-l2-ft-cache.inc','ep-l2-rg1.inc')]+['CMakeLists.txt']

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);args=p.parse_args()
    root=Path(__file__).resolve().parents[3]
    original={p:subprocess.check_output(['git','show',BASE+':'+p],cwd=root).decode() for p in FILES}
    text=dict(original)
    def replace(path,before,after):
        assert text[path].count(before)==1,(path,before)
        text[path]=text[path].replace(before,after)
    h='src/gpgpu-sim/gpu-cache.h'
    # A disabled this unqualified mode deliberately. This separate B SOURCE
    # candidate proposes real dirty consumers; only A/E can open production.
    replace(h,'''    if (m_config.m_ep_l2_tvd_dirty) {
      fprintf(stderr,"A R2 integrates CLEAN_ONLY source; dirty pending/inflight candidate remains unqualified and disabled\\n");
      abort();
    }
''','')
    replace(h,'    if (m_config.m_ep_l2_tvd_clean &&\n        (m_config.m_ep_l2_diag_copy_free',
            '    if ((m_config.m_ep_l2_tvd_clean || m_config.m_ep_l2_tvd_dirty) &&\n        (m_config.m_ep_l2_diag_copy_free')
    replace(h,'A R2 CLEAN_ONLY is separate from diagnostics/observer; original finite native cache policy required',
            'B dirty/fixed SOURCE is separate from diagnostics/observer; finite policy required; NOT qualified')
    replace(h,'  unsigned long long tvd_lookup_queries_for_test() const { return m_tvd_lookup_queries; }',
            '  unsigned long long tvd_lookup_queries_for_test() const { return m_tvd_lookup_queries; }\n  bool tvd_lookup_started_for_test(const mem_fetch *mf) const { return m_tvd_lookup==mf; }')
    pending(text);fixed(text)
    text['CMakeLists.txt']+='''
# Separate B finite component targets, opt-in only; A owns native build/release.
foreach(victim_unit bank_identity fixed_partition)
  add_executable(ep_l2_ms_victim_${victim_unit} EXCLUDE_FROM_ALL tests/ep_l2/ms_victim/test_${victim_unit}.cc)
  target_include_directories(ep_l2_ms_victim_${victim_unit} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_ms_victim_${victim_unit} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()
'''
    # The reused extensions add only their narrow hooks. A949's indexed WB
    # identity/port/rate corrections must survive this incremental proposal.
    markers=['p.wad_ticket','r.wad_ticket = p.wad_ticket','bind_writeback(',
             'const auto t=it->second.wad_ticket','writeback_matches(']
    ft=text['src/gpgpu-sim/ep-l2-ft-cache.inc']
    for marker in markers:assert marker in ft,marker
    cc=text['src/gpgpu-sim/gpu-cache.cc']
    for marker in ['m_tvd_completion_cam_cycle!=cycle','writeback_matches(completion,normalized,completing_request_uid)',
                   '++m_tvd_indexed_completions']:assert marker in cc,marker
    patch=''.join('diff --git a/{0} b/{0}\n'.format(path)+''.join(difflib.unified_diff(
        original[path].splitlines(True),text[path].splitlines(True),fromfile='a/'+path,tofile='b/'+path))
        for path in FILES if original[path]!=text[path]).replace('\n \n','\n\n')
    args.out.mkdir(parents=True,exist_ok=True)
    (args.out/'A_INCREMENTAL_HOOKS.patch').write_text(patch)
    manifest={'base_core':BASE,'A_clean_integration_preserved':True,'public_files_mutated':False,
              'patch_applied':False,'compiled':False,'native_tests_run':False,
              'status':'INCREMENTAL_SOURCE_PROPOSAL_NOT_QUALIFIED','patch_sha256':hashlib.sha256(patch.encode()).hexdigest(),
              'files':{p:{'base_sha256':hashlib.sha256(original[p].encode()).hexdigest(),
                          'proposed_sha256':hashlib.sha256(text[p].encode()).hexdigest()} for p in FILES}}
    (args.out/'ADAPTER_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'status':manifest['status'],'base':BASE,'files':len(FILES),'patch_bytes':len(patch)}))

if __name__=='__main__':main()
