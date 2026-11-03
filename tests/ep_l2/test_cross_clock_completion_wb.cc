// Directed CPU memory-component fixture; no input Trace or application kernels.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
#include "gpgpu_context.h"
#include "gpu-sim.h"
#include "icnt_wrapper.h"
#include "l2cache.h"
#include "mem_latency_stat.h"
#include "option_parser.h"
#include "trace_driven.h"
struct calendar {
 double ct=0,it=0,dt=0,lt=0,cp,ip,dp,lp;
 int next(){double sm=std::min(ct,std::min(it,dt));int m=0;if(lt<=sm){sm=lt;m|=8;lt+=lp;}if(it<=sm){m|=2;it+=ip;}if(dt<=sm){m|=4;dt+=dp;}if(ct<=sm){m|=1;ct+=cp;}return m;}
};
int main(int argc,char**argv){
 assert(argc==4);setvbuf(stdout,nullptr,_IONBF,0);unsigned mhz=std::stoul(argv[2]),writes=std::stoul(argv[3]);
 gpgpu_context ctx;gpgpu_sim_config cfg(&ctx);option_parser_t opt=option_parser_create();ctx.ptx_reg_options(opt);ctx.func_sim->ptx_opcocde_latency_options(opt);icnt_reg_options(opt);cfg.reg_options(opt);trace_config tc;tc.reg_options(opt);option_parser_cfgfile(opt,argv[1]);cfg.init();option_parser_destroy(opt);
 exec_gpgpu_sim gpu(cfg,&ctx);gpu.gpu_sim_cycle=1;const memory_config*mc=gpu.getMemoryConfig();memory_stats_t stats(cfg.num_shader(),gpu.getShaderCoreConfig(),mc,&gpu);memory_partition_unit part(0,mc,&stats,&gpu);partition_mf_allocator allocator(mc);auto sub=part.get_sub_partition(0);auto cache=sub->ft_cache_for_test();assert(cache->tvd_enabled());
 unsigned responses=0;std::set<unsigned> seen;auto collect=[&](){while(auto p=sub->pop()){assert(seen.insert(p->get_request_uid()).second);++responses;delete p;}};
 unsigned made=0;new_addr_type addr=0;while(made<writes){addrdec_t decoded;mc->m_address_mapping.addrdec_tlx(addr,&decoded);if(decoded.sub_partition!=0){addr+=128;continue;}
  active_mask_t active;active.set(0);mem_access_byte_mask_t bytes;for(unsigned b=0;b<32;++b)bytes.set(b);mem_access_sector_mask_t sec;sec.set(0);auto mf=allocator.alloc(addr,GLOBAL_ACC_W,active,bytes,sec,32,true,gpu.gpu_sim_cycle,0,0,0,nullptr,0);assert(mf->get_sub_partition_id()==0);assert(!sub->full());sub->push(mf,gpu.gpu_sim_cycle);
  unsigned expect=made+1;for(unsigned n=0;n<2000&&responses<expect;++n){part.cache_cycle(gpu.gpu_sim_cycle);collect();++gpu.gpu_sim_cycle;}assert(responses==expect);++made;addr+=128;
 }
 unsigned pending=cache->ep_l2_wad_occupancy();assert(pending>2&&pending<128);printf("CPU_WB_PREPARED|DRAM=%u|writes=%u|realWAD=%u|lowerFIFO=%u|actual_TVD=1|partial_sector_bytes=32|GPU_application_kernels=0\n",mhz,writes,pending,sub->ep_l2_l2dram_occupancy());
 calendar cal;cal.cp=cal.ip=cal.lp=1./1132.;cal.dp=1./double(mhz);unsigned steps=0,dram_ticks=0;std::map<unsigned long long,unsigned> callbacks;
 while((part.busy()||cache->ep_l2_wad_occupancy()||!cache->ft_drained()||!sub->L2_dram_queue_empty()||!part.l2_char_no_credit_leak())&&steps++<2000000){int m=cal.next();if(m&4){++callbacks[gpu.gpu_sim_cycle];++dram_ticks;part.dram_cycle();}if(m&8){part.cache_cycle(gpu.gpu_sim_cycle);collect();}if(m&1)++gpu.gpu_sim_cycle;}
 printf("CPU_DRAIN_STATE|steps=%u|busy=%u|WAD=%u|lower=%u|ft=%u|creditclean=%u\n",steps,part.busy(),cache->ep_l2_wad_occupancy(),sub->ep_l2_l2dram_occupancy(),cache->ft_drained(),part.l2_char_no_credit_leak());
 assert(!part.busy()&&sub->l2_char_no_resource_leak()&&cache->ft_drained()&&cache->ep_l2_wad_occupancy()==0&&part.l2_char_no_credit_leak());assert(responses==writes);
 printf("CPU_WB_DRAIN_PASS|DRAM=%u|writes=%u|responses=%u|initialWB=%u|DRAMticks=%u|steps=%u|core_counter=%llu|port_count=1|app_kernels=0|Trace_body_reads=0\n",mhz,writes,responses,pending,dram_ticks,steps,gpu.gpu_sim_cycle);
 return 0;
}
