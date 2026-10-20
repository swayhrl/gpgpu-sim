set pagination off
set confirm off
set print elements 16
set auto-load safe-path /usr/share/gdb/auto-load:/usr/lib/x86_64-linux-gnu
python
import gdb,json,sys,os
sys.path.insert(0,'/usr/share/gcc/python')
import libstdcxx.v6.printers
libstdcxx.v6.printers.register_libstdcxx_printers(None)
class Capture(gdb.Breakpoint):
    def __init__(self):
        super().__init__('/workspace/worktrees/gpgpu-sim-ep-l2-ms20261019-a-r3-victim-full-v1/tests/ep_l2/test_tvd_production.cc:324')
        self.hits=0
    def stop(self):
        self.hits+=1
        row={'iteration_order':self.hits-1,'scope':'READ_ONLY_EXISTING_FIXTURE_FIFO_REMOVAL_MAP','inferior_pid':gdb.selected_inferior().pid}
        try:
            f=gdb.selected_frame().read_var('f')
            try:row['on']=int(gdb.selected_frame().read_var('on'))
            except Exception:row['on']=None
            m=f['issued_lower_reads'];vis=gdb.default_visualizer(m)
            if vis is None:raise RuntimeError('stdmap visualizer unavailable; no inference substituted')
            entries=list(vis.children());assert len(entries)%2==0
            pairs=[(int(entries[i][1]),int(entries[i+1][1])) for i in range(0,len(entries),2)]
            row['read_uid_issue_counts']=pairs;row['actual_simple_DRAM_read_accepts']=sum(n for uid,n in pairs)
            row['all_read_UIDs_exactly_once']=all(n==1 for uid,n in pairs)
            row['proc_start_ticks']=int(open('/proc/%s/stat'%row['inferior_pid']).read().rsplit(')',1)[1].split()[19])
            row['boot_id']=open('/proc/sys/kernel/random/boot_id').read().strip()
            row['status']='OBSERVED'
        except Exception as e:row['status']='OBSERVABILITY_FAILED';row['error']=str(e)
        gdb.write('B_DRAM_ACCEPT_R4|'+json.dumps(row,sort_keys=True)+'\n')
        return False
cap=Capture()
def exited(e):gdb.write('B_DRAM_NATIVE_EXIT_R4|'+json.dumps({'exit_code':getattr(e,'exit_code',None),'capture_hits':cap.hits})+'\n')
gdb.events.exited.connect(exited)
end
run
