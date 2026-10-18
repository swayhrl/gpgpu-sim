// AG3 finite CPU interleavings only; no GPU, instruction payload, or performance.
#include "dtc-l1-common.h"
#include <iostream>
#ifdef NDEBUG
#error Assertions required
#endif
int main() {
  using namespace dtc_l1;
  for (bool hash : {false,true}) for (bool progressive : {false,true}) {
    config c;c.selected_mode=mode::PAPER_OO;c.finite_service=true;
    c.tag_hash_bankrow=hash;c.progressive_wakeup=progressive;
    c.logical_sets=64;c.logical_ways=8;
    oo_frontend f(c);physical_identity old{},fresh{};
    for(unsigned u=1;u<=12;++u) {
      f.advance_service(2*u);assert(f.try_serve_tag(2*u,0));assert(f.admit(u));
      f.advance_service(2*u+1);auto r=f.access(2*u+1,u,0);
      if(u==1){old=r.physical;assert(r.kind==io_access_kind::NEW_MISS);}
      else assert(r.kind==io_access_kind::PENDING_HIT);
    }
    f.advance_service(26);f.complete(old);
    bool retired[15]={};unsigned count=0;uint64_t previous=0;
    for(uint64_t cycle=27;cycle<=64;++cycle) {
      f.advance_service(cycle);
      assert(f.wakeups()-previous<=4);previous=f.wakeups();
      f.advance_service(cycle);assert(f.wakeups()==previous);
      if(cycle==27) {
        unsigned ready=0;for(unsigned u=1;u<=12;++u)ready+=f.entry_ready_for(u);
        assert(ready==(progressive?4U:0U));
      }
      if(cycle==28) {
        assert(f.try_serve_tag(cycle,0));assert(f.admit(13));
        assert(f.access(cycle,13,0).kind==io_access_kind::PENDING_HIT);
        assert(!f.entry_ready_for(13));
        auto event=f.invalidate_lookup(cycle);assert(event.changed==1&&event.retained==1);
        assert(f.fill_identity_matches(old)&&!f.tag_visible(old));
      }
      if(cycle==29) {
        assert(f.try_serve_tag(cycle,0));assert(f.admit(14));
        auto r=f.access(cycle,14,0);assert(r.kind==io_access_kind::NEW_MISS);
        fresh=r.physical;assert(fresh.id!=old.id);f.complete(fresh);
      }
      for(unsigned u=1;u<=14;++u)if(!retired[u]&&f.entry_ready_for(u)) {
        if(f.retire_ready_uid(cycle,u)){retired[u]=true;++count;}
      }
      f.assert_shadow_refs();
    }
    assert(count==14&&f.active_refs()==0&&f.occupancy()==0);
    assert(!f.is_allocated(old));assert(f.is_allocated(fresh));
    f.invalidate_lookup(65);assert(f.allocated_lines()==0);
    assert(f.wakeups()==14&&f.new_misses()==2&&f.pending_hits()==12);
    std::cout<<"PASS AG3 hash="<<hash<<" progressive="<<progressive
             <<" quota4 plus1 late_slot_reuse K1_generation Ref_closed retired=14\n";
  }
}
