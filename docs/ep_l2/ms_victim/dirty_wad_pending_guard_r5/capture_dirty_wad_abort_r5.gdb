set pagination off
set confirm off
set auto-load safe-path /usr/share/gdb/auto-load:/usr/lib/x86_64-linux-gnu
handle SIGABRT stop print nopass
python
import gdb,json,sys
sys.path.insert(0,'/usr/share/gcc/python')
import libstdcxx.v6.printers
libstdcxx.v6.printers.register_libstdcxx_printers(None)
end
run
python
row={'scope':'READ_ONLY_FAILED_NATIVE_STATE_NO_INFERIOR_FUNCTION_CALLS','frames':[],'current_mf':None,'pending':None}
pid=gdb.selected_inferior().pid;row['inferior_pid']=pid
row['start_ticks']=int(open('/proc/%d/stat'%pid).read().rsplit(')',1)[1].split()[19]);row['boot_id']=open('/proc/sys/kernel/random/boot_id').read().strip()
f=gdb.newest_frame();directory=None
while f:
    name=f.name() or '';item={'name':name}
    try:item['line']=f.find_sal().line
    except Exception:pass
    if 'preview_access' in name:
        try:
            obj=f.read_var('this').dereference();p=obj['m_ft_pending'];mf=f.read_var('mf')
            row['current_mf']=int(mf);row['current_uid']=int(mf.dereference()['m_request_uid'])
            row['pending']={k:int(p[k]) for k in ['tag_index','victim_address','dirty','wad','bound']}
            row['pending']['request']=int(p['request']);row['pending']['uid']=int(p['request'].dereference()['m_request_uid'])
            row['pending']['same_request']=int(p['request'])==int(mf)
            row['pending']['wad_ticket']={k:int(p['wad_ticket'][k]) for k in ['index','incarnation']}
            row['pending']['victim']={k:int(p['victim'][k]) for k in ['payload_id','generation']}
        except Exception as e:item['state_error']=str(e)
    if 'preview_install' in name:
        try:
            directory=f.read_var('this').dereference()
            for k in ['line','dirty','native_owner_proven_unique']:
                try:item[k]=int(f.read_var(k))
                except Exception as e:item[k+'_error']=str(e)
        except Exception as e:item['directory_error']=str(e)
    row['frames'].append(item);f=f.older()
if directory is not None and row['pending'] is not None:
    target=row['pending']['victim_address'];vis=gdb.default_visualizer(directory['m_entries']);matches=[]
    if vis:
        for i,(n,e) in enumerate(vis.children()):
            if int(e['status']) and int(e['line'])==target:
                entry={k:int(e[k]) for k in ['status','line','incarnation','readers','wb_uid_bound','wb_request_uid']}
                entry['index']=i;entry['payload']={k:int(e['payload'][k]) for k in ['payload_id','generation']};matches.append(entry)
    row['matching_existing_rows']=matches
row['captured_sigabrt']=True
gdb.write('\nB_DIRTY_WAD_ABORT_R5|'+json.dumps(row,sort_keys=True)+'\n')
end
quit
