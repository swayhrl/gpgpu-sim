#include "potential_fixture.h"
int main(int argc,char **argv) {
  assert(argc==8 && std::stoul(argv[3])==2 && std::stoul(argv[5])==0 && (std::stoul(argv[6])==16 || std::stoul(argv[6])==0));
  setvbuf(stdout,NULL,_IOLBF,0);fixture f(argv);f.block_lower=true;
  const new_addr_type stride=64*128;
  // No synthetic array occupancy: native dirty writes, real WB packets and
  // a full two-entry lower FIFO accumulate 16 local staging responsibilities.
  for(unsigned i=0;i<34;++i) {
    auto r=f.make(i*stride,true);f.memory[i*stride]=f.expected.at(r->get_request_uid());
    f.send(r);f.until_frontend_empty();
  }
  assert(f.sp->ep_l2_l2dram_occupancy()==2 && f.cache->ft_wb_staged()==16);
  assert(f.cache->ep_l2_wad_occupancy()==18 && f.cache->ep_l2_wad_capacity()==128);
  auto r=f.make(34*stride,true);unsigned uid=r->get_request_uid();f.memory[34*stride]=f.expected.at(uid);f.send(r);
  if(!std::stoul(argv[6])) {
    f.until_frontend_empty();
    assert(f.cache->ft_wb_staged()==17 && f.cache->ep_l2_payload_for_test().wb_used()==17 && f.cache->ep_l2_wad_occupancy()==19);
    assert(!f.cache->ft_wb_quota_live() && !f.cache->ft_wb_quota_reserved());
    f.block_lower=false;f.drain_lower();
    for(unsigned n=0;n<1000 && (f.cache->ft_wb_staged() || !f.cache->miss_queue_empty() || !f.sp->L2_dram_queue_empty());++n)f.step();
    assert(f.lower_wb==19 && f.cache->ep_l2_wad_occupancy()==19 && f.replies.count(uid));
    for(auto w:f.wb){f.sp->set_done(w);delete w;}f.wb.clear();
    for(unsigned n=0;n<1000 && f.part->busy();++n)f.step();
    assert(f.cache->ft_drained() && !f.part->busy() && f.sp->l2_char_no_resource_leak() && !f.cache->ep_l2_wad_occupancy());
    f.cache->ft_print(stdout,0);puts("U_WB16 default quota-OFF actual staging17 (>16), same1168/in-place/versions/WAD/exact-once/drain PASS");return 0;
  }
  for(unsigned n=0;n<8;++n)f.step();
  assert(!f.replies.count(uid) && !f.cache->ft_reservation_active() && f.cache->ft_wb_staged()==16);
  assert(f.cache->ft_wb_quota_live()==16 && !f.cache->ft_wb_quota_reserved() && f.cache->ft_wb_quota_refusals()>0);
  const auto &p=f.cache->ep_l2_payload_for_test();
  assert(p.wb_used()==16 && p.ownership_consistent());
  for(unsigned n=0;n<8;++n)f.step();
  assert(f.cache->ft_wb_staged()==16 && f.cache->ep_l2_wad_occupancy()==18 && !f.cache->ft_reservation_active());
  puts("U_CAP16 production local WB peak=16, 17th refusal/no partial commit/lower full PASS");
  f.block_lower=false;f.drain_lower();
  for(unsigned n=0;n<2000 && (!f.replies.count(uid) || f.cache->ft_wb_staged() || !f.cache->miss_queue_empty() || !f.sp->L2_dram_queue_empty());++n)f.step();
  assert(f.replies.count(uid) && f.lower_wb==19 && !f.cache->ft_wb_staged());
  assert(f.cache->ep_l2_wad_occupancy()==19);
  assert(!f.cache->ft_wb_quota_live() && !f.cache->ft_wb_quota_reserved());
  // Reuse local quota before any old set_done; WAD still covers19 packets.
  auto reuse=f.make(35*stride,true);unsigned reuse_uid=reuse->get_request_uid();
  f.memory[35*stride]=f.expected.at(reuse_uid);f.send(reuse);f.until_frontend_empty();
  for(unsigned n=0;n<1000 && (f.cache->ft_wb_staged() || !f.sp->L2_dram_queue_empty());++n)f.step();
  assert(f.replies.count(reuse_uid) && f.cache->ep_l2_wad_occupancy()==20 && f.lower_wb==20 && !f.cache->ft_wb_quota_live());
  // Data moved to lower, but the old address remains protected by WAD.
  auto hazard=f.make(0,true);unsigned hazard_uid=hazard->get_request_uid();f.send(hazard);
  auto waits=f.cache->ep_l2_wad_same_address_waits();
  for(unsigned n=0;n<8;++n)f.step();
  assert(!f.replies.count(hazard_uid) && f.cache->ep_l2_wad_same_address_waits()>waits && !f.cache->ft_reservation_active());
  auto first=f.wb.front();assert(first->get_addr()==0);f.sp->set_done(first);delete first;f.wb.erase(f.wb.begin());++f.completed_wb;
  f.memory[0]=f.expected.at(hazard_uid);
  f.until_frontend_empty();
  for(unsigned n=0;n<1000 && (f.cache->ft_wb_staged() || !f.sp->L2_dram_queue_empty());++n)f.step();
  assert(f.replies.count(hazard_uid) && f.lower_wb==21 && f.cache->ep_l2_wad_occupancy()==20);
  for(auto w:f.wb){f.sp->set_done(w);delete w;++f.completed_wb;}f.wb.clear();
  for(unsigned n=0;n<1000 && f.part->busy();++n)f.step();
  assert(f.completed_wb==21 && f.replies.size()==37 && f.replies.size()==f.expected.size());
  assert(f.sp->l2_char_no_resource_leak() && f.cache->ft_drained() && !f.part->busy());
  assert(!f.cache->ep_l2_payload_pending_operations() && !f.cache->ep_l2_wad_occupancy());
  assert(f.cache->ep_l2_payload_bank_wb_readout()==21 && f.bank_max==1);
  printf("U_CAP16 real copy source/staging banks: same=%u cross=%u\n",f.same_bank_copy,f.cross_bank_copy);
  assert(f.same_bank_copy==21 && !f.cross_bank_copy);
  f.cache->ft_print(stdout,0);
  puts("U_CAP16 production versions/exact-once/release-resume/copy stages/WAD128/drain PASS");
}
