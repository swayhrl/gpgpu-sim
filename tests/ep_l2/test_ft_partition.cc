// Complete production memory_sub_partition -> L2interface -> lower -> set_done.
#include <cassert>
#include <cstdio>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

int main(int argc,char **argv) {
  assert(argc==5);unsigned mode=std::stoul(argv[3]), c=std::stoul(argv[4]);
  gpgpu_context ctx;gpgpu_sim_config cfg(&ctx);option_parser_t p=option_parser_create();
  ctx.ptx_reg_options(p);ctx.func_sim->ptx_opcocde_latency_options(p);icnt_reg_options(p);cfg.reg_options(p);
  option_parser_cfgfile(p,argv[1]);option_parser_cfgfile(p,argv[2]);
  const char *args[]={"ft-partition","-gpgpu_ep_l2_ft_mode",argv[3],"-gpgpu_ep_l2_wbuf_entries",argv[4],
    "-gpgpu_dram_partition_queues","512:2:64:64", "-gpgpu_l2_char_dram_issue_hold_after_issues","0",
    "-gpgpu_l2_char_dram_issue_hold_cycles","100000"};
  option_parser_cmdline(p,11,args);cfg.init();option_parser_destroy(p);
  exec_gpgpu_sim gpu(cfg,&ctx);const memory_config *mc=gpu.getMemoryConfig();
  memory_stats_t stats(cfg.num_shader(),gpu.getShaderCoreConfig(),mc,&gpu);
  memory_partition_unit part(0,mc,&stats,&gpu);auto sp=part.get_sub_partition(0);partition_mf_allocator alloc(mc);
  active_mask_t active;active.set(0);mem_access_byte_mask_t bytes;
  for(unsigned i=0;i<32;++i) bytes.set(i);
  mem_access_sector_mask_t sectors;sectors.set(0);
  for(unsigned i=0;i<142;++i) {
    auto mf=alloc.alloc(i*128,i%2?L1_WRBK_ACC:GLOBAL_ACC_W,active,bytes,sectors,32,true,0,0,0,0,NULL,0);
    sp->push(mf,0);
  }
  unsigned peak=0;bool separate_lifetimes=false;
  for(unsigned cycle=0;cycle<2000;++cycle) {
    part.cache_cycle(gpu.gpu_sim_cycle);part.simple_dram_model_cycle();++gpu.gpu_sim_cycle;
    while(auto reply=sp->pop()) delete reply;
    peak=std::max(peak,sp->ft_wb_staged_for_test());
  }
  if(mode==1) assert(peak==c && sp->ep_l2_wad_occupancy()>c);
  else assert(peak>c && sp->ep_l2_wad_occupancy()==128);
  part.l2_char_release_dram_issue_hold();
  for(unsigned i=0;i<200000 && part.busy();++i) {
    part.cache_cycle(gpu.gpu_sim_cycle);part.simple_dram_model_cycle();++gpu.gpu_sim_cycle;
    while(auto reply=sp->pop()) delete reply;
    if(!sp->ft_wb_staged_for_test() && sp->ep_l2_wad_occupancy()) separate_lifetimes=true;
  }
  assert(!part.busy() && sp->l2_char_no_resource_leak());
  assert(!sp->ep_l2_wad_occupancy() && sp->ft_drained_for_test());
  assert(separate_lifetimes);
  printf("FT real partition lower-full/WBUF/WAD/set_done mode=%u C=%u peak=%u PASS\n",mode,c,peak);
}
