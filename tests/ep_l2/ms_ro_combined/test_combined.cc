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
static void common_descriptor_pool_and_mandatory_wb(char **argv,bool data_free=false) {
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
  f.finish();
  if(data_free) {
    const auto &ram=f.cache->ep_l2_payload_for_test();
    assert(f.gpu->getMemoryConfig()->m_L2_config.m_ep_l2_diag_l2_data_free &&
           !ram.bank_grants() && !ram.pending_operations());
    assert(ram.diagnostic_services(ep_l2_payload_store::FILL)>0 &&
           ram.diagnostic_services(ep_l2_payload_store::RESPONSE)>0 &&
           ram.diagnostic_services(ep_l2_payload_store::FRONTEND)>0 &&
           ram.diagnostic_services(ep_l2_payload_store::LOWER_WB)>0 &&
           ram.bank_ro_fill_write()>0 && ram.bank_ro_response_read()>0);
  }
  printf("C_FULL_V2|case=wb_victim_ro_native_D512|data_free=%u|status=PASS\n",unsigned(data_free));
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
static void pinned_wb_completion_with_ro_landing(char **argv) {
  combined_fixture f(argv);f.block_lower=true;
  // Fill the actual16-entry lower FIFO: old victims16..18 stay WB_STAGED.
  for(unsigned i=0;i<35;++i)f.write(0x400000+i*stride);
  const auto address=0x400000+16*stride;auto dir=f.cache->tvd_for_test();auto ticket=dir->find(address);
  assert(dir->live(ticket) && dir->at(ticket.index).status==l2_cache::tvd_directory::WB_STAGED);
  auto wb_handle=dir->at(ticket.index).payload;auto saved=f.cache->ep_l2_payload_for_test().inspect(wb_handle).versions;
  auto ro=f.make(128,false);auto ro_uid=ro->get_request_uid();f.send(ro);
  for(unsigned n=0;n<1000 && !f.tag_for(128).valid();++n)f.step();
  auto ro_handle=f.tag_for(128);auto &ram=f.cache->ep_l2_payload_for_test();
  assert(ro_handle.valid() && ram.inspect(ro_handle).allocation_role==ep_l2_payload_store::RO && ram.inspect(ro_handle).pins==1);
  auto reader=f.make(address,false);auto uid=reader->get_request_uid();f.send(reader);
  for(unsigned n=0;n<1000 && !f.cache->tvd_lookup_started_for_test(reader);++n)f.step();
  assert(f.cache->tvd_lookup_started_for_test(reader));
  f.block_lower=false;f.take_lower(); // Real external FIFO consumption.
  ep_l2::PayloadHandle contender;
  for(unsigned i=0;i<1024;++i) {
    auto h=f.cache->ep_l2_tag_payload_handle_for_test(i);
    if(ram.handle_live(h) && h!=ro_handle && h!=wb_handle && h.payload_id%4==wb_handle.payload_id%4){contender=h;break;}
  }
  assert(contender.valid() && ram.request(contender,false,f.gpu->gpu_sim_cycle)==ep_l2_payload_store::GRANTED);
  f.step();assert(dir->at(ticket.index).readers==1 && ram.inspect(wb_handle).pins==1);
  f.step();assert(dir->at(ticket.index).status==l2_cache::tvd_directory::WB_INFLIGHT);
  auto packet=std::find_if(f.wb.begin(),f.wb.end(),[&](mem_fetch *p){return p->get_addr()==address;});
  assert(packet!=f.wb.end() && (*packet)->get_ft_wb_data()==saved);
  f.sp->set_done(*packet);delete *packet;f.wb.erase(packet);++f.completed_wb;
  assert(dir->at(ticket.index).status==l2_cache::tvd_directory::WB_COMPLETE && ram.inspect(wb_handle).pins==1);
  assert(ram.handle_live(ro_handle) && ram.inspect(ro_handle).pins==1);
  f.wait(uid,false);assert(!ram.inspect(wb_handle).pins && ram.inspect(wb_handle).status==ep_l2_payload_store::TVD_VALID);
  f.wait(ro_uid);f.finish();puts("C_FULL_V2|case=pinned_wb_done_with_ro_landing|status=PASS|contender=PUBLIC_ARBITER");
}
static void factors_on_common_victim_parent(char **argv,unsigned detach,unsigned bypass,bool line_pressure) {
  combined_fixture f(argv,detach||bypass);
  const unsigned admitted=line_pressure?128:16;
  for(unsigned i=0;i<admitted;++i) {
    f.send(f.make(line_pressure?i*128:i*stride,false));f.await_lower(i+1);
  }
  auto extra=f.make(line_pressure?admitted*128:admitted*stride,false);auto uid=extra->get_request_uid();f.send(extra);
  const bool passes=line_pressure?bypass:detach;
  for(unsigned n=0;n<128;++n)f.step();
  assert(f.pending_reads.size()==admitted+unsigned(passes));
  assert(f.cache->ep_l2_descriptor_count_used()==admitted+unsigned(passes));
  assert(f.cache->mshr_entries_used()==(bypass?0:admitted+unsigned(passes)));
  if(!passes){assert(!f.replies.count(uid));f.return_one(0);f.await_frontend();}
  f.finish();printf("C_FULL_V2|case=factor_%s|T=%u|M=%u|status=PASS\n",line_pressure?"line128":"tag16",detach,bypass);
}
static void native_denied_then_ready_ro_shared_response_head(char **argv) {
  combined_fixture f(argv,true,"1024:16:64:1");
  // A real native texture fill makes certified sector0 ready without an RO
  // waiter. The later RO sector1 miss pins that same landing but stays cold.
  f.memory[0]={{11,22,33,44}};
  active_mask_t active;active.set(0);mem_access_byte_mask_t bytes;
  for(unsigned i=0;i<32;++i)bytes.set(i);
  auto seed=f.alloc->alloc(0,TEXTURE_ACC_R,active,bytes,mem_access_sector_mask_t(1),32,
      false,f.gpu->gpu_sim_cycle,0,0,0,nullptr,0);
  f.expected[seed->get_request_uid()]=f.memory.at(0);
  const auto seed_uid=seed->get_request_uid();f.send(seed);f.wait(seed_uid);
  const new_addr_type writer_address=0xa00000,native_address=0x800080;
  f.write(writer_address);
  auto cold=f.make(32,false);const auto cold_uid=cold->get_request_uid();f.send(cold);f.await_lower(1);
  auto native=f.make(native_address,false);const auto native_uid=native->get_request_uid();
  f.send(native);f.await_lower(2);
  const auto ro_handle=f.tag_for(0),native_handle=f.tag_for(native_address),writer_handle=f.tag_for(writer_address);
  auto &ram=f.cache->ep_l2_payload_for_test();
  assert(ro_handle.valid() && native_handle.valid() && writer_handle.valid());
  assert(ro_handle.payload_id%4==native_handle.payload_id%4 &&
         writer_handle.payload_id%4!=native_handle.payload_id%4);
  assert(ram.inspect(ro_handle).ready_sector_mask.to_ulong()==1 &&
         ram.inspect(ro_handle).pending_sector_mask.to_ulong()==2);
  // Native becomes ready at cycle end. A different-bank real write HIT at
  // the next frontend phase fills ICNT after the native read was bank-denied.
  auto writer=f.make(writer_address,true);const auto writer_uid=writer->get_request_uid();
  f.memory[writer_address][0]=(unsigned long long)writer_uid+1;f.send(writer);
  unsigned index=~0U;
  for(unsigned i=0;i<f.pending_reads.size();++i)
    if(!f.pending_reads[i]->ro_lower_packet() &&
       (f.pending_reads[i]->get_addr()&~127ull)==native_address)index=i;
  assert(index!=~0U);f.return_one(index);f.step(false);
  // Explicit paid component contender, as in B's pin test. It is not an
  // observed application opportunity or a fabricated queue/payload record.
  assert(ram.request(ro_handle,false,f.gpu->gpu_sim_cycle)==ep_l2_payload_store::GRANTED);
  f.step(false);
  assert(f.sp->ep_l2_l2icnt_occupancy()==1 && ram.pending_operations()==1 &&
         !f.replies.count(native_uid) && !f.replies.count(writer_uid));
  auto ready=f.make(0,false);const auto ready_uid=ready->get_request_uid();f.send(ready);
  // Merge a real requester for already-ready sector0 while ICNT is full.
  // It creates no new lower/data service but makes RO ready on the SAME bank
  // whose earlier native response reservation has not yet been consumed.
  for(unsigned n=0;n<64 && f.cache->ep_l2_descriptor_count_used()!=3;++n)f.step(false);
  assert(f.cache->ep_l2_descriptor_count_used()==3 && f.pending_reads.size()==1 &&
         f.sp->ep_l2_l2icnt_occupancy()==1 && ram.pending_operations()==1);
  f.collect();assert(f.replies.count(writer_uid) && !f.replies.count(native_uid));
  f.wait(native_uid,false); // Old unfrozen mux can prefer RO forever here.
  f.wait(ready_uid,false);f.wait(cold_uid);f.finish();
  puts("C_FULL_V2|case=native_denied_then_ready_ro_shared_response_head|status=PASS|contender=PUBLIC_ARBITER");
}
int main(int argc,char **argv) {
  assert(argc==10);
  unsigned t=0,m=0;char kind[16]={};
  if(sscanf(argv[9],"factor_%15[^_]_T%u_M%u",kind,&t,&m)==3) {
    assert(t<2 && m<2);factors_on_common_victim_parent(argv,t,m,!strcmp(kind,"line"));return 0;
  }
  if(!strcmp(argv[9],"same_address"))late_ro_and_same_address_victim(argv);
  else if(!strcmp(argv[9],"shared_pool"))common_descriptor_pool_and_mandatory_wb(argv);
  else if(!strcmp(argv[9],"host_update"))host_update_invalidates_only_uncovered_victim(argv);
  else if(!strcmp(argv[9],"fallback"))empty_coverage_keeps_R_flags_but_uses_native_limit(argv);
  else if(!strcmp(argv[9],"pinned_wb"))pinned_wb_completion_with_ro_landing(argv);
  else if(!strcmp(argv[9],"native_ro_mux"))native_denied_then_ready_ro_shared_response_head(argv);
  else if(!strcmp(argv[9],"data_free"))common_descriptor_pool_and_mandatory_wb(argv,true);
  else assert(false);
}
