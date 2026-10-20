from pathlib import Path
import sys,json,hashlib,subprocess,os,time,signal,resource,datetime
core=Path('/workspace/worktrees/gpgpu-sim-ep-l2-ms20261019-b-victim-v1');a=Path('/workspace/worktrees/gpgpu-sim-ep-l2-ms20261019-a-r3-victim-full-v1');ap=Path('/workspace/ep_l2_ms20261019_e_results/A_COMMON_R14_FAST_REPAIR_r1/RECEIPT.json');parent=json.loads(ap.read_bytes())
binary=Path('/workspace/build/ep-l2-ms20261019-a-common-native-r1/gpgpu-sim/ep_l2_ms_victim_production');base=a/'configs/tested-cfgs/SM7_QV100/gpgpusim.config';overlay=a/'tests/ep_l2/ft_wbuf.config'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert sha(binary)==parent['test_binary_sha256'][str(binary)]=='cadf8d519abd1590f338b8cede0e8cf459b81cf16c0ec2bc81ef99b73caaa64b'
for p in [base,overlay]:
 assert p.read_bytes()==subprocess.check_output(['git','show',parent['fixture_source_core']+':'+str(p.relative_to(a))],cwd=core)
for p,h in parent['dynamic_library_sha256'].items():assert sha(p)==h,p
fixture='tests/ep_l2/test_tvd_production.cc';old=parent['fixture_source_core'];prod=parent['runtime_producer_core'];current='27ec10bda848e4929181c60a7cbb40d6b007945c'
oldbody=subprocess.check_output(['git','show',old+':'+fixture],cwd=core);assert oldbody==subprocess.check_output(['git','show',current+':'+fixture],cwd=core)
diff=subprocess.check_output(['git','diff','--name-only',prod,current,'--','src'],cwd=core).decode().splitlines();assert diff==['src/gpgpu-sim/ep-l2-ro-runtime.inc'],diff
flags=Path('/workspace/build/ep-l2-ms20261019-a-common-native-r1/gpgpu-sim/CMakeFiles/ep_l2_ms_victim_production.dir/flags.make');assert '-DNDEBUG' not in flags.read_text();assert sha(flags)==parent['assertion_flags_sha256'][str(flags)]
e=Path('/workspace/worktrees/accel-sim-ep-l2-ms20261019-e-eval-v1/util/ep_l2/mechanism_e');sys.path.insert(0,str(e));from coordinator import Coordinator,verify_extension_binding
client=Path('/workspace/ep_l2_ft20261016_shared/mechanism_study_20261019_v1/CLIENT_CONTRACT.json');epub=verify_extension_binding(client)
out=Path('/workspace/ep_l2_ms20261019_e_results/B_FROZEN_DIRTY_WAD_GDB_r5');assert not out.exists();coord=Coordinator();lease=coord.reserve_work('B','test',1,4*1024**3,16*1024**2,str(out));out.mkdir();key=lease['lease_id'];p=None
cmd=['/usr/bin/gdb','-q','-nx','-batch','-iex','maintenance set worker-threads 1','-iex','set debuginfod enabled off','-x','/workspace/ep_l2_ms20261019_b_victim/capture_dirty_wad_abort_r5.gdb','--args',str(binary),str(base),str(overlay),'--study-dirty-inflight'];receipt=dict(schema='B_FROZEN_A_NATIVE_DIRTY_ABORT_DIAGNOSTIC_R5',at=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope='BOUNDED_REAL_PARTITION_SIMPLE_DRAM_FUNCTIONAL_NOT_CORE27_MAIN773_OR_FULL_APPLICATION_QUALIFICATION',A_parent_receipt=str(ap),A_parent_receipt_sha256=sha(ap),runtime_producer_Core=prod,fixture_Core=old,current_Core27_not_executed=current,current_main773_not_executed=True,source_difference_only='RO_RUNTIME_IMPLEMENTATION_DISABLED_IN_THIS_CLEAN_CASE',fixture_source_identical_old_to_Core27=True,framework=parent['framework_source_commit'],vendor=parent['vendor'],binary=str(binary),binary_sha256=sha(binary),command=cmd,base_config_sha256=sha(base),overlay_sha256=sha(overlay),assertion_flags_sha256=sha(flags),E_source=epub,E_lease=lease,builds_or_links=0,full_applications_by_B=0,E_qualification_reset=False)
try:
 def limits():
  resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(4*1024**3,4*1024**3));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
 with (out/'native.log').open('wb') as log:
  child_env=dict(os.environ);child_env.pop('PYTHONHOME',None);child_env.pop('PYTHONPATH',None);receipt['GDB_child_environment_repair']='Remove inherited Python runtime variables for systemGDB only; global environment unchanged';p=subprocess.Popen(cmd,env=child_env,stdout=log,stderr=subprocess.STDOUT,cwd=str(out),start_new_session=True,preexec_fn=limits)
  receipt['process']=coord.bind_work_process(key,'B',p.pid);start=time.monotonic()
  while p.poll() is None:
   st=os.statvfs('/workspace')
   if time.monotonic()-start>75 or st.f_bavail*st.f_frsize<lease['disk_floor_bytes'] or (out/'native.log').stat().st_size>12*1024**2:
    os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=3);raise RuntimeError('owned native test time/disk/output guard')
   time.sleep(.2)
  receipt['actual_exit']=p.returncode;receipt['wall_seconds']=time.monotonic()-start
 text=(out/'native.log').read_text();terms=['PASS production_clean_victim_lookup_bank_icnt_same_handle_no_lower_read','PASS production_clean_missing_sector_fallback_epoch','PASS production_clean_write_promotion_dirty_victim_real_wb_reference','PASS production_clean_atomic_promotion_dirty_victim_real_wb_reference','NATIVE_DRAM_ISSUE_MATCHED|off=2|clean=1|status=PASS','MATCHED_LOWER|off_packets=2|on_packets=1|off_bytes=64|on_bytes=32','PASS production_primary_64sets_16ways_bank_class_preserved_same_handle_promotion']
 receipt['terminal_checks']={t:t in text for t in terms};rows=[json.loads(l.split('|',1)[1]) for l in text.splitlines() if l.startswith('B_DRAM_ACCEPT_R4|')];receipt['actual_FIFO_accept_observations']=rows;receipt['observations_valid']=len(rows)==2 and [r.get('actual_simple_DRAM_read_accepts') for r in rows]==[2,1] and all(r.get('status')=='OBSERVED' and r.get('all_read_UIDs_exactly_once') for r in rows);receipt['terminal_checks']['INDEPENDENT_ACTUAL_FIFO_ACCEPTS_2_1']=receipt['observations_valid'];receipt['raw_log_sha256']=sha(out/'native.log');receipt['status']='FINITE_FROZEN_ELF_EXPECTATIONS_PASS_NOT_E_ACCEPTANCE' if p.returncode==0 and all(receipt['terminal_checks'].values()) else 'FAILED_NATIVE_RETAINED_NO_PRODUCTION_CHANGE';receipt['atomic_scope']='CONSERVATIVE_NATIVE_PROMOTION_PROTOCOL_ONLY_NOT_ARITHMETIC_OR_APPLICATION_QUALIFICATION'
finally:
 if p is not None and p.poll() is None:
  os.killpg(p.pid,signal.SIGKILL);p.wait()
 coord.release_work(key,'B','One existing frozen A test ELF --study-clean finished; no build/link/main/app. Full actual receipt retained.',no_child_started=p is None);receipt['lease_released']=True;(out/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt));print((out/'native.log').read_text()[-2200:])
