// Directed production-path controls; CPU component fixture, never an application.
#ifdef NDEBUG
#error Agent B production controls require assertions enabled
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
class mem_fetch;
static void diag_test_witness(unsigned,unsigned long long,mem_fetch*);
#define DIAG_TEST_WITNESS_ACTIVE 1
#include "ms_ro_combined/native_fixture.h"
#include "../../src/gpgpu-sim/ep-l2-detach-diag.h"

static uint64_t digest=1469598103934665603ULL;
static void mix(uint64_t n){digest^=n;digest*=1099511628211ULL;}
static void diag_test_witness(unsigned kind,unsigned long long cycle,mem_fetch*m){
 mix(kind);mix(cycle);mix(m->get_request_uid());mix(m->get_addr());
 mix(m->get_access_type());mix(m->get_access_sector_mask().to_ulong());
 mix(m->get_ep_l2_payload_id());mix(m->get_ep_l2_payload_generation());
 if(m->has_ft_wb_data())for(auto x:m->get_ft_wb_data())mix(x);
}
static constexpr new_addr_type STRIDE=64*128;
static ep_l2_life::observer* life(combined_fixture&f){return f.cache->lifetime();}
static unsigned tag_index(combined_fixture&f,ep_l2::PayloadHandle h){
 for(unsigned i=0;i<1024;++i)if(f.cache->ep_l2_tag_payload_handle_for_test(i)==h)return i;
 assert(false&&"bound Tag handle not found");return ~0u;
}
template<class F> static void until(combined_fixture&f,F condition,unsigned limit=2000){
 for(unsigned n=0;n<limit&&!condition();++n)f.step();
 assert(condition());
}
static void return_address(combined_fixture&f,new_addr_type address){
 unsigned pick=~0u;
 for(unsigned i=0;i<f.pending_reads.size();++i)if(f.pending_reads[i]->get_addr()==address){pick=i;break;}
 assert(pick!=~0u);f.return_one(pick);
}
static void finish_and_witness(combined_fixture&f,const char*name){
 f.finish();
 for(unsigned i=0;i<1024;++i){
  auto h=f.cache->ep_l2_tag_payload_handle_for_test(i);
  mix(i);mix(h.payload_id);mix(h.generation);
  if(f.cache->ep_l2_payload_for_test().handle_live(h)){
   const auto&s=f.cache->ep_l2_payload_for_test().inspect(h);
   mix(s.owner);mix(s.pins);mix(s.pending_sector_mask.to_ulong());mix(s.ready_sector_mask.to_ulong());
   for(auto v:s.versions)mix(v);
  }
 }
 mix(f.gpu->gpu_sim_cycle+f.gpu->gpu_tot_sim_cycle);
 mix(f.cache->ep_l2_payload_bank_grants());mix(f.cache->ep_l2_payload_bank_conflicts());
 mix(f.cache->ep_l2_payload_bank_attempts());mix(f.replies.size());mix(f.lower_read_packets);mix(f.lower_read_bytes);
 if(auto*o=life(f))assert(o->closed&&o->invalid==0);
 printf("AGENT_B_PRODUCTION_SEMANTIC|case=%s|cycles=%llu|digest=%llu|replies=%zu|lower_packets=%llu|lower_bytes=%llu|bank_grants=%llu|bank_conflicts=%llu|bank_attempts=%llu|drained=%u\n",
  name,f.gpu->gpu_sim_cycle+f.gpu->gpu_tot_sim_cycle,(unsigned long long)digest,f.replies.size(),
  f.lower_read_packets,f.lower_read_bytes,f.cache->ep_l2_payload_bank_grants(),
  f.cache->ep_l2_payload_bank_conflicts(),f.cache->ep_l2_payload_bank_attempts(),unsigned(f.cache->ft_drained()));
 printf("AGENT_B_PRODUCTION_CONTROL_PASS|%s\n",name);
}
static void sector_overlap_reentry(char**argv){
 combined_fixture f(argv);
 auto first=f.make(0,false);f.send(first);f.await_lower(1);
 auto h=f.tag_for(0);assert(h.valid());unsigned index=tag_index(f,h);
 uint64_t generation=life(f)?life(f)->way[index].generation:0;
 f.send(f.make(32,false));f.await_lower(2);
 f.send(f.make(96,false));f.await_lower(3);
 if(auto*o=life(f)){assert(o->way[index].reserved==11);assert(o->set[0].c[ep_l2_life::ENTER]==1);}
 for(unsigned sector:{1u,0u,3u}){
  return_address(f,sector*32);
  until(f,[&]{return f.cache->ep_l2_payload_for_test().inspect(h).ready_sector_mask.test(sector);});
  if(auto*o=life(f)){
   assert(o->way[index].generation==generation);
   if(sector!=3)assert(o->way[index].reserved!=0&&o->set[0].c[ep_l2_life::EXIT]==0);
  }
 }
 until(f,[&]{return f.replies.size()==3 && !f.cache->ep_l2_payload_for_test().inspect(h).pins;});
 if(auto*o=life(f))assert(o->set[0].c[ep_l2_life::CLOSED_NEW]==1&&o->way[index].reserved==0);
 auto missing=f.make(64,false);f.send(missing);f.await_lower(1);
 assert(f.tag_for(0)==h);
 if(auto*o=life(f)){
  assert(o->way[index].generation==generation&&o->way[index].reserved==4);
  assert(o->set[0].c[ep_l2_life::ALLOC]==1&&o->set[0].c[ep_l2_life::REENTER]==1);
  assert(o->way[index].packets==4&&o->way[index].merges==2);
  assert(o->set[0].c[ep_l2_life::REQUESTER]==4&&o->set[0].c[ep_l2_life::MERGE]==2);
  assert(o->set[0].c[ep_l2_life::RO_INITIAL_READY]==3); // sectors0,1,3 resident at second admission
 }
 assert(f.lower_read_packets==4&&f.lower_read_bytes==128);
 finish_and_witness(f,"SECTOR_OVERLAP_REENTRY");
}
static void merge_packets(char**argv){
 combined_fixture f(argv);
 f.send(f.make(0,false));f.await_lower(1);
 auto h=f.tag_for(0);unsigned index=tag_index(f,h);
 f.send(f.make(0,false));f.await_frontend();
 assert(f.pending_reads.size()==1&&f.cache->ep_l2_descriptor_count_used()==2);
 if(auto*o=life(f)){
  assert(o->way[index].packets==2&&o->way[index].merges==1);
  assert(o->set[0].c[ep_l2_life::REQUESTER]==2&&o->set[0].c[ep_l2_life::MERGE]==1);
 }
 finish_and_witness(f,"RO_MERGE_PACKET_ONCE");
}
static void real_fail_preview(char**argv){
 combined_fixture f(argv);
 for(unsigned i=0;i<16;++i){f.send(f.make(((i+1)%16)*STRIDE,false));f.await_lower(i+1);}
 auto matching=f.make(0,false),incoming=f.make(16*STRIDE,false);
 unsigned found=~0u;
 assert(f.cache->ft_raw_probe_for_test(matching,found)==HIT_RESERVED);
 assert(f.cache->ft_raw_probe_for_test(incoming,found)==RESERVATION_FAIL);
 std::vector<unsigned char> before;
 auto*o=life(f);if(o){before.resize(sizeof(*o));std::memcpy(before.data(),o,sizeof(*o));}
 for(unsigned n=0;n<32;++n){
  l2_access_plan p;ep_l2_detach_diag::preview snapshot;
  f.cache->detach_diag_snapshot(incoming,snapshot);
  f.cache->preview_access(incoming->get_addr(),incoming,p,&snapshot);
  unsigned raw_index=~0u;
  assert(f.cache->ft_raw_probe_for_test(incoming,raw_index)==RESERVATION_FAIL);
  if(ep_l2_detach_diag::get(f.cache))assert(snapshot.raw==RESERVATION_FAIL);
 }
 if(o)assert(std::memcmp(before.data(),o,sizeof(*o))==0);
 f.send(matching);f.await_frontend();
 f.send(incoming);f.await_lower(17);
 if(o){
  assert(o->set[0].c[ep_l2_life::EVENT_PEAK]==16);
  assert(o->set[0].c[ep_l2_life::PROD_TAGFAIL]>0);
 }
 finish_and_witness(f,"REAL_TAGFAIL_PREVIEW32");
}
static void generation_old_return(char**argv){
 combined_fixture f(argv);unsigned first_uid=~0u;
 for(unsigned i=0;i<16;++i){auto addr=((i+1)%16)*STRIDE;auto m=f.make(addr,false);
  if(addr==0)first_uid=m->get_request_uid();f.send(m);f.await_lower(i+1);}
 auto old=f.tag_for(0);unsigned old_index=tag_index(f,old);
 uint64_t old_generation=life(f)?life(f)->way[old_index].generation:0;
 f.send(f.make(16*STRIDE,false));f.await_lower(17);
 assert(!f.tag_for(0).valid()&&f.cache->ep_l2_payload_for_test().handle_live(old));
 if(auto*o=life(f))assert(o->way[old_index].generation>old_generation);
 f.send(f.make(0,false));f.await_lower(18);
 auto fresh=f.tag_for(0);assert(fresh.valid()&&fresh!=old);
 unsigned fresh_index=tag_index(f,fresh);
 uint64_t generation=life(f)?life(f)->way[fresh_index].generation:0;
 uint64_t ordinal=life(f)?life(f)->ordinal:0;
 unsigned pick=~0u;
 for(unsigned i=0;i<f.pending_reads.size();++i)if(f.pending_reads[i]->get_ep_l2_payload_id()==old.payload_id &&
     f.pending_reads[i]->get_ep_l2_payload_generation()==old.generation){pick=i;break;}
 assert(pick!=~0u);f.return_one(pick);f.wait(first_uid,false);
 assert(f.tag_for(0)==fresh);
 const auto&s=f.cache->ep_l2_payload_for_test().inspect(fresh);
 assert(s.pending_sector_mask.test(0)&&!s.ready_sector_mask.test(0));
 if(auto*o=life(f))assert(o->way[fresh_index].generation==generation&&o->way[fresh_index].reserved==1&&o->ordinal==ordinal);
 finish_and_witness(f,"GENERATION_DETACHED_OLD_RETURN");
}
static void tvd_zero_duration(char**argv){
 combined_fixture f(argv);
 const new_addr_type base=0x800000;
 for(unsigned i=0;i<17;++i)f.read(base+i*STRIDE);
 auto t=f.cache->tvd_for_test()->find(base);assert(f.cache->tvd_for_test()->live(t));
 uint64_t alloc=life(f)?life(f)->set[0].c[ep_l2_life::ALLOC]:0;
 auto packets=f.lower_read_packets;f.read(base);
 assert(f.lower_read_packets==packets); // actual paid TVD hit; no new lower read
 if(auto*o=life(f))assert(o->set[0].c[ep_l2_life::ALLOC]==alloc);
 uint64_t zero=life(f)?life(f)->set[0].hist[0][0]:0;
 uint64_t closed=life(f)?life(f)->set[0].c[ep_l2_life::CLOSED_NEW]:0;
 uint64_t duration=life(f)?life(f)->set[0].c[ep_l2_life::SUM_NEW]:0;
 auto promotions=f.cache->tvd_for_test()->stats().promotions;
 f.write(base);
 assert(f.cache->tvd_for_test()->stats().promotions==promotions+1&&f.lower_read_packets==packets);
 if(auto*o=life(f)){
  assert(o->set[0].hist[0][0]==zero+1);
  assert(o->set[0].c[ep_l2_life::CLOSED_NEW]==closed+1&&o->set[0].c[ep_l2_life::SUM_NEW]==duration);
  assert(o->set[0].c[ep_l2_life::ALLOC]==alloc+1&&o->set[0].occupancy==0);
 }
 finish_and_witness(f,"TVD_ZERO_DURATION");
}
// CPU-directed component case. Insert before main in the existing Agent B target.
// Uses existing life(), until(), finish_and_witness(); no production state seeded.
static void wad_early_no_fake_alloc(char**argv) {
  combined_fixture f(argv);
  const new_addr_type address=0x800000;  // outside the synthetic RO certificate
  for(unsigned i=0;i<17;++i)f.write(address+i*STRIDE);
  // Seventeen real same-set dirty writers produce one actual old writeback.
  // take_lower() observes its real lower-FIFO acceptance; set_done stays withheld.
  until(f,[&]{return !f.wb.empty();});
  assert(f.wb.size()==1 && f.wb.front()->get_addr()==address);
  assert(f.wb.front()->has_ft_wb_data());
  assert(f.cache->ep_l2_wad_contains(address));
  auto token=f.cache->tvd_for_test()->find(address);
  assert(f.cache->tvd_for_test()->live(token));
  assert(f.cache->tvd_for_test()->at(token.index).mandatory());

  std::array<ep_l2::PayloadHandle,1024> handles;
  std::array<new_addr_type,1024> owners;
  for(unsigned i=0;i<1024;++i) {
    handles[i]=f.cache->ep_l2_tag_payload_handle_for_test(i);
    owners[i]=f.cache->ft_native_tag_owner_for_test(i);
  }
  auto*o=life(f);
  const uint64_t allocations=o?o->set[0].c[ep_l2_life::ALLOC]:0;
  const uint64_t enters=o?o->set[0].c[ep_l2_life::ENTER]:0;
  const uint64_t ordinal=o?o->ordinal:0;
  const auto completed=f.completed_wb;
  auto writer=f.make(address,true);
  const unsigned uid=writer->get_request_uid();
  f.send(writer);
  // Do not use await_frontend here: the paid ROP/TVD lookup must really start.
  until(f,[&]{return f.cache->tvd_lookup_started_for_test(writer);});
  for(unsigned i=0;i<32;++i)f.step();
  assert(!f.replies.count(uid) && f.completed_wb==completed);
  assert(f.wb.size()==1 && f.cache->ep_l2_wad_contains(address));
  assert(f.cache->tvd_for_test()->live(token) &&
         f.cache->tvd_for_test()->at(token.index).mandatory());
  // Check actual production Tag bindings as well as observer allocation counts.
  for(unsigned i=0;i<1024;++i) {
    assert(f.cache->ep_l2_tag_payload_handle_for_test(i)==handles[i]);
    assert(f.cache->ft_native_tag_owner_for_test(i)==owners[i]);
  }
  if(o) {
    assert(o->set[0].c[ep_l2_life::ALLOC]==allocations);
    assert(o->set[0].c[ep_l2_life::ENTER]==enters && o->ordinal==ordinal);
  }

  // Legal external WB completion: the old snapshot completes first. Only then
  // update the expected memory version for the still-pending newer writer.
  f.complete_one();
  assert(f.completed_wb==completed+1);
  f.memory[address][0]=f.expected.at(uid)[0];
  f.wait(uid,false);
  assert(f.replies.count(uid) && !f.cache->ep_l2_wad_contains(address));
  assert(f.tag_for(address).valid());
  if(o)assert(o->set[0].c[ep_l2_life::ALLOC]>allocations);
  finish_and_witness(f,"WAD_EARLY_NO_FAKE_ALLOC");
}


int main(int argc,char**argv){
 assert(argc==10);
 const std::string name=argv[9];
 if(name=="SECTOR_OVERLAP_REENTRY")sector_overlap_reentry(argv);
 else if(name=="RO_MERGE_PACKET_ONCE")merge_packets(argv);
 else if(name=="REAL_TAGFAIL_PREVIEW32")real_fail_preview(argv);
 else if(name=="GENERATION_DETACHED_OLD_RETURN")generation_old_return(argv);
 else if(name=="TVD_ZERO_DURATION")tvd_zero_duration(argv);
 else if(name=="WAD_EARLY_NO_FAKE_ALLOC")wad_early_no_fake_alloc(argv);
 else {std::fprintf(stderr,"unknown Agent B control\n");return 2;}
 return 0;
}
