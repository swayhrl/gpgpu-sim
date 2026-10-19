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
  f.finish();puts("NATIVE_VICTIM|case=dirty_inflight_snapshot_wait_done_promote|status=PASS|dirty_pending_forward=NOT_SUPPORTED");
}

int main(int argc,char **argv) {
  assert(argc==9);setvbuf(stdout,NULL,_IOLBF,0);
  if(!strcmp(argv[8],"cycle"))cycle_or_stream(argv,true);
  else if(!strcmp(argv[8],"stream"))cycle_or_stream(argv,false);
  else if(!strcmp(argv[8],"sectors"))sectors_and_late_fill(argv);
  else if(!strcmp(argv[8],"partial"))missing_sector(argv);
  else if(!strcmp(argv[8],"dirty"))dirty_snapshot(argv);
  else return 2;
}
