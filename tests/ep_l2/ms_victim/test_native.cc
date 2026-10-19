// SOURCE CANDIDATE: no PASS is claimed until exact A-integrated native execution.
#include "native_fixture.h"
#include <cstring>
#include <algorithm>

static const new_addr_type stride=64*128;

static void cycle_or_stream(char **argv, bool cyclic) {
  victim_fixture f(argv);
  const unsigned rounds=cyclic?3:1;
  for(unsigned pass=0;pass<rounds;++pass)
    for(unsigned i=0;i<17;++i) f.read(i*stride);
  const bool enabled=f.cache->tvd_enabled();
  if(cyclic) {
    assert(f.lower_read_packets==(enabled?17u:51u));
    if(enabled) assert(f.cache->tvd_for_test()->stats().read_responses==2);
  } else {
    assert(f.lower_read_packets==17);
    if(enabled) assert(!f.cache->tvd_for_test()->stats().read_responses);
  }
  printf("NATIVE_VICTIM|case=%s|enabled=%u|response_version_checks=%zu|"
         "actual_lower_read_packets=%llu|actual_lower_read_bytes=%llu|"
         "native_DRAM_timing=NOT_MODELED_BY_EXTERNAL_MEMORY_FIXTURE\n",
         cyclic?"cycle17_in_16way_set":"stream17_no_reuse",enabled,
         f.replies.size(),f.lower_read_packets,f.lower_read_bytes);
  f.finish();
}

static void sectors_and_late_fill(char **argv) {
  victim_fixture f(argv);
  std::vector<unsigned> uid;
  // Real descriptor coalescing and four native32B sector lower transactions.
  for(unsigned s=0;s<4;++s) {
    auto r=f.make(s*32,false);uid.push_back(r->get_request_uid());f.send(r);
  }
  for(unsigned n=0;n<20000 && f.reads.size()<4;++n) f.step();
  assert(f.reads.size()==4);
  for(unsigned s: {3u,1u,0u,2u}) { f.return_read(s*32);for(unsigned n=0;n<4;++n)f.step(); }
  for(auto id:uid) f.wait(id);
  for(unsigned i=1;i<=16;++i)f.read(i*stride);
  auto before=f.lower_read_packets;f.read(96);
  assert(f.lower_read_packets==before && f.cache->tvd_for_test()->stats().read_responses==1);
  // Sector96=3 was installed from real memory version44, not fabricated at hit.
  assert(f.memory.at(0)[3]==44);
  f.finish();puts("NATIVE_VICTIM|case=sector_3_1_0_2_real_fills_then_eviction|status=PASS");
}

static void missing_sector(char **argv) {
  victim_fixture f(argv);f.read(0);
  for(unsigned i=1;i<=16;++i)f.read(i*stride);
  auto t=f.cache->tvd_for_test()->find(0);assert(f.cache->tvd_for_test()->live(t));
  auto old=f.cache->tvd_for_test()->at(t.index).payload;
  auto before=f.lower_read_packets;f.read(32);
  assert(f.lower_read_packets==before+1);
  assert(!f.cache->tvd_for_test()->live(t));
  assert(!f.cache->ep_l2_payload_for_test().handle_live(old));
  f.finish();puts("NATIVE_VICTIM|case=missing_sector_real_lower_and_old_generation_retired|status=PASS");
}

static void dirty_snapshot(char **argv) {
  victim_fixture f(argv);auto first=f.write(0);const versions snapshot={{(unsigned long long)first+1,0,0,0}};
  for(unsigned i=1;i<=16;++i)f.write(i*stride);
  for(unsigned n=0;n<100 && f.wb.empty();++n)f.step();
  auto packet=std::find_if(f.wb.begin(),f.wb.end(),[](mem_fetch *p){return p->get_addr()==0;});
  assert(packet!=f.wb.end() && (*packet)->get_ft_wb_data()==snapshot);
  auto token=f.cache->tvd_for_test()->find(0);assert(f.cache->tvd_for_test()->live(token));
  auto h=f.cache->tvd_for_test()->at(token.index).payload;
  assert(f.cache->tvd_for_test()->at(token.index).status==l2_cache::tvd_directory::WB_INFLIGHT);
  auto reads=f.lower_read_packets;f.read(0);
  assert(f.lower_read_packets==reads && f.cache->tvd_for_test()->stats().dirty_forward_responses==1);
  auto r=f.make(0,true);const auto uid=r->get_request_uid();f.send(r);
  for(unsigned n=0;n<8;++n)f.step();
  assert(!f.replies.count(uid) && f.cache->ep_l2_payload_for_test().inspect(h).versions==snapshot);
  assert((*packet)->get_ft_wb_data()==snapshot);
  f.sp->set_done(*packet);delete *packet;f.wb.erase(packet);++f.completed_wb;
  f.memory[0][0]=(unsigned long long)uid+1;
  f.wait(uid);f.read(0);
  assert(first!=uid && !f.cache->tvd_for_test()->live(token));
  f.finish();puts("NATIVE_VICTIM|case=dirty_inflight_snapshot_wait_done_promote|status=PASS|dirty_pending_forward=NOT_EXERCISED_BY_THIS_CASE");
}

static void dirty_pending_complete_with_reader(char **argv) {
  victim_fixture f(argv);f.block_lower=true;
  // Actual native dirty traffic fills the two-entry lower FIFO. Third old
  // victim stays WB_STAGED in MissQ; no fabricated cache/pool occupancy.
  for(unsigned i=0;i<19;++i)f.write(i*stride);
  const new_addr_type address=2*stride;
  const auto token=f.cache->tvd_for_test()->find(address);
  assert(f.cache->tvd_for_test()->live(token));
  const auto h=f.cache->tvd_for_test()->at(token.index).payload;
  assert(f.cache->tvd_for_test()->at(token.index).status==l2_cache::tvd_directory::WB_STAGED);
  const auto saved=f.cache->ep_l2_payload_for_test().inspect(h).versions;
  auto read=f.make(address,false);const auto read_uid=read->get_request_uid();f.send(read);
  // ROP feeds the native ICNT-to-L2 FIFO at the END of a cache cycle.
  // Wait for the actual Tag-miss observation, not an assumed one-step phase.
  for(unsigned n=0;n<1000 && !f.cache->tvd_lookup_started_for_test(read);++n)f.step();
  assert(f.cache->tvd_lookup_started_for_test(read));
  assert(!f.replies.count(read_uid));
  f.block_lower=false;f.lower(); // Legal external FIFO-consumer action.
  // Explicit component contention through the PUBLIC actual arbiter. This
  // is not claimed as a naturally observed application opportunity.
  auto &ram=f.cache->ep_l2_payload_for_test();ep_l2::PayloadHandle contender;
  for(unsigned i=0;i<1024;++i) {
    auto x=f.cache->ep_l2_tag_payload_handle_for_test(i);
    if(ram.handle_live(x) && x.payload_id%4==h.payload_id%4 && x!=h) {contender=x;break;}
  }
  assert(contender.valid() && ram.request(contender,false,f.gpu->gpu_sim_cycle)==ep_l2_payload_store::GRANTED);
  f.step(); // Older lower-WB read and younger victim read both denied/queued.
  assert(f.cache->tvd_for_test()->at(token.index).readers==1 && ram.inspect(h).pins==1);
  assert(ram.pending_operations()>=2 && ram.inspect(h).allocation_role==ep_l2_payload_store::WB);
  f.step(); // Lower-WB wins first, packet accepted, victim read still queued.
  assert(f.cache->tvd_for_test()->at(token.index).status==l2_cache::tvd_directory::WB_INFLIGHT);
  assert(ram.inspect(h).allocation_role==ep_l2_payload_store::WB && ram.inspect(h).pins==1);
  auto packet=std::find_if(f.wb.begin(),f.wb.end(),[&](mem_fetch *p){return p->get_addr()==address;});
  assert(packet!=f.wb.end() && (*packet)->get_ft_wb_data()==saved);
  assert((*packet)->get_ep_l2_payload_id()==h.payload_id && (*packet)->get_ep_l2_payload_generation()==h.generation);
  f.sp->set_done(*packet); // Real completion while victim reader is bank-denied.
  assert(f.cache->tvd_for_test()->at(token.index).status==l2_cache::tvd_directory::WB_COMPLETE);
  assert(ram.inspect(h).allocation_role==ep_l2_payload_store::WB && ram.inspect(h).pins==1);
  assert((*packet)->get_ft_wb_data()==saved);delete *packet;f.wb.erase(packet);++f.completed_wb;
  f.wait(read_uid,false);
  assert(ram.inspect(h).pins==0 && ram.inspect(h).allocation_role==ep_l2_payload_store::TVD);
  assert(ram.inspect(h).versions==saved && !ram.pending_operations());
  f.write(address);f.read(address);f.finish();
  puts("NATIVE_VICTIM|case=dirty_pending_lower_accept_done_while_reader_queued|status=PASS|component_contender=PUBLIC_ARBITER");
}

static void fixed_capacity_capture(char **argv,unsigned expected_fixed) {
  victim_fixture f(argv);const auto fixed=f.cache->ep_l2_payload_for_test().fixed_victim_entries();
  assert(f.cache->tvd_enabled() && fixed==expected_fixed);
  for(unsigned i=0;i<96;++i)f.read(i*stride);
  assert(f.lower_read_packets==96);
  const auto before=f.lower_read_packets;
  // All96 were actual cold reads; the first80 evictions fit dynamic128 rows.
  // Fixed64 must reclaim oldest victims instead of borrowing unused quota.
  f.read(0);
  assert(f.lower_read_packets==before+(fixed?1u:0u));
  for(unsigned b=0;b<4;++b) {
    if(fixed)assert(f.cache->ep_l2_payload_for_test().fixed_charged_victims(b)<=16);
  }
  f.read(64*stride);f.write(64*stride);f.read(64*stride);
  printf("NATIVE_VICTIM|case=fixed_capacity_capture_and_promotion|fixed_entries=%u|"
         "actual_lower_read_packets=%llu|versions_checked=%zu|status=PASS\n",
         fixed,f.lower_read_packets,f.replies.size());
  f.finish();
}

static void clean_wb_ticket(char **argv) {
  victim_fixture f(argv);
  assert(f.cache->tvd_enabled());
  const auto first=f.write(0);
  for(unsigned i=1;i<=16;++i)f.write(i*stride);
  for(unsigned n=0;f.wb.empty();++n) { assert(n<128);f.step(); }
  auto packet=std::find_if(f.wb.begin(),f.wb.end(),[](mem_fetch *p){return p->get_addr()==0;});
  assert(packet!=f.wb.end());
  const auto old=f.cache->tvd_for_test()->find(0);
  const unsigned row=old.index;
  const auto incarnation=old.incarnation;
  const auto packet_uid=(*packet)->get_request_uid();
  const auto snapshot=(*packet)->get_ft_wb_data();
  assert(snapshot[0]==(unsigned long long)first+1);
  assert(f.cache->tvd_for_test()->live(old));
  assert(f.cache->tvd_for_test()->writeback_matches(old,0,packet_uid));
  assert(!f.cache->tvd_for_test()->at(row).payload.valid()); // CLEAN_ONLY WB has no retained local slot.
  auto r=f.make(0,false);const auto uid=r->get_request_uid();f.send(r);
  const auto reads=f.lower_read_packets;
  for(unsigned n=0;n<8;++n)f.step();
  assert(!f.replies.count(uid) && f.lower_read_packets==reads);
  assert((*packet)->get_ft_wb_data()==snapshot);
  f.sp->set_done(*packet);delete *packet;f.wb.erase(packet);++f.completed_wb;
  assert(!f.cache->tvd_for_test()->live(old));
  f.wait(uid);assert(f.lower_read_packets==reads+1);
  for(unsigned n=0;f.wb.empty();++n) { assert(n<128);f.step(); }
  const auto replacement=f.wb.front();
  const auto fresh=f.cache->tvd_for_test()->find(replacement->get_addr());
  assert(fresh.index==row && fresh.incarnation>incarnation);
  assert(f.cache->tvd_for_test()->writeback_matches(fresh,replacement->get_addr(),replacement->get_request_uid()));
  assert(!f.cache->tvd_for_test()->writeback_matches(fresh,replacement->get_addr(),packet_uid));
  assert(!f.cache->tvd_for_test()->live(old)); // Old completion cannot select reused row.
  f.finish();puts("NATIVE_VICTIM|case=clean_only_WB_ticket_snapshot_wait_done_row_reuse|status=PASS");
}

static void bypass_full(char **argv) {
  victim_fixture f(argv,"512:1:64:64");f.block_lower=true;
  auto a=f.make(0x4000,false),b=f.make(0x8000,false);
  const auto a_uid=a->get_request_uid(),b_uid=b->get_request_uid();
  f.send(a);
  for(unsigned n=0;f.sp->L2_dram_queue_empty();++n) { assert(n<64);f.step(); }
  assert(!a->has_ep_l2_payload_identity());
  f.send(b);for(unsigned n=0;n<8;++n)f.step();
  assert(f.sp->ep_l2_icntl2_occupancy()==1 && !b->has_ep_l2_payload_identity());
  assert(f.cache->tvd_lookup_queries_for_test()==0 && f.lower_read_packets==0);
  f.block_lower=false;
  for(unsigned n=0;f.reads.size()!=2;++n) { assert(n<64);f.step(); }
  f.return_all_reads_reverse();
  for(unsigned n=0;f.replies.size()!=2;++n) { assert(n<64);f.step(); }
  assert(f.replies.count(a_uid) && f.replies.count(b_uid) && !f.part->busy());
  assert(f.cache->tvd_lookup_queries_for_test()==0 && !f.cache->tvd_for_test()->occupied());
  assert(f.cache->tvd_actual_lower_read_packets()==0 && f.lower_read_packets==2);
  puts("NATIVE_VICTIM|case=texture_only_full_bypass_fifo_holds_real_head|bypass_lower_packets=2|cache_lower_packets=0|status=PASS");
}

int main(int argc,char **argv) {
  assert(argc==9);setvbuf(stdout,NULL,_IOLBF,0);
  if(!strcmp(argv[8],"cycle"))cycle_or_stream(argv,true);
  else if(!strcmp(argv[8],"stream"))cycle_or_stream(argv,false);
  else if(!strcmp(argv[8],"sectors"))sectors_and_late_fill(argv);
  else if(!strcmp(argv[8],"partial"))missing_sector(argv);
  else if(!strcmp(argv[8],"dirty"))dirty_snapshot(argv);
  else if(!strcmp(argv[8],"clean-wb-ticket"))clean_wb_ticket(argv);
  else if(!strcmp(argv[8],"bypass-full"))bypass_full(argv);
  else if(!strcmp(argv[8],"dirty_pending"))dirty_pending_complete_with_reader(argv);
  else if(!strcmp(argv[8],"fixed_quota"))fixed_capacity_capture(argv,0);
  else if(!strcmp(argv[8],"fixed_quota64"))fixed_capacity_capture(argv,64);
  else return 2;
}
