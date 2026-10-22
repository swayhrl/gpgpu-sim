// Bounded production-path observer controls. No complete application workload.
#include "potential_fixture.h"
#include <cstring>
#include <limits>
static uint64_t witness=1469598103934665603ULL;
static void mix(uint64_t n){witness^=n;witness*=1099511628211ULL;}
static void diag_test_witness(unsigned kind,unsigned long long cycle,mem_fetch*m){mix(kind);mix(cycle);mix(m->get_request_uid());mix(m->get_addr());mix(m->get_access_type());mix(m->get_access_sector_mask().to_ulong());mix(m->get_ep_l2_payload_id());mix(m->get_ep_l2_payload_generation());if(m->has_ft_wb_data())for(auto v:m->get_ft_wb_data())mix(v);}
#define DIAG_TEST_WITNESS_ACTIVE 1
#include "ms_ro_combined/native_fixture.h"
#ifndef EP_L2_DIAG_REFERENCE
#include "../../src/gpgpu-sim/ep-l2-detach-diag.h"
#endif
static const uint64_t stride=8192;
static void fill(combined_fixture&f,unsigned n,bool ro=true,unsigned set=0){for(unsigned i=0;i<n;++i){f.send(f.make((ro?0:0x800000)+((ro&&n==16&&set==0)?(i+1)%16:i)*stride+set*128,false));f.await_lower(i+1);}}
static void tick(combined_fixture&f,unsigned n=16,bool consume=true){while(n--)f.step(consume);}
template<class F> static void fold(F&f){
 for(unsigned i=0;i<1024;++i){auto h=f.cache->ep_l2_tag_payload_handle_for_test(i);mix(i);mix(h.payload_id);mix(h.generation);if(f.cache->ep_l2_payload_for_test().handle_live(h)){const auto&s=f.cache->ep_l2_payload_for_test().inspect(h);mix(s.owner);mix(s.pins);mix(s.ready_sector_mask.to_ulong());mix(s.pending_sector_mask.to_ulong());for(auto v:s.versions)mix(v);}}
 mix(f.gpu->gpu_sim_cycle+f.gpu->gpu_tot_sim_cycle);mix(f.cache->ep_l2_payload_bank_grants());mix(f.cache->ep_l2_payload_bank_conflicts());mix(f.cache->ep_l2_payload_bank_attempts());mix(f.cache->ft_identity_redirects_for_test());mix(f.cache->ft_last_redirect_from_for_test());
}
static void signature(combined_fixture&f){fold(f);
 printf("EPL2MS_TEST|SEMANTIC|cycles=%llu|digest=%llu|replies=%zu|lower=%llu|bytes=%llu|bank_grants=%llu|bank_conflicts=%llu|bank_attempts=%llu|wad=%u|descriptors=%u|drain=%u\n",f.gpu->gpu_sim_cycle+f.gpu->gpu_tot_sim_cycle,(unsigned long long)witness,f.replies.size(),f.lower_read_packets,f.lower_read_bytes,f.cache->ep_l2_payload_bank_grants(),f.cache->ep_l2_payload_bank_conflicts(),f.cache->ep_l2_payload_bank_attempts(),f.cache->ep_l2_wad_occupancy(),f.cache->ep_l2_descriptor_count_used(),unsigned(f.cache->ft_drained()));
}
#ifndef EP_L2_DIAG_REFERENCE
static ep_l2_detach_diag::ledger* ledger(combined_fixture&f){return ep_l2_detach_diag::get(f.cache);}
static void require_observer(combined_fixture&f,ep_l2_detach_diag::field key,uint64_t min){if(auto*d=ledger(f)){assert(d->total[key]>=min);assert(d->partitions());}}
#endif
static int held_redirect_original(int argc,char **argv) {
  assert(argc==8 || argc==9);
  const bool rollback=argc==9 && std::string(argv[8])=="rollback";
  const unsigned extra=argc==9 && !rollback?15:0;fixture f(argv);
  unsigned mode=std::stoul(argv[3]),c=std::stoul(argv[4]);
  const new_addr_type stride=64*128, victim=13*stride, incoming=16*stride, other=128;
  // Invalid-way selection is descending: read0->way15, dirty13->way2;
  // all other fifteen ways retain real lower read sectors RESERVED.
  for(unsigned i=0;i<16;++i) {
    auto r=f.make(i*stride,i==13);
    if(i==13) f.memory[victim]=f.expected.at(r->get_request_uid());
    f.send(r);f.until_frontend_empty();
  }
  assert(f.cache->ft_reserved_ways_for_test(0)==15 && f.reads.size()==15);
  auto sector_write=f.make(victim+64,true);
  f.memory[victim][2]=(unsigned long long)sector_write->get_request_uid()+1;
  f.send(sector_write);f.until_frontend_empty();
  auto held=f.cache->ep_l2_tag_payload_handle_for_test(2);
  assert(f.cache->ep_l2_payload_for_test().inspect(held).versions==f.memory.at(victim));
  // For U_WB, a real ready response from another set (also bank2) consumes
  // the port before frontend admission, creating its cross-cycle retry.
  if(mode==2) {
    for(new_addr_type a:{other,other+stride}) {
      auto r=f.make(a,false);f.send(r);f.until_frontend_empty();
    }
  }
  // Fill the actual two-entry lower FIFO with reads from other sets. Its
  // backpressure must retain the WB payload until a real lower accept.
  f.block_lower=true;
  for(new_addr_type addr:{256ull,384ull}) {
    auto r=f.make(addr,false);f.send(r);f.until_frontend_empty();
  }
  assert(f.sp->ep_l2_l2dram_occupancy()==2);
  if(extra) {
    assert(mode==2 && std::stoul(argv[6])==16);
    for(unsigned i=0;i<31;++i) {
      auto w=f.make(512+i*stride,true);f.memory[512+i*stride]=f.expected.at(w->get_request_uid());
      f.send(w);f.until_frontend_empty();
    }
    assert(f.cache->ft_wb_quota_live()==15 && !f.cache->ft_wb_quota_reserved());
  }
  auto request=f.make(incoming,true);unsigned request_uid=request->get_request_uid();
  unsigned first=(unsigned)-1;assert(f.cache->ft_raw_probe_for_test(request,first)==MISS && first==2);
  f.send(request);
  if(mode==2) { f.return_read(other+stride);f.step();assert(f.cache->access_ready()); }
  // push has a one-cycle ROP-to-frontend boundary. For U ensure the ready
  // response is consumed on the actual admission cycle by pre-queueing request.
  for(unsigned n=0;n<8 && !f.cache->ft_reservation_active();++n) f.step();
  assert(f.cache->ft_reservation_active() && f.cache->ft_pending_index_for_test()==2);
  assert(mode==2 || f.cache->ft_pending_phase_for_test()==1);
  assert(f.cache->ep_l2_wad_contains(victim));
  if (std::stoul(argv[6])) assert(f.cache->ft_wb_quota_reserved()==1 && f.cache->ft_wb_quota_live()==extra);
  printf("A1 armed mode=%u C=%u native16way=1 reserved=15 pending=2 phase=%u victim_id=%u generation=%u\n",
      mode,c,f.cache->ft_pending_phase_for_test(),held.payload_id,held.generation);

  // This real old read enters the native ReturnQ after phase0/denial and
  // before the next frontend retry. Production fill precedes frontend probe.
  if(extra) {
    assert(f.cache->ft_wb_quota_reserved()==1 && f.cache->ft_wb_quota_live()==15);
    // Hold the real response path for several physical cycles. Only advance
    // the physical GPU clock, leaving the actual pending request untouched.
    f.gpu->gpu_sim_cycle+=3;
    assert(f.cache->ft_wb_quota_reserved()==1 && f.cache->ft_wb_quota_live()==15);
    puts("U_CAP16 actual mixed15live+1reserved and retained multi-cycle token PASS");
  }
  f.return_read(0);
  f.step(); // Old source must fail its pending.tag_index==reprobe.index assert.
  assert(f.cache->ft_identity_redirects_for_test()>0 && f.cache->ft_last_redirect_from_for_test()==15);
  printf("A1 competing fill exercised: raw candidate changed to way15, transaction committed way2; redirects=%llu\n",
      f.cache->ft_identity_redirects_for_test());
  // Remaining fills and real lower FIFO accept; deliberately hold set_done
  // until staging is gone, proving WAD's distinct lower lifetime.
  for(unsigned n=0;n<1000 && !f.replies.count(request_uid);++n) f.step();
  assert(f.replies.count(request_uid));
  auto landed=f.cache->ep_l2_tag_payload_handle_for_test(2);
  const auto &new_line=f.cache->ep_l2_payload_for_test().inspect(landed);
  assert(f.cache->ft_native_tag_owner_for_test(2)==incoming && f.cache->ft_native_tag_owner_for_test(15)==0);
  assert(new_line.owner==incoming && new_line.versions[0]==(unsigned long long)request_uid+1);
  auto competed=f.cache->ep_l2_tag_payload_handle_for_test(15);
  assert(f.cache->ep_l2_payload_for_test().inspect(competed).owner==0);

  for(unsigned n=0;n<4;++n)f.step();
  assert(f.lower_wb==0 && f.cache->ft_wb_staged()==1+extra && f.cache->ep_l2_wad_contains(victim));
  if (std::stoul(argv[6])) assert(!f.cache->ft_wb_quota_reserved() && f.cache->ft_wb_quota_live()==1+extra && f.cache->ft_wb_quota_retries()>0);
  f.block_lower=false;f.drain_lower();
  std::vector<new_addr_type> remaining;for(auto &entry:f.reads)remaining.push_back(entry.first);
  for(auto addr:remaining){f.return_read(addr);f.step();}
  for(unsigned n=0;n<1000 && (f.cache->ft_wb_staged() || !f.cache->miss_queue_empty() || !f.sp->L2_dram_queue_empty());++n)f.step();
  assert(f.lower_wb==1+extra && f.cache->ft_wb_staged()==0 && f.cache->ep_l2_wad_contains(victim));
  for(auto w:f.wb){f.sp->set_done(w);delete w;++f.completed_wb;}f.wb.clear();
  for(unsigned n=0;n<1000 && f.part->busy();++n)f.step();
  assert(f.completed_wb==1+extra && f.replies.size()==f.expected.size());
  assert(f.sp->l2_char_no_resource_leak() && f.cache->ft_drained() && !f.part->busy());
  assert(f.cache->ep_l2_payload_pending_operations()==0 && !f.cache->ep_l2_wad_occupancy());
  assert(f.bank_max==1 && f.cache->ep_l2_payload_for_test().physical_slots()==(std::stoul(argv[5])?1168:1152+c));
  f.cache->ft_print(stdout,0);
  printf("A1 production 16way cross-cycle fill/exact-once/versions/lower-accept/set_done/bank/drain mode=%u C=%u PASS\n",mode,c);
  fold(f);printf("SUBCONTROL_PASS|HELD_REDIRECT|redirects=%llu|last_raw=%u\n",f.cache->ft_identity_redirects_for_test(),f.cache->ft_last_redirect_from_for_test());return 0;
}


static std::string sibling(char**argv,const char*file){std::string p=argv[2];return p.substr(0,p.find_last_of('/')+1)+file;}
static void frame_begin(const char*s){printf("DETACH_NATIVE_FRAME_BEGIN|%s\n",s);}
static void frame_end(const char*s){printf("DETACH_NATIVE_FRAME_END|%s\n",s);}
static void preview_context(combined_fixture&f,mem_fetch*m,int expected_descriptor,int expected_address,int expected_line_slots){
 l2_access_plan plan;
#ifdef EP_L2_DIAG_REFERENCE
 f.cache->preview_access(m->get_addr(),m,plan);
#else
 ep_l2_detach_diag::preview context;f.cache->preview_access(m->get_addr(),m,plan,&context);
 assert(context.capacity_consulted&&context.descriptor_check==expected_descriptor&&context.address_check==expected_address&&context.line_slots_check==expected_line_slots);
#endif
 assert(plan.probe_status==RESERVATION_FAIL);
}
static void address32_control(char**argv){frame_begin("M1_ADDRESS32_MERGE_CAPACITY");{
 combined_fixture f(argv);f.send(f.make(0,false));f.await_lower(1);
 for(unsigned i=0;i<31;++i){f.send(f.make(0,false));f.await_frontend();}
 assert(f.cache->ep_l2_descriptor_count_used()==32);auto extra=f.make(0,false);f.send(extra);tick(f,16);preview_context(f,extra,1,0,-1);
#ifndef EP_L2_DIAG_REFERENCE
 auto*d=ledger(f);uint64_t cap=d?d->total[ep_l2_detach_diag::denied_capacity]:0,missq=d?d->total[ep_l2_detach_diag::denied_missq]:0;
#endif
 std::list<cache_event>events;assert(f.cache->access(extra->get_addr(),extra,f.gpu->gpu_sim_cycle,events)==RESERVATION_FAIL);
#ifndef EP_L2_DIAG_REFERENCE
 if(d){assert(d->total[ep_l2_detach_diag::denied_capacity]==cap+1&&d->total[ep_l2_detach_diag::denied_missq]==missq);assert(d->total[ep_l2_detach_diag::ro_address_fail]>0);}
#endif
 f.finish();fold(f);puts("SUBCONTROL_PASS|M1_ADDRESS32_MERGE_CAPACITY|line_gate=NA");}frame_end("M1_ADDRESS32_MERGE_CAPACITY");}
static void merge_missq_control(char**argv){frame_begin("MERGE_MISSQ_AND_ZERO_MISSING");{
 combined_fixture f(argv);f.block_lower=true;unsigned n=0;
 while(f.cache->miss_queue_has_slots(1)){assert(n<500);f.send(f.make(n++*128,false));f.await_frontend();}
 assert(n>16&&f.cache->ep_l2_descriptor_count_used()<512);const auto occupancy=f.cache->miss_queue_occupancy();
 auto no_missing=f.make(0,false);f.send(no_missing);f.await_frontend();assert(f.cache->miss_queue_occupancy()==occupancy);
 auto extra=f.make(32,false);f.send(extra);tick(f,16);
#ifndef EP_L2_DIAG_REFERENCE
 auto*d=ledger(f);uint64_t cap=d?d->total[ep_l2_detach_diag::denied_capacity]:0,missq=d?d->total[ep_l2_detach_diag::denied_missq]:0;
#endif
 std::list<cache_event>events;assert(f.cache->access(extra->get_addr(),extra,f.gpu->gpu_sim_cycle,events)==RESERVATION_FAIL);
#ifndef EP_L2_DIAG_REFERENCE
 if(d)assert(d->total[ep_l2_detach_diag::denied_missq]==missq+1&&d->total[ep_l2_detach_diag::denied_capacity]==cap);
#endif
 f.block_lower=false;f.take_lower();f.finish();fold(f);puts("SUBCONTROL_PASS|MERGE_MISSQ_AND_ZERO_MISSING|finite_real_queue_full=1|zero_missing_admitted=1");}frame_end("MERGE_MISSQ_AND_ZERO_MISSING");}
static void m0_line_control(char**argv){frame_begin("M0_LINE128");{
 std::string config=sibling(argv,"T1M0.config");char*args[10];for(unsigned i=0;i<10;++i)args[i]=argv[i];args[2]=const_cast<char*>(config.c_str());
 combined_fixture f(args);for(unsigned i=0;i<128;++i){f.send(f.make(i*128,false));f.await_lower(i+1);}assert(f.cache->mshr_entries_used()==128);
 auto extra=f.make(128*128,false);f.send(extra);tick(f,16);preview_context(f,extra,1,1,0);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::ro_line_slots_fail,1);
#endif
 f.finish();fold(f);puts("SUBCONTROL_PASS|M0_LINE128|original_line_probe_and_capacity=REAL");}frame_end("M0_LINE128");}
int main(int argc,char**argv){assert(argc==10);const char*which=argv[9];
 const bool no_runtime=!std::strcmp(which,"T_OFF");
 const bool ready=!std::strcmp(which,"PRESSURE_NO_MISSING")||!std::strcmp(which,"RESPONSE_BACKPRESSURE");
 if(!std::strcmp(which,"PREVIEW_REPEAT")){std::string legacy=sibling(argv,"LEGACY_HELD.config");char*args[8];for(unsigned i=0;i<8;++i)args[i]=argv[i];args[2]=const_cast<char*>(legacy.c_str());args[3]=const_cast<char*>("1");frame_begin("HELD_REDIRECT");assert(held_redirect_original(8,args)==0);frame_end("HELD_REDIRECT");}
 if(!std::strcmp(which,"PRESSURE_STICKY_REUSE"))address32_control(argv);
 if(!std::strcmp(which,"DETACH_THEN_LATER_DENIAL"))merge_missq_control(argv);
 if(!std::strcmp(which,"ROLLUP_RESIDUAL"))m0_line_control(argv);
 frame_begin("MAIN_CONTROL");
 combined_fixture f(argv,!no_runtime,ready?"1024:16:64:1":"1024:16:64:64");
 if(!std::strcmp(which,"T_OFF")||!std::strcmp(which,"EMPTY_CERT")){fill(f,16,false);f.send(f.make(0x800000+16*stride,false));tick(f);assert(f.pending_reads.size()==16);}
 else if(!std::strcmp(which,"NO_PRESSURE_POSITIVE_RO")||!std::strcmp(which,"OBSERVER_NONINTERFERENCE")){f.read(0);f.read(32);f.read(0);}
 else if(!std::strcmp(which,"PRESSURE_NO_LIVE_RO")||!std::strcmp(which,"PREVIEW_BLOCKS_COMMIT")){fill(f,16,false);f.send(f.make(0,false));tick(f);assert(f.pending_reads.size()==16);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::pressure_no_live_ro,1);
#endif
 }
 else if(!std::strcmp(which,"PRESSURE_ONLY_OTHER_SET_RO")){fill(f,16,false);f.send(f.make(128,false));f.await_lower(17);f.send(f.make(0,false));tick(f);assert(f.pending_reads.size()==17);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::pressure_no_live_ro,1);
#endif
 }
 else if(!std::strcmp(which,"PRESSURE_STICKY_REUSE")){fill(f,16);for(unsigned i=0;i<16;++i){f.send(f.make(i*stride+32,false));f.await_lower(17+i);}for(unsigned i=f.pending_reads.size();i-->0;)if(f.pending_reads[i]->get_addr()%128==32)f.return_one(i);tick(f,128);assert(f.replies.size()==16);f.send(f.make(16*stride,false));tick(f);assert(f.pending_reads.size()==16);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::reuse_seen_first_transition,16);require_observer(f,ep_l2_detach_diag::way_reuse_seen,1);assert(!ledger(f)||ledger(f)->total[ep_l2_detach_diag::detach_success]==0);
#endif
 }
 else if(!std::strcmp(which,"PRESSURE_NO_MISSING")){fill(f,2);f.return_one(0);tick(f,32,false);assert(f.sp->ep_l2_l2icnt_occupancy()==1);auto h=f.tag_for(stride);f.return_one(0);tick(f,32,false);assert(f.cache->ep_l2_payload_for_test().inspect(h).ready_sector_mask.test(0));assert(f.cache->ep_l2_payload_for_test().inspect(h).pins==1);
#ifndef EP_L2_DIAG_REFERENCE
 if(auto*d=ledger(f)){auto p=f.cache->detach_diag_predicate_for_test(h);assert(p.attached&&!p.reuse&&(p.required&~p.ready)==0);assert(!f.cache->detach_diag_can_detach_for_test(h));}
#endif
 puts("CONTROL_SCOPE|ready_predicate=REAL_READY_PINNED_TRANSACTION|raw_all_reserved_pressure=NOT_ASSERTED|reason=ready_Tag_can_be_replacement_candidate_but_pinned_commit_stalls");
 }
 else if(!std::strcmp(which,"RESPONSE_BACKPRESSURE")){fill(f,16);auto old=f.tag_for(0);f.send(f.make(16*stride,false));f.await_lower(17);assert(!f.tag_for(0).valid());unsigned pick=~0U;for(unsigned i=0;i<f.pending_reads.size();++i)if(f.pending_reads[i]->get_ep_l2_payload_id()!=old.payload_id){pick=i;break;}assert(pick!=~0U);f.return_one(pick);tick(f,32,false);assert(f.sp->ep_l2_l2icnt_occupancy()==1);pick=~0U;for(unsigned i=0;i<f.pending_reads.size();++i)if(f.pending_reads[i]->get_ep_l2_payload_id()==old.payload_id){pick=i;break;}assert(pick!=~0U);f.return_one(pick);tick(f,32,false);assert(f.cache->ep_l2_payload_for_test().handle_live(old));assert(f.cache->ep_l2_payload_for_test().inspect(old).pins==1);assert(f.cache->ep_l2_payload_for_test().inspect(old).ready_sector_mask.test(0));puts("CONTROL_SCOPE|detached_ready_payload=REAL_PINNED|ICNT_FULL=1|drain_after_actual_consumer=REQUIRED");
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::detach_success,1);require_observer(f,ep_l2_detach_diag::detached_return_packets,1);
#endif
 }
 else if(!std::strcmp(which,"PRESSURE_ELIGIBLE")){fill(f,16);auto h=f.tag_for(0);f.send(f.make(16*stride,false));f.await_lower(17);assert(!f.tag_for(0).valid()&&f.cache->ep_l2_payload_for_test().handle_live(h));f.send(f.make(0,false));f.await_lower(18);auto n=f.tag_for(0);assert(n.valid()&&n!=h);for(unsigned i=0;i<f.pending_reads.size();++i)if(f.pending_reads[i]->get_ep_l2_payload_id()==h.payload_id){f.return_one(i);break;}tick(f,32);assert(f.tag_for(0)==n);assert(f.cache->ep_l2_payload_for_test().inspect(n).pending_sector_mask.any()&&!f.cache->ep_l2_payload_for_test().inspect(n).ready_sector_mask.test(0));
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::pressure_rewrite_miss,1);require_observer(f,ep_l2_detach_diag::detach_success,1);
#endif
 }
 else if(!std::strcmp(which,"NATIVE_REQUESTER_RO_VICTIM")){fill(f,16);auto mf=f.make(0x800000,false);f.send(mf);tick(f);assert(f.pending_reads.size()==16);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::pressure_relief_not_consulted,1);require_observer(f,ep_l2_detach_diag::pressure_eligible,1);
#endif
 auto direct=f.make(0x800000+stride,false);std::list<cache_event>events;auto s=f.cache->access(direct->get_addr(),direct,f.gpu->gpu_sim_cycle,events);assert(s==RESERVATION_FAIL);assert(!f.tag_for(0).valid());f.send(direct);puts("CONTROL_SCOPE|native_frontend=DENIED_NOT_CONSULTED|direct_access=REAL_RELIEVE_THEN_DIRECTORY_DENIAL");}
 else if(!std::strcmp(which,"PREVIEW_REPEAT")){fill(f,16);auto mf=f.make(16*stride,false);f.send(mf);tick(f,1);
#ifndef EP_L2_DIAG_REFERENCE
 auto*d=ledger(f);std::array<uint64_t,ep_l2_detach_diag::FIELD_COUNT>before{};if(d)before=d->total;
#endif
 for(unsigned i=0;i<32;++i){l2_access_plan p;f.cache->preview_access(mf->get_addr(),mf,p);
#ifndef EP_L2_DIAG_REFERENCE
 ep_l2_detach_diag::preview o;f.cache->detach_diag_snapshot(mf,o);f.cache->preview_access(mf->get_addr(),mf,p,&o);
#endif
 }
#ifndef EP_L2_DIAG_REFERENCE
 if(d)assert(d->total==before);
#endif
 f.await_lower(17);}
 else if(!std::strcmp(which,"DETACH_THEN_LATER_DENIAL")){fill(f,16);for(unsigned i=0;i<496;++i){auto a=((i%63)+1)*128+(i/63)*stride;f.send(f.make(a,false));f.await_lower(17+i);}assert(f.cache->ep_l2_descriptor_count_used()==512);auto mf=f.make(17*stride,false);f.send(mf);tick(f,16);auto h=f.tag_for(0);preview_context(f,mf,0,-1,-1);std::list<cache_event>events;auto s=f.cache->access(mf->get_addr(),mf,f.gpu->gpu_sim_cycle,events);assert(s==RESERVATION_FAIL&&!f.tag_for(0).valid()&&f.cache->ep_l2_payload_for_test().handle_live(h));
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::trigger_denied_after_detach,1);require_observer(f,ep_l2_detach_diag::denied_capacity,1);
#endif
 }
 else if(!std::strcmp(which,"ROLLUP_RESIDUAL")){
#ifndef EP_L2_DIAG_REFERENCE
 f.cache->detach_diag_kernel(true,11);
#endif
 f.read(0);
#ifndef EP_L2_DIAG_REFERENCE
 f.cache->detach_diag_kernel(true,12);
#endif
 f.read(128);
#ifndef EP_L2_DIAG_REFERENCE
 f.cache->detach_diag_kernel(false,11);f.cache->detach_diag_kernel(false,12);
#endif
 f.read(256);
 }
 else if(!std::strcmp(which,"BOUNDED_OUTPUT")){
#ifndef EP_L2_DIAG_REFERENCE
 ep_l2_detach_diag::ledger d;d.total[ep_l2_detach_diag::observed_heads]=~uint64_t(0);d.add(ep_l2_detach_diag::observed_heads);assert(d.overflow&&!d.partitions());ep_l2_detach_diag::registry *local=new ep_l2_detach_diag::registry();ep_l2_detach_diag::preview p;for(unsigned i=0;i<100;++i)local->sample_pressure(d,p,i,i);assert(local->sample_count==64&&d.total[ep_l2_detach_diag::samples_dropped]==36);local->output_bytes=67108865;std::string path=std::string("observer_output_cap_")+(std::getenv("EPL2_DETACH_DIAG")?std::getenv("EPL2_DETACH_DIAG"):"0")+".json";FILE*sink=std::fopen(path.c_str(),"wx");assert(sink);local->print_counts(sink,"SLICE",0,d,d.total);assert(local->output_invalid&&local->output_marker);std::fclose(sink);delete local;
#endif
 f.read(0);puts("CONTROL_SCOPE|overflow_and_sample_cap=PASS_OBSERVER_DATA_STRUCTURE_ONLY|model_state_not_seeded=1");}
 else {fprintf(stderr,"unknown control %s\n",which);return 2;}
 f.finish();signature(f);frame_end("MAIN_CONTROL");
#ifndef EP_L2_DIAG_REFERENCE
 if(auto*d=ledger(f)){assert(d->partitions());assert(d->total[ep_l2_detach_diag::detach_success]==d->total[ep_l2_detach_diag::detached_retired]);}
#endif
 printf("DETACH_DIAGNOSTIC_CONTROL_PASS|case=%s\n",which);return 0;
}
