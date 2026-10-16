#include <assert.h>
#include <stdio.h>
#include <map>
#include <set>
#include "../../src/gpgpu-sim/ep-l2-ro-transactions.h"

using namespace ep_l2_ro;

static mem_fetch *request(unsigned id) {
  return reinterpret_cast<mem_fetch *>(static_cast<uintptr_t>(id + 1));
}

// Small byte/version reference adapter for the independent production module.
// Not the final Lane-1 allocator or a performance simulator.
class reference_port : public payload_port {
 public:
  struct landing {
    identity h;
    new_addr_type line;
    bool allocated;
    std::array<unsigned char,128> data;
    landing() : line(0), allocated(false) { data.fill(0); }
  };
  struct tag { identity h; new_addr_type line; uint64_t gen; unsigned ready;
    tag() : line(0), gen(0), ready(0) {} };
  std::vector<landing> slots;
  std::map<unsigned,tag> tags;
  std::array<uint64_t,4> used_cycle;
  std::array<unsigned char,128> observed;
  unsigned reads, fills, releases, tag_updates;
  reference_port() : slots(1168), reads(0), fills(0), releases(0), tag_updates(0) {
    used_cycle.fill(~0ULL); observed.fill(0);
  }
  static unsigned char memory(new_addr_type line, unsigned byte) {
    return static_cast<unsigned char>((line / 128 * 37 + byte * 13 + 7) % 251);
  }
  identity reserve(unsigned id, new_addr_type line, unsigned ti) {
    assert(!slots[id].allocated);
    landing &p=slots[id]; ++p.h.generation; p.h.id=id;
    p.line=line; p.allocated=true; p.data.fill(0);
    tag &t=tags[ti]; ++t.gen; t.h=p.h; t.line=line; t.ready=0;
    return p.h;
  }
  tag_binding binding(unsigned i) { return tag_binding(i,tags[i].gen); }
  bool live(identity h,new_addr_type line) const {
    return h.id<slots.size() && slots[h.id].allocated &&
      slots[h.id].h==h && slots[h.id].line==line;
  }
  bool bank(identity h,uint64_t cycle) {
    if(used_cycle[h.id%4]==cycle) return false;
    used_cycle[h.id%4]=cycle; return true;
  }
  bool fill_write(identity h,unsigned mask,uint64_t cycle) {
    assert(live(h,slots[h.id].line));
    if(!bank(h,cycle)) return false;
    ++fills;
    for(unsigned s=0;s<4;++s) if(mask>>s&1)
      for(unsigned b=s*32;b<(s+1)*32;++b)
        slots[h.id].data[b]=memory(slots[h.id].line,b);
    return true;
  }
  bool response_read(identity h,unsigned mask,uint64_t cycle) {
    if(!bank(h,cycle)) return false;
    ++reads;
    for(unsigned s=0;s<4;++s) if(mask>>s&1)
      for(unsigned b=s*32;b<(s+1)*32;++b) {
        observed[b]=slots[h.id].data[b];
        assert(observed[b]==memory(slots[h.id].line,b));
      }
    return true;
  }
  bool tag_matches(tag_binding b,identity h,new_addr_type line) const {
    std::map<unsigned,tag>::const_iterator t=tags.find(b.index);
    return t!=tags.end() && t->second.gen==b.generation &&
      t->second.h==h && t->second.line==line;
  }
  void tag_ready(tag_binding b,identity h,new_addr_type line,unsigned ready) {
    assert(tag_matches(b,h,line)); tags[b.index].ready=ready; ++tag_updates;
  }
  void tag_detach(tag_binding b,identity h) {
    assert(tags[b.index].h==h && tags[b.index].gen==b.generation);
    tags.erase(b.index);
  }
  void release(identity h) {
    assert(slots[h.id].allocated && slots[h.id].h==h);
    slots[h.id].allocated=false; ++releases;
  }
};

static std::string hash() { return std::string(64,'a'); }

static void out_of_order_backpressure_and_waiter_protection() {
  mshr_table shared(128,32,512,32); reference_port p;
  transactions t(shared,p,1168,switches(true,true)); t.begin_epoch(1,hash());
  identity h=p.reserve(0,0x1000,0);
  assert(t.admit(1,0x1000,h,p.binding(0),request(1),3,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  assert(t.merge(h,request(2),4)==ACCEPTED);
  assert(!t.detach(h)); // second requester protects even if one retires
  assert(t.lower_needed(h)==7); t.lower_accepted(h,7);
  assert(t.fill(h,4,1)); assert(t.pending(h)==3);
  response r;
  assert(!t.prepare_response(h,1,r)); // shared bank write/read arbitration
  assert(t.prepare_response(h,2,r)); assert(r.request==request(2));
  t.commit_response(h,false);
  unsigned reads=p.reads;
  assert(t.prepare_response(h,3,r)); assert(p.reads==reads);
  assert(shared.descriptor_count_used()==2);
  t.commit_response(h,true); assert(shared.descriptor_count_used()==1);
  assert(!t.detach(h));
  assert(t.fill(h,2,4)); assert(!t.prepare_response(h,5,r));
  assert(t.fill(h,1,6)); assert(t.prepare_response(h,7,r));
  assert(r.request==request(1)); t.commit_response(h,true);
  assert(shared.descriptor_count_used()==0 && t.drained());
  assert(p.tags[0].ready==7 && p.releases==0); // clean resident stays alive
  assert(t.end_epoch());
}

static void same_line_detached_instances_and_generation() {
  mshr_table shared(128,32,512,32); reference_port p;
  transactions t(shared,p,1168,switches(true,true));t.begin_epoch(2,hash());
  identity a=p.reserve(0,0x2000,0);
  assert(t.admit(2,0x2000,a,p.binding(0),request(10),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  t.lower_accepted(a,1); assert(t.detach(a));
  identity b=p.reserve(4,0x2000,0);
  assert(t.admit(2,0x2000,b,p.binding(0),request(11),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  t.lower_accepted(b,1); assert(t.stats.duplicate_sectors==1);
  assert(shared.num_entries_used()==0 && shared.descriptor_count_used()==2);
  assert(!t.end_epoch());
  assert(t.fill(b,1,1)); unsigned updates=p.tag_updates;
  assert(!t.fill(a,1,1)); assert(t.fill(a,1,2));
  assert(p.tag_updates==updates && p.tags[0].h==b); // old fill cannot modify B
  response r;
  assert(t.prepare_response(a,3,r));t.commit_response(a,false);
  assert(p.slots[a.id].allocated);t.commit_response(a,true);
  assert(!p.slots[a.id].allocated && p.slots[b.id].allocated);
  assert(t.prepare_response(b,4,r));t.commit_response(b,true);
  assert(t.end_epoch() && shared.descriptor_count_used()==0);
  identity fresh=p.reserve(0,0x3000,1);
  assert(fresh.generation!=a.generation);
  t.begin_epoch(3,hash());
  assert(t.admit(3,0x3000,fresh,p.binding(1),request(12),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  bool rejected=false;try{t.fill(a,1,5);}catch(const std::logic_error&){rejected=true;}
  assert(rejected);t.lower_accepted(fresh,1);assert(t.detach(fresh));
  assert(t.fill(fresh,1,5));assert(t.prepare_response(fresh,6,r));t.commit_response(fresh,true);
  assert(t.end_epoch());
}

static void descriptor_pool_is_shared_and_address_bounded() {
  mshr_table shared(128,32,512,32); reference_port p;
  for(unsigned n=0;n<511;++n)
    shared.add_for_test(0x100000+(n/32)*128,request(100+n),mem_access_sector_mask_t(1));
  transactions t(shared,p,1168,switches(true,true));t.begin_epoch(4,hash());
  identity a=p.reserve(0,0x4000,0);
  assert(t.admit(4,0x4000,a,p.binding(0),request(800),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  identity b=p.reserve(1,0x5000,1);
  assert(t.admit(4,0x5000,b,p.binding(1),request(801),1,true,GLOBAL_DEMAND_READ)==DESCRIPTOR_OR_ADDRESS_FULL);
  assert(shared.descriptor_count_used()==512 && shared.num_entries_used()==16);
  t.lower_accepted(a,1);assert(t.detach(a));assert(t.fill(a,1,1));response r;
  assert(t.prepare_response(a,2,r));t.commit_response(a,true);
  assert(shared.descriptor_count_used()==511);
  assert(t.admit(4,0x5000,b,p.binding(1),request(801),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);

  mshr_table per_line(128,32,512,32);reference_port p2;
  transactions t2(per_line,p2,1168,switches(true,true));t2.begin_epoch(5,hash());
  identity h=p2.reserve(0,0x6000,0);
  assert(t2.admit(5,0x6000,h,p2.binding(0),request(900),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  for(unsigned n=0;n<31;++n)assert(t2.merge(h,request(901+n),1)==ACCEPTED);
  assert(t2.merge(h,request(999),1)==DESCRIPTOR_OR_ADDRESS_FULL);
  assert(per_line.full_reason(0x6000)==mshr_table::EP_L2_BLOCK_PER_ADDRESS_CAP);
  assert(per_line.descriptor_count_used()==32);
}

static void rejected_inputs_and_illegal_protocol() {
  mshr_table shared(128,32,512,32);reference_port p;
  transactions off(shared,p,1168);off.begin_epoch(1,hash());
  identity h=p.reserve(0,0x7000,0);
  assert(off.admit(1,0x7000,h,p.binding(0),request(0),1,true,GLOBAL_DEMAND_READ)==NORMAL_PATH);
  assert(shared.descriptor_count_used()==0);
  transactions t(shared,p,1168,switches(false,true));t.begin_epoch(6,hash());
  for(unsigned kind=SYNTHETIC_WRITE_ALLOCATE_READ;kind<=UNKNOWN_REQUEST;++kind)
    assert(t.admit(6,0x7000,h,p.binding(0),request(0),1,true,request_kind(kind))==NORMAL_PATH);
  assert(t.admit(6,0x7000,h,p.binding(0),request(0),1,false,GLOBAL_DEMAND_READ)==NORMAL_PATH);
  assert(t.admit(7,0x7000,h,p.binding(0),request(0),1,true,GLOBAL_DEMAND_READ)==NORMAL_PATH);
  assert(t.admit(6,0x7000,h,p.binding(0),request(0),1,true,GLOBAL_DEMAND_READ)==ACCEPTED);
  assert(!t.detach(h));
  bool violation=false;try{t.assert_no_epoch_write(6,0x7000);}catch(const std::logic_error&){violation=true;}assert(violation);
  violation=false;try{t.fill(h,1,1);}catch(const std::logic_error&){violation=true;}assert(violation);
  t.lower_accepted(h,1);
  violation=false;try{t.lower_accepted(h,1);}catch(const std::logic_error&){violation=true;}assert(violation);
  assert(t.fill(h,1,2));
  violation=false;try{t.fill(h,1,3);}catch(const std::logic_error&){violation=true;}assert(violation);
  response r;assert(t.prepare_response(h,4,r));t.commit_response(h,true);
  assert(t.end_epoch());

  mshr_table::ro_descriptor_handle d,stale;
  assert(shared.reserve_ro_descriptor(0x8000,request(1),mem_access_sector_mask_t(1),d));
  stale=d;shared.release_ro_descriptor(d);
  assert(shared.reserve_ro_descriptor(0x8000,request(1),mem_access_sector_mask_t(1),d));
  assert(d.id==stale.id && d.generation!=stale.generation);
  assert(!shared.ro_descriptor_live(stale));shared.release_ro_descriptor(d);
}

int main() {
  out_of_order_backpressure_and_waiter_protection();
  same_line_detached_instances_and_generation();
  descriptor_pool_is_shared_and_address_bounded();
  rejected_inputs_and_illegal_protocol();
  puts("EP-L2 Lane 3 RO production-module/reference tests: PASS (4 scenario groups)");
}
