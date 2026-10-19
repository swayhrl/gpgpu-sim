// PREPARED_NOT_TESTED: source-only follow-up to 1fcf5113. No runtime authority.
// Actual LDST issue/fill/cycle, OO notifier, physical bytes, collector and WB.
// Synthetic pre-coalesced requests and externally supplied reply bytes only.
// No functional ISA, modeled stores, GPU::cycle, trace, kernel or coherence.
#include "gpgpu-sim/gpu-sim.h"
#include "gpgpu-sim/shader.h"
#include "gpgpu-sim/icnt_wrapper.h"
#include "gpgpu-sim/scoreboard.h"
#include "gpgpu_context.h"
#include "gpgpusim_entrypoint.h"
#include "option_parser.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <ctime>
#include <dlfcn.h>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <vector>
#if !defined(DTC_SEM1_CONDITIONAL_VALUES) || defined(NDEBUG)
#error This diagnostic requires its fresh matching ABI and enabled assertions
#endif

using line_bytes = std::array<uint8_t,128>;
using identity = dtc_l1::physical_identity;
static constexpr uint64_t A = 0x2000, B = A + 64 * 128;
static line_bytes pattern(unsigned n) {
  line_bytes out{};
  for (unsigned i=0;i<128;++i)
    out[i]=n==0?i:(n==1?255-i:static_cast<uint8_t>(i+17*n));
  return out;
}
struct wire_t : mem_fetch_interface {
  std::vector<mem_fetch *> requests;
  bool full(unsigned,bool) const override { return false; }
  void push(mem_fetch *p) override { requests.push_back(p); }
};
static warp_inst_t *make_load(shader_core_ctx *core,unsigned reg,uint64_t line,
                              const std::vector<unsigned> &requested) {
  assert(line%128==0 && !requested.empty());
  auto *p=new warp_inst_t(core->get_config());
  p->op=LOAD_OP;p->memory_op=memory_load;p->space=memory_space_t(global_space);
  p->cache_op=CACHE_L1;p->data_size=requested.size();p->pc=0x100+reg*16;
  p->op_pipe=MEM__OP;p->num_regs=1;p->outcount=1;p->incount=0;p->isize=16;
  p->pred=-1;p->ar1=-1;p->ar2=-1;
  for(auto &v:p->out)v=0;for(auto &v:p->in)v=0;
  p->out[0]=reg;p->arch_reg.dst[0]=reg;
  active_mask_t active;active.set(0);
  p->issue(active,0,core->get_gpu()->global_cycle(),0,0,0);
  mem_access_byte_mask_t bm;mem_access_sector_mask_t sm;
  unsigned lo=128,hi=0;
  for(unsigned byte:requested) {
    assert(byte<128);bm.set(byte);sm.set(byte/32);
    lo=std::min(lo,byte);hi=std::max(hi,byte);
  }
  const unsigned first=lo/32,last=hi/32;
  p->m_accessq.emplace_back(GLOBAL_ACC_R,line+first*32,(last-first+1)*32,
      false,active,bm,sm,core->get_config()->gpgpu_ctx);
  return p;
}
struct request_t {
  uint64_t line;
  line_bytes expected;
  std::vector<unsigned> mask;
};
struct slot_t { unsigned slot;uint64_t generation; };
struct sm_t {
  shader_core_ctx *core;
  ldst_unit *ld;
  opndcoll_rfu_t *rf;
  wire_t wire;
  dtc_l1::sem1_values bridge;
  std::map<uint64_t,request_t> requests;
  std::map<uint64_t,slot_t> slots;
  std::vector<uint64_t> uids;
  size_t built=0;
  struct batch_t {
    std::vector<mem_fetch *> replies;
    unsigned returned=0,ready=0;
  };
  std::vector<batch_t> batches;
  explicit sm_t(shader_core_ctx *c):core(c),ld(c->m_ldst_unit),
      rf(dynamic_cast<opndcoll_rfu_t *>(c->m_operand_collector)) {
    assert(rf);ld->m_icnt=&wire;ld->m_sem1_values=&bridge;
  }
  dtc_l1::oo_frontend &front() { return *ld->m_dtc_l1_oo_frontend; }
  uint64_t issue(uint64_t line,const line_bytes &expected,
                 std::vector<unsigned> mask={0,1,2,3}) {
    assert(ld->m_dispatch_reg->empty());
    auto *p=make_load(core,uids.size()+1,line,mask);const auto uid=p->get_uid();
    requests.emplace(uid,request_t{line,expected,mask});uids.push_back(uid);
    core->m_warp[0]->inc_inst_in_pipeline();core->m_scoreboard->reserveRegisters(p);
    register_set regs(1,"SEM1_LITMUS12");regs.move_in(p);ld->issue(regs);
    return uid;
  }
  identity physical(uint64_t uid) const {
    const auto &r=bridge.consumers.at(uid).references;
    assert(!r.empty());return {r.front().physical,r.front().generation};
  }
  // Producer state alone feeds replies; reference snapshots are never read here.
  unsigned build(unsigned cycle,gpgpu_context &ctx,const gpgpu_sim_config &cfg,
                 const std::map<uint64_t,line_bytes> &producer,unsigned ready,
                 unsigned order,std::string fault="",bool swap=false) {
    unsigned made=0;
    static const unsigned orders[4][4]={{3,1,0,2},{0,2,1,3},{2,0,3,1},{1,3,2,0}};
    while(built<wire.requests.size()) {
      auto *root=wire.requests[built++];batch_t batch;batch.ready=ready;
      const uint64_t line=root->get_addr()&~uint64_t(127);
      auto source=producer.at(line);
      if(swap && line==A)source=producer.at(B);
      if(swap && line==B)source=producer.at(A);
      if(fault=="wrong-generation") {
        std::cout<<"SEM1_L12_FAULT wrong-generation root="<<root->get_request_uid()<<std::endl;
        ++ld->m_dtc_l1_oo_inflight.at(root->get_request_uid()).physical.generation;
      }
      for(unsigned index=0;index<4;++index) {
        const unsigned sector=orders[order%4][index];
        active_mask_t active;active.set(0);mem_access_byte_mask_t bm;mem_access_sector_mask_t sm;
        sm.set(sector);for(unsigned k=sector*32;k<(sector+1)*32;++k)bm.set(k);
        mem_access_t access(GLOBAL_ACC_R,line+sector*32,32,false,active,bm,sm,&ctx);
        auto *reply=new mem_fetch(access,static_cast<const warp_inst_t *>(nullptr),0,8,
            0,core->get_sid(),0,&cfg.m_memory_config,cycle,root);reply->set_reply();
        dtc_l1::sem1_values::bytes32 payload{};
        for(unsigned k=0;k<32;++k)payload[k]=source[sector*32+k];
        if(fault=="bad-byte" && line==A && sector==0)payload[0]^=1;
        bridge.inject_reply(reply->get_request_uid(),payload);batch.replies.push_back(reply);
      }
      batches.push_back(std::move(batch));++made;
    }
    return made;
  }
  unsigned fill(unsigned cycle,bool missing=false) {
    // At most one offered reply per SM per component tick, even with many roots.
    for(auto &batch:batches)if(cycle>=batch.ready && batch.returned<4) {
      if(missing && batch.returned==3)continue;
      ld->fill(batch.replies[batch.returned++]);return 1;
    }
    return 0;
  }
  void bank_block() {
    auto *p=make_load(core,1,A,{0,1,2,3});assert(rf->writeback(*p));delete p;
  }
  void cycle() {
    ld->cycle();front().assert_shadow_refs();
    for(const auto &kv:bridge.consumers)if(!slots.count(kv.first)) {
      assert(kv.second.retire_cycle==UINT64_MAX);
      slots.emplace(kv.first,slot_t{front().entry_slot(kv.first),
                                  front().entry_slot_generation(kv.first)});
      const auto p=physical(kv.first);const auto slot=slots.at(kv.first);
      std::cout<<"SEM1_L12_BIND sm="<<core->get_sid()<<" uid="<<kv.first
        <<" line="<<requests.at(kv.first).line<<" physical="<<p.id
        <<" generation="<<p.generation<<" slot="<<slot.slot
        <<" slot_generation="<<slot.generation<<"\n";
    }
  }
  unsigned compare(bool drain=true) {
    unsigned mismatches=0;
    for(const auto &kv:bridge.consumers) {
      const auto uid=kv.first;const auto &consumer=kv.second;
      assert(consumer.retire_cycle!=UINT64_MAX);
      const auto &request=requests.at(uid);
      for(const auto &sample:consumer.collected)
        std::cout<<"SEM1_L12_COLLECT sm="<<core->get_sid()<<" uid="<<uid
          <<" physical="<<sample.source.physical<<" generation="<<sample.source.generation
          <<" sector="<<sample.source.sector<<" cycle="<<sample.cycle
          <<" retire_cycle="<<consumer.retire_cycle<<"\n";
      for(unsigned byte:request.mask) {
        bool found=false;unsigned got=0;
        for(const auto &sample:consumer.collected)if(sample.source.sector==byte/32) {
          assert(!found);found=true;got=sample.bytes.at(byte%32);
        }
        assert(found);
        std::cout<<"SEM1_L12_BYTE sm="<<core->get_sid()<<" uid="<<uid<<" byte="<<byte
          <<" got="<<got<<" expected="<<unsigned(request.expected[byte])<<"\n";
        mismatches+=got!=request.expected[byte];
      }
    }
    if(drain) {
      assert(bridge.consumers.size()==requests.size() && bridge.packets_empty());
      dtc_l1::paper_frontend_stats stats;ld->get_dtc_l1_stats(stats);
      assert(!stats.oo_active_refs && !stats.oo_inflight_current);
      assert(stats.oo_retire_count==requests.size());
      assert(!core->m_scoreboard->pendingWrites(0));
    }
    return mismatches;
  }
};
struct fixture_t {
  gpgpu_context ctx;
  gpgpu_sim_config cfg;
  exec_gpgpu_sim *gpu;
  std::vector<std::unique_ptr<sm_t>> sms;
  fixture_t(const char *config,unsigned cores,bool ad,bool source_e,unsigned ae=0):cfg(&ctx) {
    auto parser=option_parser_create();cfg.reg_options(parser);icnt_reg_options(parser);
    ctx.ptx_reg_options(parser);ctx.func_sim->ptx_opcocde_latency_options(parser);
    option_parser_cfgfile(parser,config);
    const char *opts[]={"SEM1_LITMUS12","-gpgpu_n_clusters","1",
      "-gpgpu_n_cores_per_cluster",cores==2?"2":"1","-gpgpu_n_mem","1",
      "-gpgpu_n_sub_partition_per_mchannel","1","-gpgpu_shader_core_pipeline","64:32",
      "-gpgpu_shader_cta","2","-gpgpu_smem_latency","2","-network_mode","2",
      "-gpgpu_dtc_l1_mode","3","-gpgpu_dtc_l1_finite_service","1",
      "-gpgpu_dtc_l1_logical_sets","64","-gpgpu_dtc_l1_logical_ways","8",
      "-gpgpu_dtc_l1_physical_lines","640","-gpgpu_dtc_l1_oo_pib_entries","128",
      "-gpgpu_l1_cache_write_ratio","0","-gpgpu_gmem_skip_L1D","0",
      "-gpgpu_memory_partition_indexing","0","-enable_ptx_file_line_stats","0",
      "-gpgpu_dtc_l1_debug_event_limit","0","-power_simulation_enabled","0",
      "-visualizer_enabled","0","-gpgpu_dtc_l1_tag_hash_bankrow",ae?"1":"0",
      "-gpgpu_dtc_l1_progressive_wakeup",ad?"1":"0",
      "-gpgpu_dtc_l1_invalidate_on_source_event",source_e?"1":"0"};
    option_parser_cmdline(parser,sizeof(opts)/sizeof(opts[0]),opts);cfg.init();
    ctx.the_gpgpusim->g_simulation_starttime=std::time(nullptr);
    gpu=new exec_gpgpu_sim(cfg,&ctx);gpu->init();
    for(unsigned i=0;i<cores;++i)
      sms.emplace_back(new sm_t(gpu->get_simt_core_clusters()[0]->get_shader_cores()[i]));
    // Exact live component geometry is checked; no reduced-pressure geometry.
    for(auto &sm:sms) {
      const auto &c=sm->front().m_cfg;
      assert(c.logical_sets==64 && c.logical_ways==8 && c.physical_lines==640);
      assert(c.oo_pib_entries==128 && c.allocation_width==4);
      assert(c.tag_banks==4 && c.tag_requests_per_bank_per_cycle==1);
    }
  }
  void begin(unsigned cycle) {
    gpu->gpu_sim_cycle=cycle;
    for(auto &sm:sms)sm->rf->process_banks();
  }
  void end() { for(auto &sm:sms)sm->cycle(); }
  // One finite scenario per process. No model thread or GPU cycle is started.
};
static void result(const char *id,const std::string &detail) {
  std::cout<<"SEM1_L12_RESULT id="<<id<<" "<<detail
    <<" actual_ldst=1 conditional_reply_bytes=1 kernels=0 GPU_cycle_calls=0"
    <<" architectural_values=UNPROVEN native=UNPROVEN coherence=UNPROVEN\n";
}
static int tag_eviction(const char *config,bool swap) {
  fixture_t f(config,1,false,false);auto &sm=*f.sms[0];
  std::map<uint64_t,line_bytes> producer;
  for(unsigned n=0;n<9;++n)producer.emplace(A+n*64*128,pattern(n));
  bool witnessed=false;uint64_t first=0;
  for(unsigned tick=1;tick<=190;++tick) {
    f.begin(tick);
    if(tick<=33 && (tick-1)%4==0) {
      unsigned n=(tick-1)/4;
      auto uid=sm.issue(A+n*64*128,pattern(n));if(!n)first=uid;
    }
    sm.build(tick,f.ctx,f.cfg,producer,90,0,"",swap);sm.fill(tick);
    f.end();
    if(!witnessed && sm.bridge.consumers.count(first) && sm.bridge.consumers.size()==9 && tick<90) {
      const auto id=sm.physical(first);
      assert(sm.front().is_allocated(id) && !sm.front().tag_visible(id));
      assert(sm.front().ref_count(id)>0 && sm.front().deferred_reclaims()>0);
      assert(sm.bridge.consumers.at(first).collected.empty());witnessed=true;
      std::cout<<"SEM1_L12_EVICT_LIVE uid="<<first<<" physical="<<id.id
        <<" generation="<<id.generation<<" refs="<<sm.front().ref_count(id)
        <<" cycle="<<tick<<"\n";
    }
  }
  assert(witnessed && sm.wire.requests.size()==9 && sm.front().tag_evictions()>=1);
  const auto mismatches=sm.compare();
  if(swap) { assert(mismatches>0);std::cout<<"SEM1_L12_REJECT A_B_PAYLOAD_SWAP\n";return 1; }
  assert(!mismatches);result("S02","tag_evicted_while_ref_live=1 old_A_preserved=1");return 0;
}
static int late_reuse(const char *config,bool stale_waiter) {
  // S08 alone uses 32 initial waiters, not S12's at-most-ten-consumer domain.
  // Eight finite notify ticks leave room for actual collect+1/WB+slot reuse.
  fixture_t f(config,1,true,false);auto &sm=*f.sms[0];
  std::map<uint64_t,line_bytes> producer{{A,pattern(0)},{B,line_bytes{}}};
  producer.at(B).fill(0x55);
  line_bytes reference_b{};reference_b.fill(0x55);
  uint64_t first=0,late=0,new_b=0;bool late_live=false,reused=false;
  for(unsigned tick=1;tick<=250;++tick) {
    f.begin(tick);
    if(tick<=125 && (tick-1)%4==0) {
      const auto uid=sm.issue(A,pattern(0));if(!first)first=uid;
    }
    if(first && sm.bridge.consumers.count(first) && tick>125) {
      const auto id=sm.physical(first);const auto &p=sm.front().m_phys.at(id.id);
      const bool broadcasting=p.data_ready && !p.ready && !p.waiters.empty();
      if(broadcasting && !late && sm.ld->m_dispatch_reg->empty())late=sm.issue(A,pattern(0));
      else if(broadcasting && late && !new_b &&
              sm.bridge.consumers.at(first).retire_cycle!=UINT64_MAX &&
              sm.ld->m_dispatch_reg->empty())new_b=sm.issue(B,reference_b);
    }
    sm.build(tick,f.ctx,f.cfg,producer,tick<170?150:190,0);sm.fill(tick);
    f.end();
    if(late && sm.slots.count(late) && !late_live) {
      const auto id=sm.physical(late);const auto &p=sm.front().m_phys.at(id.id);
      assert(id.id==sm.physical(first).id && id.generation==sm.physical(first).generation);
      assert(p.late_merges>0 && p.data_ready && !p.ready);late_live=true;
      std::cout<<"SEM1_L12_LATE_MERGE uid="<<late<<" physical="<<id.id
               <<" generation="<<id.generation<<" cycle="<<tick<<"\n";
    }
    if(new_b && sm.slots.count(new_b) && !reused) {
      const auto old_slot=sm.slots.at(first),new_slot=sm.slots.at(new_b);
      assert(old_slot.slot==new_slot.slot && new_slot.generation>old_slot.generation);
      const auto a=sm.physical(first),b=sm.physical(new_b);
      auto &p=sm.front().m_phys.at(a.id);
      assert(a.id!=b.id && p.data_ready && !p.ready && !p.waiters.empty());
      reused=true;
      std::cout<<"SEM1_L12_SLOT_REUSE old_uid="<<first<<" new_uid="<<new_b
        <<" slot="<<new_slot.slot<<" old_slot_generation="<<old_slot.generation
        <<" new_slot_generation="<<new_slot.generation<<" cycle="<<tick<<"\n";
      if(stale_waiter) {
        // Deliberate corruption of an actual still-unnotified A waiter. The
        // next production advance_service must reject B's missing A reference.
        std::cout<<"SEM1_L12_FAULT stale-waiter A="<<a.id<<" B="<<b.id
                 <<" slot="<<new_slot.slot<<std::endl;
        p.waiters.front().slot=new_slot.slot;
        p.waiters.front().slot_generation=new_slot.generation;
      }
    }
  }
  if(stale_waiter)return 71; // failure if the real notifier did not abort
  assert(late_live && reused && sm.wire.requests.size()==2 && sm.uids.size()==34);
  assert(!sm.compare());result("S08","late_merge=1 prior_retire=1 PIB_slot_reuse=1");return 0;
}
static int cross_sm(const char *config) {
  fixture_t f(config,2,false,true);
  std::map<uint64_t,line_bytes> producer{{A,pattern(0)}};
  line_bytes reference_new=pattern(0);
  uint64_t old1=0,new0=0,new1=0;identity cached1{};
  for(unsigned tick=1;tick<=110;++tick) {
    f.begin(tick);
    if(tick==1)for(auto &sm:f.sms) {
      const auto uid=sm->issue(A,pattern(0));if(sm->core->get_sid()==1)old1=uid;
    }
    if(tick==60) {
      cached1=f.sms[1]->physical(old1);
      assert(f.sms[1]->front().tag_visible(cached1));
      producer.at(A)[0]=0x99; // explicit external publication, not a store
      reference_new[0]=0x99;
      f.sms[0]->ld->invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER);
      assert(f.sms[1]->front().tag_visible(cached1));
    }
    if(tick==65) {
      new0=f.sms[0]->issue(A,reference_new);
      new1=f.sms[1]->issue(A,pattern(0)); // routing expectation, not freshness
    }
    for(auto &sm:f.sms) {
      sm->build(tick,f.ctx,f.cfg,producer,tick<60?20:80,0);sm->fill(tick);
    }
    f.end();
  }
  assert(!f.sms[0]->compare() && !f.sms[1]->compare());
  assert(f.sms[0]->wire.requests.size()==2 && f.sms[1]->wire.requests.size()==1);
  const auto retained=f.sms[1]->physical(new1);
  assert(retained.id==cached1.id && retained.generation==cached1.generation);
  assert(f.sms[0]->physical(new0).generation>0);
  result("S11","SM0_selected_E_new_byte=153 SM1_retained_byte=0 SM1_freshness=UNRESOLVED");return 0;
}
static uint32_t next_random(uint32_t &x) { x^=x<<13;x^=x>>17;x^=x<<5;return x; }
static int schedules(const char *config,unsigned index) {
  assert(index<64);uint32_t rng=0x53454d31U;
  // All schedules are fixed from the one seed, independent of wall clock/UID.
  for(unsigned k=0;k<=index;++k)next_random(rng);
  const unsigned order=next_random(rng)%4;
  const bool ad=next_random(rng)&1,ae=next_random(rng)&1;
  const bool countermodel=index==58;
  const bool publication=(next_random(rng)&1)||countermodel||index==63;
  const bool source_e=publication && !countermodel && index!=63;
  const std::string fault=index==59?"bad-byte":index==60?"missing-sector":
                          index==61?"wrong-generation":index==62?"early-release":
                          index==63?"false-publication":"";
  std::cout<<"SEM1_L12_PLAN index="<<index<<" seed=0x53454d31 AE="<<ae<<" AD="<<ad
    <<" publication="<<publication<<" selected_E="<<source_e
    <<" fault="<<(fault.empty()?"none":fault)<<std::endl;
  fixture_t f(config,1,ad,source_e,ae);auto &sm=*f.sms[0];
  std::map<uint64_t,line_bytes> producer{{A,pattern(0)},{B,pattern(1)}};
  line_bytes reference_a=pattern(0),reference_b=pattern(1);
  unsigned events=0;bool blocked=false;uint64_t later_a=0;
  auto event=[&]() { assert(++events<=128); };
  // Worst case: 96 ticks + 10 issues + 16 sector offers + publication + E
  // + four real RF conflicts = 128 counted fixture events. This is a bound,
  // not a claim to cover all 128-event interleavings or all internal events.
  for(unsigned tick=1;tick<=96;++tick) {
    event();f.begin(tick); // each passive component tick counts as one event
    if(tick<=29 && (tick-1)%4==0) {
      event();const unsigned n=(tick-1)/4;
      std::vector<unsigned> mask{0,1,2,3};
      if(n && next_random(rng)%3==1)mask={31,32};
      else if(n && next_random(rng)%3==2)mask={64,65,66,67,68,69,70,71};
      sm.issue(n%2?B:A,n%2?reference_b:reference_a,mask);
    }
    if(tick==70 && publication) {
      event();producer.at(A)[0]=0x99;reference_a[0]=0x99;
      if(source_e) { event();sm.ld->invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER); }
    }
    if(tick==75 || tick==79) {
      event();const auto uid=sm.issue(tick==75?A:B,tick==75?reference_a:reference_b);
      if(tick==75)later_a=uid;
    }
    if(tick==20 && fault=="early-release") {
      event();const auto id=sm.physical(sm.uids.front());assert(sm.front().ref_count(id)>0);
      std::cout<<"SEM1_L12_FAULT early-release physical="<<id.id<<std::endl;
      sm.front().release(id);return 71;
    }
    sm.build(tick,f.ctx,f.cfg,producer,tick<70?50:85,order,
             fault=="bad-byte"||fault=="wrong-generation"?fault:"");
    if(sm.fill(tick,fault=="missing-sector"))event();
    if(tick>=55 && tick<=58) { event();sm.bank_block(); }
    f.end();
  }
  assert(sm.uids.size()==10);
  if(fault=="missing-sector") {
    assert(sm.front().active_refs()>0);
    for(const auto &kv:sm.bridge.consumers)assert(kv.second.retire_cycle==UINT64_MAX);
    std::cout<<"SEM1_L12_REJECT missing-sector no_consumer_result events="<<events<<"\n";return 2;
  }
  const unsigned mismatches=sm.compare();
  dtc_l1::paper_frontend_stats stats;sm.ld->get_dtc_l1_stats(stats);
  blocked=stats.oo_ready_but_writeback_blocked_cycles>0;assert(blocked);
  if(fault=="bad-byte" || fault=="false-publication") {
    assert(mismatches>0);std::cout<<"SEM1_L12_REJECT "<<fault<<"\n";return 1;
  }
  if(fault=="wrong-generation"||fault=="early-release")return 71;
  if(countermodel) {
    assert(mismatches==1);
    const auto &samples=sm.bridge.consumers.at(later_a).collected;
    assert(!samples.empty() && samples.front().bytes[0]==0);
    std::cout<<"SEM1_L12_COUNTERMODEL external_publish_without_E no_store_execution\n";
  } else assert(!mismatches);
  result("S12","index="+std::to_string(index)+" seed=0x53454d31 events="+
      std::to_string(events)+" consumers=10 countermodel="+std::to_string(countermodel));return 0;
}
int main(int argc,const char **argv) {
  if(argc<3 || argc>4)return 64;
  rlimit zero{0,0};if(setrlimit(RLIMIT_CORE,&zero))return 70;
  Dl_info binding{};auto *symbol=dlsym(RTLD_DEFAULT,"_ZN9ldst_unit5cycleEv");
  assert(symbol && dladdr(symbol,&binding));
  std::cout<<"SEM1_L12_LIBRARY "<<binding.dli_fname<<std::endl;
  const std::string mode=argv[2];
  if(argc==3 && mode=="S02")return tag_eviction(argv[1],false);
  if(argc==3 && mode=="S02-swap")return tag_eviction(argv[1],true);
  if(argc==3 && mode=="S08")return late_reuse(argv[1],false);
  if(argc==3 && mode=="S08-stale-waiter")return late_reuse(argv[1],true);
  if(argc==3 && mode=="S11")return cross_sm(argv[1]);
  if(argc==4 && mode=="S12") {
    const std::string n=argv[3];if(n.empty()||n.size()>2)return 64;
    unsigned index=0;for(char c:n) { if(c<'0'||c>'9')return 64;index=index*10+c-'0'; }
    if(index>=64)return 64;return schedules(argv[1],index);
  }
  return 64;
}
