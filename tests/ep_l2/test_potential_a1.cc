#include "potential_fixture.h"
// Deliberate native-credit fault injection, not application-reachability
// evidence. Protected member pointer accesses the actual configuration object;
// no downcast, Tag/payload mutation or production test hook is used.
struct credit_fault : cache_config {
  static unsigned &capacity(cache_config &c) { return c.*(&credit_fault::m_miss_queue_size); }
};
int main(int argc,char **argv) {
  assert(argc==7 || argc==8);
  const bool rollback=argc==8 && std::string(argv[7])=="rollback";
  const unsigned extra=argc==8 && !rollback?15:0;setvbuf(stdout,NULL,_IOLBF,0);fixture f(argv);
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
  if(rollback) {
    assert(mode==2 && std::stoul(argv[6])==16);
    auto &cc=const_cast<memory_config *>(f.gpu->getMemoryConfig())->m_L2_config;
    unsigned old=credit_fault::capacity(cc);credit_fault::capacity(cc)=0;
    // Skip the subpartition's predictive pre-admission for this injected loss:
    // call the normal public cache front door with its real pending requester.
    std::list<cache_event> events;
    auto status=f.cache->access(request->get_addr(),request,900001,events);
    credit_fault::capacity(cc)=old;
    assert(status==RESERVATION_FAIL && !f.cache->ft_reservation_active());
    assert(f.cache->ft_wb_quota_rollbacks()==1 && !f.cache->ft_wb_quota_reserved() && !f.cache->ft_wb_quota_live());
    assert(f.cache->ep_l2_tag_payload_handle_for_test(2)==held && !f.cache->ep_l2_wad_contains(victim));
    assert(f.cache->ep_l2_payload_for_test().inspect(held).versions==f.memory.at(victim));
    f.block_lower=false;f.drain_lower();
    std::vector<new_addr_type> remaining;for(auto &entry:f.reads)remaining.push_back(entry.first);
    for(auto addr:remaining){f.return_read(addr);f.step();}
    for(unsigned n=0;n<1000 && (!f.replies.count(request_uid) || f.cache->ft_wb_staged() || !f.cache->miss_queue_empty() || !f.sp->L2_dram_queue_empty());++n)f.step();
    assert(f.replies.count(request_uid) && f.lower_wb==1 && !f.cache->ft_wb_quota_live());
    for(auto w:f.wb){f.sp->set_done(w);delete w;}f.wb.clear();
    for(unsigned n=0;n<1000 && f.part->busy();++n)f.step();
    assert(!f.part->busy() && f.sp->l2_char_no_resource_leak() && f.cache->ft_drained() && !f.cache->ep_l2_payload_pending_operations() && !f.cache->ep_l2_wad_occupancy());
    assert(f.replies.size()==f.expected.size());f.cache->ft_print(stdout,0);
    puts("U_CAP16 injected native-credit failure: real rollback/release once/victim restore/retry/exact-once/drain PASS");
    return 0;
  }
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
}
