// Bounded CPU production-path qualification. Synthetic input; no DRAM timing or app.
#ifdef NDEBUG
#error Tag-capacity controls require every assertion enabled
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
static void mix(uint64_t x){digest^=x;digest*=1099511628211ULL;}
static void diag_test_witness(unsigned k,unsigned long long cycle,mem_fetch*m){
 mix(k);mix(cycle);mix(m->get_request_uid());mix(m->get_addr());mix(m->get_access_sector_mask().to_ulong());
 mix(m->get_ep_l2_payload_id());mix(m->get_ep_l2_payload_generation());
 if(m->has_ft_wb_data())for(auto x:m->get_ft_wb_data())mix(x);
}
static std::string geometry(unsigned s,unsigned w){return "S:"+std::to_string(s)+":128:"+std::to_string(w)+",L:B:m:L:P,A:128:1,256:0,32";}
struct capacity_fixture:combined_fixture{
 capacity_fixture(char**argv,const std::string&g,bool response_small=false):combined_fixture(argv,true,response_small?"1024:16:64:1":"1024:16:64:64",g.c_str()){
  auto*mc=gpu->getMemoryConfig();auto&ram=cache->ep_l2_payload_for_test();
  assert(ram.physical_slots()==1168 && ram.k_resident_slots==1024 && ram.bypass_capacity()==128 && ram.static_wb_capacity()==16);
  assert(cache->mshr_entry_capacity()==128 && cache->mshr_merge_capacity()==1);
  assert(cache->ep_l2_descriptor_pool_capacity()==512 && cache->ep_l2_wad_capacity()==128);
  assert(cache->ep_l2_logical_tag_count_for_test()==logical_tag_count);
  assert(cache->ep_l2_tag_payload_entries_for_test()==logical_tag_count);
  assert(cache->ep_l2_tag_payload_consistent());
  // The 32-set decoder is its real polynomial; never relabel the64-set hash.
  auto c32=mc->m_L2_config,c64=mc->m_L2_config;auto g32=geometry(32,logical_tag_ways),g64=geometry(64,logical_tag_ways);
  c32.cache_config::init(&g32[0],FuncCachePreferNone);c64.cache_config::init(&g64[0],FuncCachePreferNone);
  bool differs=false;new_addr_type example=0;
  for(new_addr_type a=0;a<(1ull<<22);a+=128){
   auto a32=c32.set_index(a),a64=c64.set_index(a);assert(a32<32&&a64<64);
   assert(mc->m_L2_config.set_index(a)==(logical_tag_sets==32?a32:a64));
   if(a32!=(a64&31)){differs=true;example=a;break;}
  }
  assert(differs && "P/IPOLY32 must not be a64-set report mask");
  printf("TAG_CAPACITY_HASH|sets=%u|ways=%u|logical=%u|counterexample=%llu|hash32=%u|hash64=%u|not_masked64=1\n",logical_tag_sets,logical_tag_ways,logical_tag_count,(unsigned long long)example,c32.set_index(example),c64.set_index(example));
 }
 std::vector<new_addr_type> addresses(unsigned n,bool ro=true,unsigned set=0){
  std::vector<new_addr_type> out;auto&cfg=gpu->getMemoryConfig()->m_L2_config;new_addr_type base=ro?0:0x800000;
  for(new_addr_type a=base;a<base+0x800000&&out.size()<n;a+=128)if(cfg.set_index(a)==set)out.push_back(a);
  assert(out.size()==n);return out;
 }
 unsigned tag_index(ep_l2::PayloadHandle h){for(unsigned i=0;i<logical_tag_count;++i)if(cache->ep_l2_tag_payload_handle_for_test(i)==h)return i;assert(false);return~0u;}
 unsigned pending(ep_l2::PayloadHandle h,unsigned sector=0){
  for(unsigned i=0;i<pending_reads.size();++i)if(pending_reads[i]->get_ep_l2_payload_id()==h.payload_id&&pending_reads[i]->get_ep_l2_payload_generation()==h.generation&&pending_reads[i]->get_access_sector_mask().test(sector))return i;
  assert(false && "actual old lower packet not found");return~0u;
 }
 template<class Predicate>void until(Predicate p,unsigned limit=20000){for(unsigned n=0;n<limit&&!p();++n)step();assert(p());}
 void ticks(unsigned n=32,bool consume=true){while(n--)step(consume);}
 unsigned fill_pending(const std::vector<new_addr_type>&a){
  assert(a.size()>=logical_tag_ways);unsigned first_uid=~0u;
  for(unsigned i=0;i<logical_tag_ways;++i){
   const auto address=a[(i+1)%logical_tag_ways];auto *request=make(address,false);
   if(address==a[0])first_uid=request->get_request_uid();
   send(request);await_lower(i+1);
  }
  assert(cache->ft_reserved_ways_for_test(a[0])==logical_tag_ways);
  assert(first_uid!=~0u && expected.count(first_uid)==1);return first_uid;
 }
 void finish_named(const char*name){
  finish();assert(cache->ft_drained()&&!cache->ep_l2_descriptor_count_used()&&!cache->ep_l2_wad_occupancy());
  assert(cache->ep_l2_payload_ownership_consistent()&&cache->ep_l2_tag_payload_consistent());
  for(unsigned i=0;i<logical_tag_count;++i){auto h=cache->ep_l2_tag_payload_handle_for_test(i);mix(i);mix(h.payload_id);mix(h.generation);
   if(cache->ep_l2_payload_for_test().handle_live(h)){auto&s=cache->ep_l2_payload_for_test().inspect(h);mix(s.owner);mix(s.pins);mix(s.ready_sector_mask.to_ulong());mix(s.pending_sector_mask.to_ulong());for(auto x:s.versions)mix(x);}}
  auto*o=cache->lifetime();if(o){assert(o->closed&&!o->invalid);assert(o->active_sets==logical_tag_sets&&o->active_ways==logical_tag_ways);}
  printf("TAG_CAPACITY_SEMANTIC|case=%s|sets=%u|ways=%u|cycles=%llu|digest=%llu|replies=%zu|lower_packets=%llu|lower_bytes=%llu|bank_grants=%llu|bank_conflicts=%llu|bank_attempts=%llu|physical=1168|resident=1024|bypass=128|WB=16|MSHR=128|descriptors=512|drained=1\n",name,logical_tag_sets,logical_tag_ways,gpu->gpu_sim_cycle+gpu->gpu_tot_sim_cycle,(unsigned long long)digest,replies.size(),lower_read_packets,lower_read_bytes,cache->ep_l2_payload_bank_grants(),cache->ep_l2_payload_bank_conflicts(),cache->ep_l2_payload_bank_attempts());
  puts("TAG_CAPACITY_CONTROL_PASS");
 }
};
static void pressure_identity(char**argv,const std::string&g,bool T){
 capacity_fixture f(argv,g);assert(f.cache->ro_tag_detach_enabled_for_test()==T);auto a=f.addresses(f.logical_tag_ways+2);const unsigned first=f.fill_pending(a);
 auto old=f.tag_for(a[0]);assert(old.valid());unsigned oldidx=f.tag_index(old);
 assert(f.expected.count(first)==1);
 assert(f.pending_reads[f.pending(old)]->get_request_uid()!=first); // lower packet != original waiter
 auto matching=f.make(a[0],false),incoming=f.make(a[f.logical_tag_ways],false);unsigned idx=~0u;
 assert(f.cache->ft_raw_probe_for_test(matching,idx)==HIT_RESERVED);f.expected.erase(matching->get_request_uid());delete matching;
 assert(f.cache->ft_raw_probe_for_test(incoming,idx)==RESERVATION_FAIL);
 // Raw/prod preview32 times must not mutate lifecycle or production payload identities.
 std::vector<unsigned char> before;auto*o=f.cache->lifetime();if(o){before.resize(sizeof(*o));std::memcpy(before.data(),o,sizeof(*o));}
 for(unsigned n=0;n<32;++n){l2_access_plan p;ep_l2_detach_diag::preview s;f.cache->detach_diag_snapshot(incoming,s);f.cache->preview_access(incoming->get_addr(),incoming,p,&s);assert(f.cache->ft_raw_probe_for_test(incoming,idx)==RESERVATION_FAIL);assert(f.tag_for(a[0])==old);}
 if(o)assert(!std::memcmp(before.data(),o,sizeof(*o)));
 auto gen=o?o->way[oldidx].generation:0;f.send(incoming);
 if(!T){
  f.ticks(64);assert(f.pending_reads.size()==f.logical_tag_ways&&f.tag_for(a[0])==old&&!f.replies.count(first));
  if(auto*d=ep_l2_detach_diag::get(f.cache))assert(d->total[ep_l2_detach_diag::detach_success]==0);
  f.return_one(f.pending(old));f.await_lower(f.logical_tag_ways); // one returned, one legal new admission
  f.wait(first,false);assert(f.tag_for(a[f.logical_tag_ways]).valid());
 }else{
  f.await_lower(f.logical_tag_ways+1);assert(!f.tag_for(a[0]).valid()&&f.cache->ep_l2_payload_for_test().handle_live(old));
  if(o)assert(o->way[oldidx].generation>gen);
  f.send(f.make(a[0],false));f.await_lower(f.logical_tag_ways+2);auto fresh=f.tag_for(a[0]);assert(fresh.valid()&&fresh!=old);
  auto freshidx=f.tag_index(fresh);auto newgen=o?o->way[freshidx].generation:0,ord=o?o->ordinal:0;
  f.return_one(f.pending(old));f.wait(first,false);
  assert(f.tag_for(a[0])==fresh);auto&s=f.cache->ep_l2_payload_for_test().inspect(fresh);assert(s.pending_sector_mask.test(0)&&!s.ready_sector_mask.test(0));
  if(o)assert(o->way[freshidx].generation==newgen&&o->ordinal==ord&&o->way[freshidx].reserved==1);
  if(auto*d=ep_l2_detach_diag::get(f.cache))assert(d->total[ep_l2_detach_diag::detach_success]>=1);
 }
 f.finish_named(T?"PRESSURE_T_ON_IDENTITY":"PRESSURE_T_OFF_LEGAL_BLOCK");
}
static void sector_reorder(char**argv,const std::string&g){
 capacity_fixture f(argv,g);auto a=f.addresses(1)[0];auto p0=f.make(a,false),p1=f.make(a+32,false),dup=f.make(a,false);auto u0=p0->get_request_uid(),u1=p1->get_request_uid(),ud=dup->get_request_uid();
 f.send(p0);f.await_lower(1);auto h=f.tag_for(a);f.send(p1);f.await_lower(2);f.send(dup);f.await_frontend();assert(f.lower_read_packets==2&&f.tag_for(a)==h);
 f.return_one(f.pending(h,1));f.wait(u1,false);assert(!f.replies.count(u0)&&!f.replies.count(ud));assert(f.cache->ep_l2_payload_for_test().inspect(h).pending_sector_mask.test(0));
 f.return_one(f.pending(h,0));f.wait(u0,false);f.wait(ud,false);auto*o=f.cache->lifetime();auto ix=f.tag_index(h);auto gen=o?o->way[ix].generation:0;
 f.send(f.make(a+64,false));f.await_lower(1);assert(f.tag_for(a)==h);if(o){assert(o->way[ix].generation==gen);assert(o->way[ix].request_mask==7);assert(o->set[0].c[ep_l2_life::REENTER]>=1);}
 f.return_one(f.pending(h,2));f.finish_named("SECTOR_MERGE_REORDER");
}
static void response_backpressure(char**argv,const std::string&g){
 capacity_fixture f(argv,g,true);auto a=f.addresses(f.logical_tag_ways+1);f.fill_pending(a);auto old=f.tag_for(a[0]);
 f.send(f.make(a[f.logical_tag_ways],false));f.await_lower(f.logical_tag_ways+1);auto fresh=f.tag_for(a[f.logical_tag_ways]);assert(fresh.valid()&&fresh!=old&&!f.tag_for(a[0]).valid());
 f.return_one(f.pending(fresh));f.ticks(32,false);assert(f.sp->ep_l2_l2icnt_occupancy()==1);
 f.return_one(f.pending(old));f.ticks(32,false);auto&ram=f.cache->ep_l2_payload_for_test();assert(ram.handle_live(old));auto&s=ram.inspect(old);assert(s.pins==1&&s.ready_sector_mask.test(0));assert(f.tag_for(a[f.logical_tag_ways])==fresh);
 f.finish_named("RESPONSE_BACKPRESSURE_DRAIN");
}
static void physical_tail(char**argv,const std::string&g){
 capacity_fixture f(argv,g);auto native=f.addresses(f.logical_tag_ways-1,false),ro=f.addresses(131);for(auto a:native){f.send(f.make(a,false));f.await_lower(f.pending_reads.size()+1);}
 f.send(f.make(ro[0],false));f.await_lower(f.logical_tag_ways);auto old=f.tag_for(ro[0]);auto index=f.tag_index(old);unsigned bank=old.payload_id%4,maxid=old.payload_id;
 std::vector<ep_l2::PayloadHandle> detached;
 for(unsigned n=1;n<130;++n){auto previous=f.tag_for(ro[n-1]);assert(previous.valid());f.send(f.make(ro[n],false));f.await_lower(f.logical_tag_ways+n);assert(!f.tag_for(ro[n-1]).valid()&&f.cache->ep_l2_payload_for_test().handle_live(previous));detached.push_back(previous);auto h=f.tag_for(ro[n]);assert(f.tag_index(h)==index&&h.payload_id%4==bank);maxid=std::max(maxid,h.payload_id);}
 assert(maxid>=512 && "logicalTag shrink must not shrink physicalResident allocation");
 for(auto h:detached)assert(f.cache->ep_l2_payload_for_test().handle_live(h));
 printf("TAG_CAPACITY_PHYSICAL_TAIL|logical=%u|max_actual_payload_id=%u|same_bank=%u|pending_RO_detached=%zu|physical=1168|resident=1024\n",f.logical_tag_count,maxid,bank,detached.size());
 f.finish_named("PHYSICAL_TAIL_SAME_BANK");
}
int main(int argc,char**argv){
 assert(argc==13);unsigned sets=std::stoul(argv[10]),ways=std::stoul(argv[11]);bool T=std::stoul(argv[12]);assert((sets==32||sets==64)&&(ways==8||ways==16));auto g=geometry(sets,ways);
 const std::string test=argv[9];
 if(test=="PRESSURE_IDENTITY")pressure_identity(argv,g,T);
 else if(test=="SECTOR_MERGE_REORDER"){assert(T);sector_reorder(argv,g);}
 else if(test=="RESPONSE_BACKPRESSURE"){assert(T);response_backpressure(argv,g);}
 else if(test=="PHYSICAL_TAIL_SAME_BANK"){assert(T);physical_tail(argv,g);}
 else {std::fprintf(stderr,"unknown tagcapacity control\n");return 2;}
}
