// PREPARED_NOT_TESTED. Real component issue/cycle/fill and production collect/WB.
// No kernel, trace, GPU cycle(), simulator runtime thread, functional ISA or GPU.
// The external publication contract is a fixture assumption, not a modeled store.
#include "gpgpu-sim/gpu-sim.h"
#include "gpgpu-sim/shader.h"
#include "gpgpu-sim/icnt_wrapper.h"
#include "gpgpu-sim/scoreboard.h"
#include "gpgpu_context.h"
#include "gpgpusim_entrypoint.h"
#include "option_parser.h"
#include <array>
#include <ctime>
#include <dlfcn.h>
#include <iostream>
#include <string>
#include <sys/resource.h>
#include <vector>
#ifndef DTC_SEM1_CONDITIONAL_VALUES
#error SEM1 fixture requires a separate diagnostic ABI build
#endif
struct transport : mem_fetch_interface {
  std::vector<mem_fetch *> requests;
  bool full(unsigned,bool) const override { return false; }
  void push(mem_fetch *p) override { requests.push_back(p); }
};
static warp_inst_t *load(shader_core_ctx *core, unsigned reg, bool masked) {
  auto *p=new warp_inst_t(core->get_config());
  p->op=LOAD_OP; p->memory_op=memory_load; p->space=memory_space_t(global_space);
  p->cache_op=CACHE_L1; p->data_size=masked?2:4; p->pc=0x100+reg*16;
  p->op_pipe=MEM__OP; p->num_regs=1; p->outcount=1; p->incount=0;
  p->isize=16; p->pred=-1; p->ar1=-1; p->ar2=-1;
  for(auto &v:p->out)v=0; for(auto &v:p->in)v=0;
  p->out[0]=reg; p->arch_reg.dst[0]=reg;
  active_mask_t active; active.set(0);
  p->issue(active,0,core->get_gpu()->global_cycle(),0,0,0);
  mem_access_byte_mask_t bytes; mem_access_sector_mask_t sectors;
  if(masked) { bytes.set(31);bytes.set(32);sectors.set(0);sectors.set(1); }
  else { for(unsigned b=0;b<4;++b)bytes.set(b);sectors.set(0); }
  p->m_accessq.emplace_back(GLOBAL_ACC_R,0x2000,masked?64:32,false,active,bytes,sectors,
                             core->get_config()->gpgpu_ctx);
  return p;
}
int main(int argc,const char **argv) {
  if(argc!=3)return 64;
  rlimit zero{0,0};setrlimit(RLIMIT_CORE,&zero);
  const std::string mode=argv[2];
  const std::vector<std::string> allowed{"shared","masked","no-e","with-e","ae","ad",
       "wrong-byte","missing-sector","wrong-generation","early-release"};
  if(std::find(allowed.begin(),allowed.end(),mode)==allowed.end())return 64;
  Dl_info binding{};auto *symbol=dlsym(RTLD_DEFAULT,"_ZN9ldst_unit5cycleEv");
  assert(symbol && dladdr(symbol,&binding));
  std::cout<<"SEM1_LIBRARY "<<binding.dli_fname<<"\n";
  gpgpu_context ctx;gpgpu_sim_config cfg(&ctx);auto parser=option_parser_create();
  cfg.reg_options(parser);icnt_reg_options(parser);ctx.ptx_reg_options(parser);
  ctx.func_sim->ptx_opcocde_latency_options(parser);option_parser_cfgfile(parser,argv[1]);
  const char *opts[]={"SEM1", "-gpgpu_n_clusters","1","-gpgpu_n_cores_per_cluster","1",
    "-gpgpu_n_mem","1","-gpgpu_n_sub_partition_per_mchannel","1",
    "-gpgpu_shader_core_pipeline","64:32","-gpgpu_shader_cta","2",
    "-gpgpu_smem_latency","2","-network_mode","2","-gpgpu_dtc_l1_mode","3",
    "-gpgpu_dtc_l1_finite_service","1","-gpgpu_dtc_l1_logical_sets","64",
    "-gpgpu_dtc_l1_logical_ways","8","-gpgpu_l1_cache_write_ratio","0",
    "-gpgpu_gmem_skip_L1D","0","-gpgpu_memory_partition_indexing","0",
    "-enable_ptx_file_line_stats","0","-gpgpu_dtc_l1_debug_event_limit","0",
    "-power_simulation_enabled","0","-visualizer_enabled","0"};
  option_parser_cmdline(parser,sizeof(opts)/sizeof(opts[0]),opts);
  const char *extra[]={"SEM1_MODE","-gpgpu_dtc_l1_tag_hash_bankrow",mode=="ae"?"1":"0",
      "-gpgpu_dtc_l1_progressive_wakeup",mode=="ad"?"1":"0",
      "-gpgpu_dtc_l1_invalidate_on_source_event",mode=="with-e"?"1":"0"};
  option_parser_cmdline(parser,sizeof(extra)/sizeof(extra[0]),extra);cfg.init();
  ctx.the_gpgpusim->g_simulation_starttime=std::time(nullptr);
  auto *gpu=new exec_gpgpu_sim(cfg,&ctx);gpu->init();
  auto *core=gpu->get_simt_core_clusters()[0]->get_shader_cores()[0];
  auto *ld=core->m_ldst_unit;auto *rf=dynamic_cast<opndcoll_rfu_t *>(core->m_operand_collector);
  assert(rf);transport wire;ld->m_icnt=&wire;dtc_l1::sem1_values bridge;ld->m_sem1_values=&bridge;
  // Producer and independent reference are separate state; neither reference nor
  // expected bytes are accessible to Core hooks or physical collection reads.
  std::array<uint8_t,128> producer{},reference{};
  for(unsigned i=0;i<128;++i)producer[i]=static_cast<uint8_t>(i);
  for(unsigned i=0;i<128;++i)reference[i]=static_cast<uint8_t>(i);
  struct batch { std::vector<mem_fetch *> replies;unsigned returned=0;unsigned ready; };
  std::vector<batch> batches;size_t built=0;unsigned issued=0;
  std::vector<uint64_t> uids;std::map<uint64_t,std::array<uint8_t,128>> expected;
  unsigned count=mode=="ad"?10:3;bool masked=mode=="masked";
  for(unsigned cycle=1;cycle<=140;++cycle) {
    gpu->gpu_sim_cycle=cycle;rf->process_banks();
    const bool regular=mode=="ad"?(cycle<=37 && (cycle-1)%4==0):
                                             (cycle==1||cycle==5||cycle==11);
    const bool later=(mode=="no-e"||mode=="with-e")&&cycle==85;
    if(regular||later) {
      auto *p=load(core,issued+1,masked);assert(ld->m_dispatch_reg->empty());
      uids.push_back(p->get_uid());expected.emplace(p->get_uid(),reference);
      core->m_warp[0]->inc_inst_in_pipeline();core->m_scoreboard->reserveRegisters(p);
      register_set regs(1,"SEM1");regs.move_in(p);ld->issue(regs);++issued;
    }
    if(cycle==80&&(mode=="no-e"||mode=="with-e")) {
      // Explicit fixture publication only: there is NO executed store instruction.
      producer[0]=0x99;reference[0]=0x99;
      if(mode=="with-e")ld->invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER);
    }
    if(cycle==20&&mode=="early-release") {
      assert(!bridge.consumers.empty());auto b=bridge.consumers.begin()->second.references.at(0);
      assert(ld->m_dtc_l1_oo_frontend->ref_count({b.physical,b.generation})>0);
      ld->m_dtc_l1_oo_frontend->release({b.physical,b.generation}); // actual production assertion
      return 71;
    }
    while(built<wire.requests.size()) {
      auto *root=wire.requests[built++];batch b;b.ready=cycle<80?60:100;
      if(mode=="wrong-generation")
        ++ld->m_dtc_l1_oo_inflight.at(root->get_request_uid()).physical.generation;
      for(unsigned sector:{3U,1U,0U,2U}) {
        active_mask_t active;active.set(0);mem_access_byte_mask_t bm;mem_access_sector_mask_t sm;
        sm.set(sector);for(unsigned k=sector*32;k<(sector+1)*32;++k)bm.set(k);
        mem_access_t access(GLOBAL_ACC_R,root->get_addr()+sector*32,32,false,active,bm,sm,&ctx);
        auto *reply=new mem_fetch(access,static_cast<const warp_inst_t *>(nullptr),0,8,0,0,0,
            &cfg.m_memory_config,cycle,root);reply->set_reply();
        dtc_l1::sem1_values::bytes32 payload;
        for(unsigned k=0;k<32;++k)payload[k]=producer[sector*32+k];
        if(mode=="wrong-byte"&&sector==0)payload[0]^=1;
        bridge.inject_reply(reply->get_request_uid(),payload);b.replies.push_back(reply);
      }
      batches.push_back(std::move(b));
    }
    for(auto &b:batches) if(cycle>=b.ready&&b.returned<4) {
      if(mode=="missing-sector"&&b.returned==3)continue;
      ld->fill(b.replies[b.returned++]);
    }
    // Genuine RF bank conflict after collection; its successful retries must not
    // recollect or drop Ref. No fake writeback predicate is used.
    if(cycle>=62&&cycle<=70) { auto *block=load(core,1,false);assert(rf->writeback(*block));delete block; }
    ld->cycle();ld->m_dtc_l1_oo_frontend->assert_shadow_refs();
  }
  if(mode=="missing-sector") {
    assert(ld->m_dtc_l1_oo_frontend->active_refs()>0);
    std::cout<<"SEM1_REJECT incomplete_return_no_consumer_result\n";return 2;
  }
  assert(issued==count+((mode=="no-e"||mode=="with-e")?1:0));
  assert(bridge.consumers.size()==issued&&bridge.packets_empty());
  unsigned mismatches=0;bool stale_detected=false;
  for(const auto &kv:bridge.consumers) {
    const auto uid=kv.first;const auto &c=kv.second;
    assert(c.retire_cycle!=UINT64_MAX&&c.collected.size()==(masked?2:1));
    auto actual_byte=[&](unsigned byte) {
      for(const auto &s:c.collected)if(s.source.sector==byte/32)return s.bytes.at(byte%32);
      throw std::logic_error("requested byte lacks actual collected sector");
    };
    const auto &exp=expected.at(uid);
    const std::vector<unsigned> requested=masked?std::vector<unsigned>{31,32}:std::vector<unsigned>{0,1,2,3};
    for(unsigned byte:requested) {
      const unsigned got=actual_byte(byte),want=exp[byte];
      std::cout<<"SEM1_BYTE uid="<<uid<<" byte="<<byte<<" got="<<got<<" expected="<<want<<"\n";
      if(got!=want) {
        ++mismatches;
        if(mode=="no-e"&&uid==uids.back()&&byte==0&&got==0&&want==0x99)stale_detected=true;
      }
    }
  }
  dtc_l1::paper_frontend_stats stats;ld->get_dtc_l1_stats(stats);
  assert(stats.oo_active_refs==0&&stats.oo_inflight_current==0&&stats.oo_retire_count==issued);
  assert(stats.oo_ready_but_writeback_blocked_cycles>0);
  assert(!core->m_scoreboard->pendingWrites(0));
  if(mode=="no-e") {
    assert(stale_detected&&mismatches==1&&wire.requests.size()==1);
    std::cout<<"SEM1_CONDITIONAL_COUNTERMODEL external_publish_without_E cached_old_byte; store_execution=0\n";
  } else if(mismatches) return 1;
  std::cout<<"SEM1_RESULT mode="<<mode<<" actual_ldst=1 conditional_sector_values=1 kernels=0 GPU_cycle_calls=0"
           <<" accepted_packets="<<bridge.accepted_packets<<" consumers="<<issued
           <<" architectural_values=UNPROVEN native=UNPROVEN\n";
  return 0;
}
