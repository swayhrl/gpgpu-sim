// Built against both fixed X2 and candidate production headers in one build.
#include "dtc-l1-common.h"
#include <cstdio>
int main() {
  for (bool finite : {false, true}) {
    dtc_l1::config c; c.finite_service=finite; c.logical_sets=c.logical_ways=1;
    c.physical_lines=2; c.oo_pib_entries=16;
    dtc_l1::oo_frontend f(c); f.advance_service(1);
    dtc_l1::physical_identity a{};
    for (unsigned uid=1;uid<=9;++uid) { assert(f.admit(uid)); a=f.access(1,uid,0).physical; }
    f.complete(a);
    for (unsigned cycle=1;cycle<=4;++cycle) {
      f.advance_service(cycle);
      std::printf("finite=%u cycle=%u wakes=%llu refs=%llu ready=",finite,cycle,
          (unsigned long long)f.wakeups(),(unsigned long long)f.ref_count(a));
      for(unsigned uid=1;uid<=9;++uid) std::printf("%u",f.entry_ready_for(uid));
      std::puts("");
    }
    f.add_external_dependency(1); assert(!f.entry_ready_for(1));
    f.complete_external_dependency(1);
    auto lost=f.invalidate_lookup(5); assert(lost.retained==1);
    assert(f.admit(10)); auto b=f.access(5,10,0).physical; f.complete(b);
    f.advance_service(6);
    for(unsigned uid=1;uid<=10;++uid) assert(f.retire_ready_uid(10+uid,uid));
    f.invalidate_lookup(25);
    std::printf("end finite=%u allocated=%zu refs=%llu wakes=%llu retires=%llu reclaims=%llu sum=%llu count=%llu\n",
        finite,f.allocated_lines(),(unsigned long long)f.active_refs(),
        (unsigned long long)f.wakeups(),(unsigned long long)f.retires(),
        (unsigned long long)f.final_ref_reclaims(),(unsigned long long)f.map_loss_to_free_sum(),
        (unsigned long long)f.map_loss_to_free_count());
  }
}
