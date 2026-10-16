// Complete production memory_sub_partition -> L2interface -> lower -> set_done.
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

int main(int argc,char **argv) {
  setvbuf(stdout,NULL,_IONBF,0);
  assert(argc==6 || argc==7);unsigned mode=std::stoul(argv[3]), c=std::stoul(argv[4]);
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
  const unsigned offset=std::stoul(argv[5]);
  const std::string scenario=argc==7?argv[6]:"evicted";
  assert(scenario=="evicted" || scenario=="--warm-hit" || scenario=="--warm-multi-sector" ||
         scenario=="--reject-post-kernel" || scenario=="--reject-dirty-victim");
  const bool multi=scenario=="--warm-multi-sector";
  const bool warm_hit=scenario=="--warm-hit" || multi;
  for(unsigned i=0;i<offset;++i)
    sp->force_l2_tag_update(warm_hit ? 0 : 0x10000000,0,sectors);
  if(multi) {
    mem_access_sector_mask_t other;other.set(2);
    for(unsigned i=0;i<3;++i) sp->force_l2_tag_update(64,offset+i,other);
  }
  if(scenario=="--reject-post-kernel") {
    // No pending hardware remains; a new epoch alone must still reject reset
    // to initial-memory symbols. This is not an active-WB-only guard.
    gpu.gpu_tot_sim_cycle=1;
    sp->force_l2_tag_update(0,offset,sectors);
    return 77; // caller expects SIGABRT and the explicit scope guard message
  }
  std::map<unsigned,unsigned> warm_expected;
  if(warm_hit) {
    auto read=alloc.alloc(0,GLOBAL_ACC_R,active,bytes,sectors,32,false,0,0,0,0,NULL,0);
    warm_expected[read->get_request_uid()]=0;sp->push(read,0);
    if(multi) {
      mem_access_byte_mask_t b;for(unsigned i=64;i<96;++i)b.set(i);
      mem_access_sector_mask_t s;s.set(2);
      auto second=alloc.alloc(64,GLOBAL_ACC_R,active,b,s,32,false,0,0,0,0,NULL,0);
      warm_expected[second->get_request_uid()]=2;sp->push(second,0);
    }
  }
  auto collect=[&]() {
    while(auto reply=sp->pop()) {
      auto expected=warm_expected.find(reply->get_request_uid());
      if(expected!=warm_expected.end()) {
        assert(reply->has_ft_wb_data() && reply->get_ft_wb_data()[expected->second]==0);
        warm_expected.erase(expected);
      }
      delete reply;
    }
  };
  unsigned violations=0, max_grants=0;
  auto one_cycle = [&]() {
    unsigned long long before[4];
    for(unsigned b=0;b<4;++b) before[b]=sp->ft_bank_grants_for_test(b);
    part.cache_cycle(gpu.gpu_sim_cycle);part.simple_dram_model_cycle();
    for(unsigned b=0;b<4;++b) {
      unsigned delta=sp->ft_bank_grants_for_test(b)-before[b];
      max_grants=std::max(max_grants,delta);
      if(delta>1) {
        if(violations<8) printf("CLOCK_VIOLATION|cycle=%llu|bank=%u|grants=%u\n",gpu.gpu_sim_cycle,b,delta);
        ++violations;
      }
    }
    ++gpu.gpu_sim_cycle;
  };
  for(unsigned i=0;i<142;++i) {
    auto mf=alloc.alloc(i*128,i%2?L1_WRBK_ACC:GLOBAL_ACC_W,active,bytes,sectors,32,true,0,0,0,0,NULL,0);
    sp->push(mf,0);
  }
  unsigned peak=0;bool separate_lifetimes=false;
  for(unsigned cycle=0;cycle<2000;++cycle) {
    one_cycle();
    collect();
    peak=std::max(peak,sp->ft_wb_staged_for_test());
  }
  if(scenario=="--reject-dirty-victim") {
    // Real writes/WB pressure precede this attempted host overwrite. Never
    // erase dirty data or pending WAD/payload state to make prefill succeed.
    sp->force_l2_tag_update(0x20000000,offset,sectors);
    return 77;
  }
  if(mode==1) assert(peak==c && sp->ep_l2_wad_occupancy()>c);
  else assert(peak>c && sp->ep_l2_wad_occupancy()==128);
  part.l2_char_release_dram_issue_hold();
  for(unsigned i=0;i<200000 && part.busy();++i) {
    one_cycle();
    collect();
    if(!sp->ft_wb_staged_for_test() && sp->ep_l2_wad_occupancy()) separate_lifetimes=true;
  }
  assert(!part.busy() && sp->l2_char_no_resource_leak());
  assert(!sp->ep_l2_wad_occupancy() && sp->ft_drained_for_test());
  assert(separate_lifetimes && warm_expected.empty());
  sp->print_ep_l2_b0_snapshot(stdout,static_cast<unsigned long long>(-1));
  printf("CLOCK_AUDIT|offset=%u|violations=%u|max_grants_per_physical_bank_cycle=%u\n",offset,violations,max_grants);
  printf("FT real partition lower-full/WBUF/WAD/set_done mode=%u C=%u peak=%u PASS\n",mode,c,peak);
  return violations ? 42 : 0;
}
