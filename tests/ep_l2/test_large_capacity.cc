// Bounded synthetic requests through real memory_sub_partition/cache/tag/MSHR
// and lower accept/return lifecycle. This is not an application/trace run.
#ifdef A2_OFF_REFERENCE
#include "/workspace/worktrees/gpgpu-sim-ep-l2-ms20261020-a-r39-own-pending-v1/tests/ep_l2/potential_fixture.h"
#else
#include "potential_fixture.h"
#endif
#include <sys/wait.h>
#include <unistd.h>
#include <sys/resource.h>

static void layout() {
#ifndef A2_OFF_REFERENCE
  using P=ep_l2_payload_store;
  P p(2,1,1024,false,false,1,true);
  assert(p.physical_slots()==74752 && p.resident_capacity()==65536 && p.bypass_capacity()==8192 && p.wb_begin()==73728);
  std::vector<P::payload_handle> hs;
  for(auto r:{P::RESIDENT,P::BYPASS,P::WB}) {
    unsigned begin=r==P::RESIDENT?0:r==P::BYPASS?65536:73728;
    unsigned end=r==P::RESIDENT?65536:r==P::BYPASS?73728:74752;
    // Sample both sides of each real scaled boundary; generations must not alias.
    for(unsigned id:{begin,end-1}) {
      auto h=p.reserve(id,r,128ull*(id+1),r==P::RESIDENT?P::RESIDENT_VALID:r==P::BYPASS?P::BYPASS_READY:P::WB_RESERVED);
      assert(h.payload_id==id && h.generation==1 && p.handle_live(h));hs.push_back(h);
    }
  }
  assert(p.ownership_consistent_deep());
  for(auto h:hs){p.release(h);assert(!p.handle_live(h));}
  assert(p.free_slots()==74752 && p.ownership_consistent_deep());
  auto old=p.reserve(65535,P::RESIDENT,128,P::RESIDENT_VALID);p.release(old);
  auto fresh=p.reserve(65535,P::RESIDENT,128,P::RESIDENT_VALID);
  assert(fresh.generation!=old.generation && !p.handle_live(old));p.release(fresh);
  // Fill actual enlarged WB region, prove finite boundary remains enforced.
  hs.clear();for(unsigned i=0;i<1024;++i){auto h=p.preview_free(P::WB);assert(h.valid()&&h.payload_id==73728+i);hs.push_back(p.reserve(h.payload_id,P::WB,i*128,P::WB_RESERVED));}
  assert(!p.preview_free(P::WB).valid() && p.wb_used()==1024);
  for(auto h:hs)p.release(h);
  assert(p.ownership_consistent_deep() && p.free_slots()==74752);
  puts("LARGE_LAYOUT_BOUNDARIES_GENERATION_FINITE_WB_DRAIN_PASS");
#endif
}

int main(int argc,char **argv) {
  assert(argc>=3);setvbuf(stdout,nullptr,_IOLBF,0);
  layout();
  char mode[]="1",wb[]="16",zero[]="0",one[]="1";
  char *args[]={argv[0],argv[1],argv[2],mode,wb,zero,zero,one};
  fixture f(args,"512:2:64:64",argc>3 ? "S:64:128:16,L:B:m:L:P,A:128:1,256:0,32" : "S:64:128:16,L:B:m:L:L,A:128:1,256:0,32");
#ifdef A2_OFF_REFERENCE
  const bool on=false;
#else
  const bool on=f.gpu->getMemoryConfig()->m_L2_config.ep_l2_large_enabled();
#endif
  auto &pool=f.cache->ep_l2_payload_for_test();
  assert(pool.physical_slots()==(on?74752:1168));
  auto local=[&](new_addr_type a) {
    for(;;a+=128){addrdec_t raw;f.gpu->getMemoryConfig()->m_address_mapping.addrdec_tlx(a,&raw);if(raw.sub_partition==0)return a;}
  };
  const auto cold=local(0x123400);
  auto r0=f.make(cold,false),r1=f.make(cold+32,false);
  auto id0=r0->get_request_uid(),id1=r1->get_request_uid();
  f.send(r0);f.step();f.send(r1);
  for(unsigned n=0;f.reads.size()!=2;++n){assert(n<1000);f.step();}
  f.return_read(cold+32);f.step();f.return_read(cold);
  for(unsigned n=0;!f.replies.count(id0)||!f.replies.count(id1);++n){assert(n<1000);f.step();}
  // Force real set conflicts independent of the selected legal index function.
  std::vector<new_addr_type> lines;
  unsigned set=(unsigned)-1;
  for(new_addr_type a=128;a<128ull*16000000 && lines.size()<18;a+=128) {
    addrdec_t raw;f.gpu->getMemoryConfig()->m_address_mapping.addrdec_tlx(a,&raw);if(raw.sub_partition!=0)continue;
    unsigned s=f.gpu->getMemoryConfig()->m_L2_config.set_index(a);
    if(set==(unsigned)-1)set=s;
    if(s==set)lines.push_back(a);
  }
  assert(lines.size()==18);
  for(unsigned i=0;i<16;++i) {
    auto w=f.make(lines[i],true);auto uid=w->get_request_uid();
    f.send(w);for(unsigned n=0;!f.replies.count(uid);++n){assert(n<1000);f.step();}
    f.memory[lines[i]]=f.expected.at(uid);
  }
  // Dirty replacement: copy is real, fixed latch remains even with free banks.
  auto w=f.make(lines[16],true);auto uid=w->get_request_uid();
  bool saw_latch=false;f.send(w);for(unsigned n=0;!f.replies.count(uid);++n){assert(n<1000);f.step();if(f.cache->ft_reservation_active()&&f.cache->ft_pending_phase_for_test()==1)saw_latch=true;}
  assert(saw_latch);
  f.memory[lines[16]]=f.expected.at(uid);
  for(unsigned n=0;f.wb.empty();++n){assert(n<1000);f.step();}
  assert(f.cache->ft_copy_lines()==1 && f.wb.size()==1 && f.cache->ep_l2_wad_occupancy()==1);
  auto packet=f.wb.back();assert(packet->has_ft_wb_data());
  assert(packet->get_ft_wb_data()==f.memory.at(packet->get_addr()));
  auto ordered=f.make(packet->get_addr(),false);uid=ordered->get_request_uid();
  f.send(ordered);for(unsigned i=0;i<8;++i)f.step();
  assert(!f.replies.count(uid) && f.reads.empty());
  // External fixture models the real lower completion callback; no fake ready.
  f.sp->set_done(packet);++f.completed_wb;delete packet;f.wb.clear();
  for(unsigned i=0;f.reads.empty();++i){assert(i<1000);f.step();}
  auto address=f.reads.begin()->first;f.return_read(address);
  for(unsigned i=0;!f.replies.count(uid);++i){assert(i<1000);f.step();}
  for(unsigned i=0;i<8;++i)f.step();
  // Reloading the evicted line itself evicts a second dirty producer. Finish
  // that actual lower WB too; a RESPONSE is not a hardware-drain receipt.
  for(auto packet:f.wb){f.sp->set_done(packet);++f.completed_wb;delete packet;}
  f.wb.clear();for(unsigned i=0;i<8;++i)f.step();
  assert(f.lower_wb==2 && f.completed_wb==2 && f.cache->ft_copy_lines()==2);
  if(on) {
    // Production capacity extension: >old128 native line records and >old512
    // response descriptors, held until the actual reversed lower returns.
    std::vector<unsigned> pending;std::vector<new_addr_type> addresses;
    std::set<unsigned> occupied_sets={set,f.gpu->getMemoryConfig()->m_L2_config.set_index(cold)};
    auto candidate=local(0x20000000ull);
    for(unsigned i=0;i<600;++i) {
      while(!occupied_sets.insert(f.gpu->getMemoryConfig()->m_L2_config.set_index(candidate)).second)candidate=local(candidate+128);
      auto r=f.make(candidate,false);assert(r->get_sub_partition_id()==0);candidate=local(candidate+128);pending.push_back(r->get_request_uid());
      addresses.push_back(r->get_addr());f.send(r);f.step();
    }
    for(unsigned n=0;f.reads.size()!=600;++n){assert(n<2000);f.step();}
    assert(f.cache->mshr_entries_used()==600 && f.cache->ep_l2_descriptor_count_used()==600);
    for(auto it=addresses.rbegin();it!=addresses.rend();++it)f.return_read(*it);
    for(unsigned n=0;;++n){assert(n<4000);bool done=true;for(auto id:pending)done&=f.replies.count(id);if(done)break;f.step();}
    pending.clear();
    auto merged=local(0x30000000ull);
    while(occupied_sets.count(f.gpu->getMemoryConfig()->m_L2_config.set_index(merged)))merged=local(merged+128);
    for(unsigned i=0;i<64;++i){auto r=f.make(merged,false);assert(r->get_sub_partition_id()==0);pending.push_back(r->get_request_uid());f.send(r);f.step();}
    f.until_frontend_empty();
    assert(f.reads.size()==1 && f.cache->ep_l2_descriptor_count_used()==64 && f.cache->mshr_max_targets_on_one_entry()==64);
    f.return_read(merged);
    for(unsigned n=0;;++n){assert(n<1000);bool done=true;for(auto id:pending)done&=f.replies.count(id);if(done)break;f.step();}
    puts("LARGE_CAPACITY_EXPANSION_NATIVE_PASS|held_line_MSHR=600|held_descriptors=600|per_address=64|reversed_real_returns=600|exact_once=1");
  }
  assert(f.cache->ft_drained() && !f.cache->ep_l2_wad_occupancy() && !pool.pending_sector_count());
  assert(f.cache->mshr_entries_used()==0 && f.cache->ep_l2_descriptor_count_used()==0 && pool.ownership_consistent());
  if(on){assert(pool.bank_grants()==0);assert(pool.diagnostic_services(ep_l2_payload_store::COPY_READ)==2 && pool.diagnostic_services(ep_l2_payload_store::COPY_WRITE)==2);}
  else assert(pool.bank_grants()>0);
#ifndef A2_OFF_REFERENCE
  if(on) {
    std::vector<new_addr_type> preload;unsigned chosen=(unsigned)-1;
    for(new_addr_type a=local(0x50000000ull);preload.size()!=17;a=local(a+128)) {
      const unsigned s=f.gpu->getMemoryConfig()->m_L2_config.set_index(a);
      if(s==set || s==f.gpu->getMemoryConfig()->m_L2_config.set_index(cold))continue;
      if(chosen==(unsigned)-1) {
        bool occupied=false;for(unsigned way=0;way<16;++way)occupied|=pool.handle_live(f.cache->ep_l2_tag_payload_handle_for_test(s*16+way));
        if(occupied)continue;chosen=s;
      }
      if(s==chosen)preload.push_back(a);
    }
    mem_access_sector_mask_t mask;mask.set(0);
    for(auto a:preload)assert(f.cache->ft_try_force_tag_access(a,f.gpu->gpu_sim_cycle,mask));
    assert(f.cache->large_prefill_clean_victims==1 && f.cache->ft_drained());
    puts("LARGE_HOST_PREFILL_CAPACITY_CONFLICT_COUNT_PASS|clean_replacements=1|not_timed_GPU_stall=1");
  }
#endif
  f.cache->ft_print(stdout,0);f.sp->print(stdout);
  printf("EPL2MS_TEST|SEMANTIC|master=%u|requesters=%u|responses=%zu|copy=2|lower_accept=2|wb_complete=2|data=PASS|drain=PASS|cycle=%llu\n",on,on?684:20,f.replies.size(),f.gpu->gpu_sim_cycle);
  rusage usage{};getrusage(RUSAGE_SELF,&usage);
  printf("LARGE_PRODUCTION_NATIVE_PASS|master=%u|requests=%u|replies=%zu|copy=2|lower_WB=2|completed=2|reference_versions=PASS|same_address_order=PASS|drain=PASS|cycle=%llu|maxrss_kib=%ld|sizeof_slot=%zu\n",on,on?684u:20u,f.replies.size(),f.gpu->gpu_sim_cycle,usage.ru_maxrss,sizeof(ep_l2_payload_store::slot));
}
