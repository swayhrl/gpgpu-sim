#include <assert.h>
#include <map>
#include <sstream>
#include <stdio.h>
#include "../../src/gpgpu-sim/ep-l2-ro-store-port.h"
using namespace ep_l2_ro;
static mem_fetch *req(unsigned n) {return reinterpret_cast<mem_fetch*>(uintptr_t(n+1));}
static std::string hash(){return std::string(64,'b');}
static unsigned long long memory(new_addr_type line,unsigned sector){return line*37+sector*11+17;}
static std::array<unsigned long long,4> data(new_addr_type line) {
  std::array<unsigned long long,4> d;
  for(unsigned i=0;i<4;++i)d[i]=memory(line,i);return d;
}
class tag_reference : public tag_port {
 public:
  struct tag {identity h;uint64_t generation;new_addr_type line;unsigned ready;};
  std::map<unsigned,tag> tags;
  std::map<unsigned,new_addr_type> payload_lines;
  uint64_t generation=0;
  unsigned reads=0,updates=0;
  tag_binding attach(unsigned index,identity h,new_addr_type line) {
    tags[index]=tag{h,++generation,line,0};payload_lines[h.payload_id]=line;
    return tag_binding(index,generation);
  }
  bool matches(tag_binding b,identity h,new_addr_type line) const {
    auto i=tags.find(b.index);
    return i!=tags.end()&&i->second.h==h&&i->second.generation==b.generation&&i->second.line==line;
  }
  void ready(tag_binding b,identity h,new_addr_type line,unsigned mask,bool) {
    assert(matches(b,h,line));tags[b.index].ready=mask;++updates;
  }
  void detach(tag_binding b,identity h) {assert(tags[b.index].h==h);assert(tags[b.index].generation==b.generation);tags.erase(b.index);}
  void read(identity h,unsigned mask,const std::array<unsigned long long,4>&versions) {
    for(unsigned s=0;s<4;++s)if(mask>>s&1)assert(versions[s]==memory(payload_lines.at(h.payload_id),s));
    ++reads;
  }
};
static identity reserve(ep_l2_payload_store &p,new_addr_type line,unsigned bank) {
  auto h=p.preview_free(ep_l2_payload_store::RO,bank);assert(h.valid());
  return p.reserve(h.payload_id,ep_l2_payload_store::RO,line,ep_l2_payload_store::RO_FILL_ATTACHED);
}
static void accounting(ep_l2_payload_store &p) {
  assert(p.free_slots()+p.resident_used()+p.bypass_used()+p.wb_used()+p.ro_used()==p.physical_slots());
  assert(p.ownership_consistent());
}
int main(){
  std::istringstream ranges("first_line,last_line_inclusive\n0000000000001000,0000000000200000\n");
  oracle_certificate cert(ranges,hash(),"TRACE_MODEL_CLOSED_WORLD");
  ep_l2_payload_store store(2,2,16);tag_reference tags;store_port port(store,tags);
  mshr_table shared(128,32,512,32);transactions t(shared,port,1168,cert,switches(true,true));
  t.begin_epoch(1,hash());
  auto a=reserve(store,0x1000,0);auto at=tags.attach(0,a,0x1000);
  assert(t.admit(1,0x1000,a,at,req(0),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  t.lower_accepted(a,1);assert(t.detach(a));
  auto b=reserve(store,0x1000,0);auto bt=tags.attach(0,b,0x1000);
  assert(a!=b && store.handle_live(a));
  assert(t.admit(1,0x1000,b,bt,req(1),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  t.lower_accepted(b,1);assert(store.inspect(a).pins==1&&store.inspect(b).pins==1);
  auto wbfree=store.preview_free(ep_l2_payload_store::WB,0);
  auto wb=store.reserve(wbfree.payload_id,ep_l2_payload_store::WB,0x2000,ep_l2_payload_store::WB_STAGED);
  assert(store.request(wb,false,1,1,ep_l2_payload_store::WB_READOUT)==ep_l2_payload_store::GRANTED);
  auto returned=data(0x1000);
  assert(!port.accept_return(t,a,1,1,returned)); // competes with actual WB bank grant
  assert(store.inspect(a).pending_sector_mask.to_ulong()==1);
  assert(port.accept_return(t,a,1,2,returned));unsigned updates=tags.updates;
  assert(!port.accept_return(t,b,1,2,returned));
  assert(port.accept_return(t,b,1,3,returned));
  assert(tags.updates==updates+1&&tags.tags[0].h==b);
  assert(store.inspect(a).allocation_role==ep_l2_payload_store::RO);
  assert(store.inspect(b).allocation_role==ep_l2_payload_store::RO && store.inspect(b).pins==1);accounting(store);
  response r;assert(t.prepare_response(a,4,r));t.commit_response(a,false,4);
  assert(store.handle_live(a)&&store.inspect(a).pins==1);
  assert(t.prepare_response(a,5,r));t.commit_response(a,true,5);
  assert(!store.handle_live(a));
  assert(t.prepare_response(b,6,r));t.commit_response(b,true,6);
  assert(store.handle_live(b)&&store.inspect(b).pins==0&&store.ro_used()==0);
  store.release(wb);assert(t.end_epoch());accounting(store);
  assert(shared.descriptor_count_used()==0&&store.pending_operations()==0);
  assert(t.stats.duplicate_sectors==1&&tags.reads==3);
  assert(store.bank_ro_fill_write()==2&&store.bank_ro_response_read()==3);
  ep_l2_payload_store static_store(2,1,16);
  assert(!static_store.preview_free(ep_l2_payload_store::RO).valid());
  bool rejected=false;try{store_port invalid(static_store,tags);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
  ep_l2_payload_store sensitivity(2,2,8);store_port s8(sensitivity,tags);
  assert(sensitivity.physical_slots()==1160);accounting(sensitivity);
  puts("EP-L2 Lane 3 actual shared-store/pin/bank/version integration: PASS");
}
