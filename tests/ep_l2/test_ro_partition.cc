// Actual memory_sub_partition frontend -> real memport/DRAM -> ReturnQ -> ICNT.
#include <cassert>
#include <cstdio>
#include <set>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"
int main(int argc,char**argv){
  assert(argc==7 || argc==9);gpgpu_context ctx;gpgpu_sim_config cfg(&ctx);option_parser_t p=option_parser_create();
  ctx.ptx_reg_options(p);ctx.func_sim->ptx_opcocde_latency_options(p);icnt_reg_options(p);cfg.reg_options(p);
  option_parser_cfgfile(p,argv[1]);option_parser_cfgfile(p,argv[2]);option_parser_cfgfile(p,argv[3]);
  const char *a[]={"ro-partition","-gpgpu_ep_l2_ft_mode","2","-gpgpu_ep_l2_wbuf_entries",argc==9?argv[8]:"16",
    "-gpgpu_ep_l2_ro_tag_detach",argv[5],"-gpgpu_ep_l2_ro_line_mshr_bypass",argv[6],
    "-gpgpu_dram_partition_queues","512:16:64:1",
    "-gpgpu_cache:dl2","S:1:128:1,L:B:m:L:L,A:128:32,256:0,32"};
  option_parser_cmdline(p,13,a);cfg.init();option_parser_destroy(p);
  exec_gpgpu_sim gpu(cfg,&ctx);auto mc=gpu.getMemoryConfig();memory_stats_t stats(cfg.num_shader(),gpu.getShaderCoreConfig(),mc,&gpu);
  memory_partition_unit part(0,mc,&stats,&gpu);auto sp=part.get_sub_partition(0);partition_mf_allocator alloc(mc);
  sp->ro_bind_commandlist(argv[4]);
  const unsigned offset=argc==9?std::stoul(argv[7]):0;
  for(unsigned i=0;i<offset;++i)sp->force_l2_tag_update((i%2)*32,0,mem_access_sector_mask_t(1<<(i%2)));
  sp->ro_begin_epoch();
  const bool ro_on=std::stoul(argv[5])||std::stoul(argv[6]);
  const auto activated_offset=sp->ft_memcpy_offset_for_test();
  if(ro_on) {
    assert(!sp->ft_try_force_l2_tag_update(0,0,mem_access_sector_mask_t(1)));
    assert(sp->ft_memcpy_offset_for_test()==activated_offset);
  }
  active_mask_t act;act.set(0);std::set<unsigned> expected,seen;
  for(unsigned i=0;i<4;++i){unsigned addr=offset ? (i==0?0:i==1?96:(i%2)*128) : (i%2)*128;
    unsigned sector=(addr>>5)&3;
    mem_access_byte_mask_t bytes;for(unsigned n=sector*32;n<sector*32+32;++n)bytes.set(n);
    auto m=alloc.alloc(addr,GLOBAL_ACC_R,act,bytes,mem_access_sector_mask_t(1<<sector),32,false,0,0,0,0,NULL,0);
    expected.insert(m->get_request_uid());sp->push(m,0);
  }
  bool backpressure=false;unsigned peak=0,max_grants=0,violations=0;
  for(unsigned cycle=0;cycle<200000;++cycle){
    unsigned long long before[4];for(unsigned b=0;b<4;++b)before[b]=sp->ft_bank_grants_for_test(b);
    part.cache_cycle(gpu.gpu_sim_cycle);part.simple_dram_model_cycle();
    for(unsigned b=0;b<4;++b){unsigned delta=sp->ft_bank_grants_for_test(b)-before[b];max_grants=std::max(max_grants,delta);if(delta>1)++violations;}
    ++gpu.gpu_sim_cycle;
    peak=std::max(peak,sp->ep_l2_descriptor_count_used());
    // One-entry ICNT queue stays full for 2000 cycles after real traffic.
    if(cycle>2000){while(auto r=sp->pop()) {assert(expected.count(r->get_request_uid()));assert(seen.insert(r->get_request_uid()).second);assert(r->has_ft_wb_data());delete r;}}
    else if(sp->ep_l2_descriptor_count_used() && sp->top()!=NULL)backpressure=true;
    if(cycle>2000&&!part.busy())break;
  }
  assert(violations==0);
  assert(seen==expected);assert(peak>0);assert(backpressure);assert(!part.busy());assert(sp->l2_char_no_resource_leak());
  sp->ro_end_epoch();
  if(ro_on) {
    assert(!sp->ft_try_force_l2_tag_update(0,gpu.gpu_sim_cycle,mem_access_sector_mask_t(1)));
    assert(sp->ft_memcpy_offset_for_test()==activated_offset);
  }
  sp->ro_cache_for_test()->ro_print(stdout,0);
  printf("RO_PHYSICAL_CLOCK|offset=%u|violations=%u|max_grants_per_physical_bank_cycle=%u\n",offset,violations,max_grants);
  printf("RO actual partition detach=%s bypass=%s exact-once=%zu peak_descriptors=%u backpressure=%u PASS\n",argv[5],argv[6],seen.size(),peak,backpressure);
}
