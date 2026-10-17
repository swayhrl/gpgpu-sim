// Source-bound C++ directed fixture: actual store/transactions/MSHR, exact
// runtime prepare/commit and scheduler methods. See SOURCE_BINDING.json for
// the explicit clock/Tag/packet facades. This is NOT the19 production suite.
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include "ep-l2-ro-store-port.h"
#include "mshr_methods.inc"
using namespace ep_l2_ro;
using Store=ep_l2_payload_store;
struct packet_sink {
  bool valid=false;std::array<unsigned long long,4> versions{};
  void set_ft_wb_data(const std::array<unsigned long long,4>&v){versions=v;valid=true;}
};
struct facade_response {packet_sink *request=nullptr;unsigned sectors=0;identity transaction;};
struct transaction_adapter {
  transactions &actual;std::map<mem_fetch*,packet_sink> packets;
  explicit transaction_adapter(transactions &t):actual(t){}
  bool ready_response(identity&h,unsigned cursor)const{return actual.ready_response(h,cursor);}
  bool prepare_response(identity h,uint64_t cycle,facade_response &out){
    response r;if(!actual.prepare_response(h,cycle,r))return false;
    out.request=&packets.at(r.request);out.sectors=r.sectors;out.transaction=r.transaction;return true;
  }
  void commit_response(identity h,bool accepted,uint64_t cycle){actual.commit_response(h,accepted,cycle);}
};
struct native_queue_facade {
  bool native_ready=false;mem_fetch *native_packet=nullptr;
  bool access_ready()const{return native_ready;}
  mem_fetch *peek_next_access()const{assert(native_ready);return native_packet;}
};
struct runtime_facade;
struct scheduler_cache : native_queue_facade {
  Store &m_ep_l2_payload;uint64_t now=0;runtime_facade *m_ro=nullptr;
  std::map<mem_fetch*,identity> m_ft_read_refs;
  explicit scheduler_cache(Store &p):m_ep_l2_payload(p){}
  bool ft_enabled()const{return true;}
  bool ft_response_ready(unsigned long long cycle);
};
struct runtime_facade {
  typedef identity handle;
  scheduler_cache &c;transaction_adapter *tx;
  bool selected=false,prefer_ro=true;facade_response reply;
  uint64_t response_cycle=0;unsigned cursor=0;
  runtime_facade(scheduler_cache &cache,transaction_adapter *t):c(cache),tx(t){}
  uint64_t physical_cycle()const{return c.now;}
#include "runtime_methods.inc"
};
#include "response_scheduler.inc"
struct tags : tag_port {
  struct tag {identity h;new_addr_type line;uint64_t gen;unsigned ready;};
  std::map<unsigned,tag> live;uint64_t next=0;
  tag_binding attach(unsigned i,identity h,new_addr_type line){live[i]={h,line,++next,0};return {i,next};}
  bool matches(tag_binding b,identity h,new_addr_type line)const{
    auto i=live.find(b.index);return i!=live.end()&&i->second.h==h&&i->second.line==line&&i->second.gen==b.generation;
  }
  void ready(tag_binding b,identity h,new_addr_type line,unsigned mask,bool){assert(matches(b,h,line));live.at(b.index).ready=mask;}
  void detach(tag_binding b,identity h){assert(live.at(b.index).h==h);live.erase(b.index);}
  void read(identity,unsigned,const std::array<unsigned long long,4>&){}
};
struct rig {
  Store store;tags tag;store_port port;
  std::istringstream ranges;oracle_certificate cert;mshr_table shared;transactions tx;
  transaction_adapter adapter;scheduler_cache cache;runtime_facade rt;
  std::set<mem_fetch*> responses;std::array<unsigned long long,4> previous{};
  unsigned max_grants=0,serial=0;
  rig(unsigned entries=16):store(2,2,entries),port(store,tag),
    ranges("first_line,last_line_inclusive\n0000000000000000,0000000000020000\n"),
    cert(ranges,std::string(64,'b'),"TRACE_MODEL_CLOSED_WORLD"),shared(128,32,512,32),
    tx(shared,port,1152+entries,cert,switches(true,true)),adapter(tx),cache(store),rt(cache,&adapter){
    cache.m_ro=&rt;tx.begin_epoch(1,std::string(64,'b'));
  }
  mem_fetch *request(){auto p=reinterpret_cast<mem_fetch*>(uintptr_t(0x1000+(++serial)*16));adapter.packets[p]=packet_sink();return p;}
  identity admit(new_addr_type line,unsigned mask){
    auto free=store.preview_free(Store::RO,0);assert(free.valid());
    auto h=store.reserve(free.payload_id,Store::RO,line,Store::RO_FILL_ATTACHED);auto b=tag.attach(h.payload_id,h,line);
    assert(tx.admit(1,line,h,b,request(),mask,true,GLOBAL_DEMAND_READ)==ACCEPTED);
    tx.lower_accepted(h,mask);return h;
  }
  identity competitor(){auto h=store.preview_free(Store::WB,0);return store.reserve(h.payload_id,Store::WB,0x18000,Store::WB_STAGED);}
  void cycle(uint64_t n){cache.now=n;for(unsigned b=0;b<4;++b)previous[b]=store.bank_grants(b);}
  void check(){for(unsigned b=0;b<4;++b){auto d=store.bank_grants(b)-previous[b];assert(d<=1);max_grants=std::max(max_grants,unsigned(d));}assert(store.ownership_consistent());}
  bool fill(identity h,unsigned mask){std::array<unsigned long long,4> v{{11,22,33,44}};return port.accept_return(tx,h,mask,cache.now,v);}
  bool prepare(){return cache.ft_response_ready(cache.now);}
  void commit(){assert(rt.selected);auto p=adapter.packets.begin();for(;p!=adapter.packets.end();++p)if(&p->second==rt.reply.request)break;
    assert(p!=adapter.packets.end()&&responses.insert(p->first).second);rt.commit(cache.now);}
  void finish(unsigned expected){assert(responses.size()==expected&&shared.descriptor_count_used()==0&&tx.drained());
    assert(store.pending_operations()==0&&store.pending_sector_count()==0&&tx.end_epoch());assert(store.ownership_consistent());}
};
static int a2(unsigned entries){
  rig f(entries);auto h=f.admit(0x1000,1);assert(f.tx.merge(h,f.request(),2,1,true,GLOBAL_DEMAND_READ)==ACCEPTED);f.tx.lower_accepted(h,2);
  auto wb=f.competitor();const auto gen=h.generation;
  f.cycle(0);assert(f.fill(h,1));assert(f.store.inspect(h).pending_sector_mask.to_ulong()==2);f.check();
  f.cycle(1);assert(f.store.request(wb,false,1)==Store::GRANTED);assert(!f.fill(h,2));f.check();
  f.cycle(2);assert(!f.prepare());assert(!f.rt.selected);assert(f.fill(h,2));assert(f.store.inspect(h).pins==1);f.check();
  unsigned accepted=0;
  for(uint64_t c=3;c<35&&accepted<2;++c){f.cycle(c);if(f.prepare()){f.commit();++accepted;}f.check();}
  if(accepted!=2){printf("A2_FAIL|responses=%u|pending_tokens=%u|role=%u|generation=%u\n",accepted,f.store.pending_operations(),unsigned(f.store.inspect(h).allocation_role),gen);return 21;}
  assert(f.store.inspect(h).allocation_role==Store::RESIDENT&&f.store.inspect(h).pins==0&&h.generation==gen);
  assert(f.store.inspect(h).ready_sector_mask.to_ulong()==3);f.store.release(wb);f.finish(2);
  printf("A2_PASS|C=%u|responses=2|partial_fill=1|pending_tokens=0|max_grants=%u|generation=%u\n",entries,f.max_grants,gen);return 0;
}
static int a3(unsigned entries){
  rig f(entries);auto a=f.admit(0x1000,1),b=f.admit(0x1080,1);assert(a.payload_id==0&&b.payload_id==4);auto wb=f.competitor();
  f.cycle(0);assert(f.fill(b,1));assert(!f.fill(a,1));f.check();
  f.cycle(1);assert(!f.prepare());assert(!f.rt.selected);assert(f.fill(a,1));f.check();
  // Full ICNT: do not call prepare. Other bank users still retry; the selected
  // oldest read for slot4 must remain live and must precede this later WB.
  for(uint64_t c=2;c<5;++c){f.cycle(c);assert(f.store.request(wb,false,c)==Store::BANK_TRUE_CONTENTION);f.check();}
  bool wb_done=false;unsigned accepted=0;unsigned first=~0U;
  for(uint64_t c=5;c<38&&accepted<2;++c){f.cycle(c);
    if(f.prepare()){if(!accepted)first=f.rt.reply.transaction.payload_id;f.commit();++accepted;}
    if(!wb_done&&f.store.request(wb,false,c)==Store::GRANTED)wb_done=true;
    f.check();
  }
  if(accepted!=2){printf("A3_FAIL|responses=%u|pending_tokens=%u|old_candidate=%u|new_candidate=%u\n",accepted,f.store.pending_operations(),b.payload_id,a.payload_id);return 22;}
  assert(first==b.payload_id&&wb_done);assert(f.store.inspect(a).pins==0&&f.store.inspect(b).pins==0);
  f.store.release(wb);f.finish(2);
  printf("A3_PASS|C=%u|first=4|responses=2|ICNT_hold=3|fair_WB=1|pending_tokens=0|max_grants=%u\n",entries,f.max_grants);return 0;
}
static int controls(unsigned entries){
  rig f(entries);auto h=f.admit(0x1000,1);auto wb=f.competitor();
  f.cycle(0);assert(f.fill(h,1));f.check();
  f.cycle(1);assert(f.store.request(wb,false,1)==Store::GRANTED);assert(!f.prepare());assert(!f.rt.selected);
  for(auto &p:f.adapter.packets)assert(!p.second.valid);f.check();
  // A native response appears only after RO denial. It must not steal the
  // dispatcher forever while the Bank retains an older RO read reservation.
  auto native=reinterpret_cast<mem_fetch*>(uintptr_t(0x9000));f.cache.native_packet=native;
  f.cache.native_ready=true;f.cache.m_ft_read_refs[native]=wb;f.rt.prefer_ro=false;
  f.cycle(2);assert(f.prepare());assert(f.rt.selected&&f.rt.reply.transaction==h);
  auto grants=f.store.bank_grants();assert(f.prepare()&&f.store.bank_grants()==grants);f.check();
  // Grant happened, but an external queue rejects/delays enqueue. At the next
  // permitted attempt the same candidate must pay another RAM read; no free
  // retained completion data is introduced by selection.
  f.cycle(3);f.check(); // ICNT full: skip service/commit entirely
  f.cycle(4);assert(f.prepare());assert(f.store.bank_grants()==grants+1);f.commit();f.check();
  assert(f.store.inspect(h).pins==0);bool duplicate=false;
  try{f.rt.commit(4);}catch(const std::logic_error&){duplicate=true;}assert(duplicate);
  bool stale=false;f.cycle(5);
  try{f.port.accept_return(f.tx,identity(h.payload_id,h.generation+1),1,5,std::array<unsigned long long,4>{{}});}catch(const std::logic_error&){stale=true;}assert(stale);
  assert(f.shared.descriptor_count_used()==0);f.cache.native_ready=false;f.store.release(wb);f.finish(1);
  printf("CONTROL_PASS|C=%u|waiting_vs_granted=1|native_arrival=1|reread_after_backpressure=1|stale_generation=1|exact_once=1|max_grants=%u\n",entries,f.max_grants);return 0;
}
int main(int argc,char **argv){assert(argc==3);unsigned entries=std::stoul(argv[2]);std::string which=argv[1];
  if(which=="a2")return a2(entries);if(which=="a3")return a3(entries);if(which=="controls")return controls(entries);return 90;}
