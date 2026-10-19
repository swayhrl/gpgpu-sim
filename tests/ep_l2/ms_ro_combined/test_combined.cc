// SOURCE ONLY until the exact A integrated candidate is compiled and E accepts.
#include "native_fixture.h"
#include <cstring>
static const new_addr_type stride=64*128;
static void late_ro_and_same_address_victim(char **argv) {
  combined_fixture f(argv);auto first=f.make(0,false);auto first_uid=first->get_request_uid();f.send(first);f.await_lower(1);
  auto old=f.tag_for(0);assert(old.valid());
  for(unsigned i=1;i<=16;++i) {f.send(f.make(i*stride,false));f.await_lower(i+1);}
  assert(!f.tag_for(0).valid() && f.cache->ep_l2_payload_for_test().handle_live(old));
  auto second=f.make(0,false);auto second_uid=second->get_request_uid();f.send(second);f.await_lower(18);
  auto newer=f.tag_for(0);assert(newer.valid() && newer!=old);
  unsigned selected=~0U;for(unsigned i=0;i<f.pending_reads.size();++i)
    if(f.pending_reads[i]->get_ep_l2_payload_id()==newer.payload_id)selected=i;
  assert(selected!=~0U);f.return_one(selected);f.wait(second_uid,false);
  const auto cold_address=17*stride;f.send(f.make(cold_address,false));f.await_lower(18);
  auto current=f.tag_for(cold_address);assert(current.valid());
  auto dir=f.cache->tvd_for_test();auto ticket=dir->find(0);assert(dir->live(ticket) && dir->at(ticket.index).payload==newer);
  auto lower=f.lower_read_packets;auto again=f.make(0,false);auto again_uid=again->get_request_uid();f.send(again);f.wait(again_uid,false);
  assert(f.lower_read_packets==lower); // Actual retained version served; old RO still pending.
  assert(f.cache->ep_l2_payload_for_test().handle_live(old));
  const auto queries=f.cache->tvd_lookup_queries_for_test();auto writer=f.make(0,true);bool refused=false;
  try{f.cache->tvd_frontend(writer,f.gpu->gpu_sim_cycle,true);}catch(const std::logic_error&){refused=true;}
  assert(refused && f.cache->tvd_lookup_queries_for_test()==queries && dir->live(ticket));
  f.expected.erase(writer->get_request_uid());delete writer;
  selected=~0U;for(unsigned i=0;i<f.pending_reads.size();++i)
    if(f.pending_reads[i]->get_ep_l2_payload_id()==old.payload_id)selected=i;
  assert(selected!=~0U);f.return_one(selected);f.wait(first_uid,false);
  auto &ram=f.cache->ep_l2_payload_for_test();
  assert(!ram.handle_live(old) && ram.handle_owner_matches(current,cold_address));
  assert(ram.inspect(current).ready_sector_mask.none() && ram.inspect(current).pending_sector_mask.any());
  assert(dir->live(ticket) && dir->at(ticket.index).payload==newer);
  f.finish();puts("C_FULL_V2|case=late_ro_same_address_victim|status=PASS");
}
static void common_descriptor_pool_and_mandatory_wb(char **argv) {
  combined_fixture f(argv);
  for(unsigned i=0;i<17;++i)f.write(0x400000+i*stride);
  for(unsigned n=0;n<1000 && f.wb.empty();++n)f.step();assert(!f.wb.empty());
  auto dir=f.cache->tvd_for_test();auto ticket=dir->find(0x400000);
  assert(dir->live(ticket) && dir->at(ticket.index).mandatory());auto wb_payload=dir->at(ticket.index).payload;
  f.read(0x400000); // Dirty forwarding pays TVD read, no extra lower read.
  for(unsigned i=0;i<384;++i){f.send(f.make(i*128,false));f.await_lower(i+1);}
  for(unsigned i=0;i<128;++i){f.send(f.make(0x800000+i*128,false));f.await_lower(385+i);}
  assert(f.cache->ep_l2_descriptor_count_used()==512 && f.cache->mshr_entries_used()==128);
  assert(dir->live(ticket) && dir->at(ticket.index).payload==wb_payload && dir->at(ticket.index).mandatory());
  auto extra=f.make(384*128,false);auto uid=extra->get_request_uid();f.send(extra);
  for(unsigned n=0;n<32;++n)f.step(false);
  assert(f.cache->ep_l2_descriptor_count_used()==512 && !f.replies.count(uid) && f.pending_reads.size()==512);
  f.return_one(0);f.await_frontend();
  assert(f.peak_shared==512 && dir->live(ticket));
  // Fill the real64-entry ICNT FIFO using real ready lower packets. New RO
  // completions still own paid pins/descriptors while a victim reader waits.
  for(unsigned n=0;n<20000 && f.sp->ep_l2_l2icnt_occupancy()<64;++n) {
    f.step(false);
    if(!f.pending_reads.empty() && !f.sp->dram_L2_queue_full())f.return_one(0);
  }
  assert(f.sp->ep_l2_l2icnt_occupancy()==64 && f.cache->ep_l2_payload_for_test().ro_used());
  auto reader=f.make(0x400000,false);const auto reader_uid=reader->get_request_uid();f.send(reader);
  for(unsigned n=0;n<32;++n)f.step(false);
  assert(!f.replies.count(reader_uid) && dir->live(ticket));
  const auto snapshot=f.wb.front()->get_ft_wb_data();f.complete_one();f.step(false);
  assert(dir->live(ticket) && dir->at(ticket.index).payload==wb_payload);
  assert(f.cache->ep_l2_payload_for_test().inspect(wb_payload).versions==snapshot);
  f.wait(reader_uid); // Actual queue consumer removes the backpressure.
  f.finish();puts("C_FULL_V2|case=wb_victim_ro_native_D512|status=PASS");
}
static void host_update_invalidates_only_uncovered_victim(char **argv) {
  combined_fixture f(argv);
  for(unsigned i=0;i<17;++i)f.read(0x400000+i*stride);
  auto dir=f.cache->tvd_for_test();auto ticket=dir->find(0x400000);assert(dir->live(ticket));
  const auto payload=dir->at(ticket.index).payload;const auto offset=f.sp->ft_memcpy_offset_for_test();
  assert(!f.sp->ft_try_force_l2_tag_update(0,f.gpu->gpu_sim_cycle,mem_access_sector_mask_t(1)));
  assert(f.sp->ft_memcpy_offset_for_test()==offset && dir->live(ticket));
  assert(f.sp->ft_try_force_l2_tag_update(0x400000,f.gpu->gpu_sim_cycle,mem_access_sector_mask_t(1)));
  assert(!dir->live(ticket) && !f.cache->ep_l2_payload_for_test().handle_live(payload));
  f.memory[0x400000][0]=0;f.read(0x400000); // selected host-prefill version-zero symbol.
  f.finish();puts("C_FULL_V2|case=certified_refusal_uncovered_host_victim_update|status=PASS");
}
static void empty_coverage_keeps_R_flags_but_uses_native_limit(char **argv) {
  combined_fixture f(argv);
  for(unsigned i=0;i<128;++i){f.send(f.make(i*128,false));f.await_lower(i+1);}
  assert(f.cache->mshr_entries_used()==128 && f.cache->ep_l2_descriptor_count_used()==128);
  for(auto p:f.pending_reads)assert(!p->ro_lower_packet());
  auto extra=f.make(128*128,false);auto uid=extra->get_request_uid();f.send(extra);
  for(unsigned n=0;n<32;++n)f.step();
  assert(!f.replies.count(uid) && f.pending_reads.size()==128 && !f.cache->ep_l2_payload_for_test().ro_used());
  f.return_one(0);f.await_frontend();f.finish();
  puts("C_FULL_V2|case=empty_coverage_flags_on_native_limit|status=PASS");
}
int main(int argc,char **argv) {
  assert(argc==10);
  if(!strcmp(argv[9],"same_address"))late_ro_and_same_address_victim(argv);
  else if(!strcmp(argv[9],"shared_pool"))common_descriptor_pool_and_mandatory_wb(argv);
  else if(!strcmp(argv[9],"host_update"))host_update_invalidates_only_uncovered_victim(argv);
  else if(!strcmp(argv[9],"fallback"))empty_coverage_keeps_R_flags_but_uses_native_limit(argv);
  else assert(false);
}
