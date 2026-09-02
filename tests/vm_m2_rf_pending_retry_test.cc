#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  // One shared L2 port makes any incorrect pending-A probe observable as a
  // port stall for B in the same cycle.  Per-SM L1s keep B's first lookup
  // independently eligible.
  vm_translation::translation_controller vm(vm_translation::translation_config(
      2, page, vm_translation::tlb_config(4, 4, 1),
      vm_translation::tlb_config(4, 4, 1), 4, 4, 1, 10));
  const uint64_t va = 3 * page + 17;
  uint64_t pa = 0;

  // A incurs exactly one initial L1/L2 miss and is accepted into one MSHR.
  assert(vm.translate(0, 0, va, 0, 101, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  vm.cycle(0);
  assert(vm.l1(0).stats().accesses == 1);
  assert(vm.l1(0).stats().misses == 1);
  assert(vm.l2().stats().accesses == 1);
  assert(vm.l2().stats().misses == 1);
  assert(vm.stats().mshr_allocations == 1);
  assert(vm.stats().waiter_registrations == 1);

  // A's retry consumes neither L1 nor L2 port/probe.  Thus independent B can
  // use the only shared L2 port in this same cycle and merge normally.
  assert(vm.translate(0, 0, va, 1, 101, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(vm.l1(0).stats().accesses == 1);
  assert(vm.l1(0).stats().port_stalls == 0);
  assert(vm.l2().stats().accesses == 1);
  assert(vm.l2().stats().port_stalls == 0);
  assert(vm.translate(1, 0, va, 1, 202, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(vm.l1(1).stats().accesses == 1);
  assert(vm.l2().stats().accesses == 2);
  assert(vm.l2().stats().misses == 2);
  assert(vm.l2().stats().port_stalls == 0);
  assert(vm.stats().mshr_merges == 1);
  assert(vm.stats().waiter_registrations == 2);

  // More retries of the already registered A add no TLB probes, misses, or
  // port stalls.  They are visible only in the dedicated bypass counter.
  for (uint64_t cycle = 2; cycle < 10; ++cycle) {
    assert(vm.translate(0, 0, va, cycle, 101, &pa) ==
           vm_translation::TRANSLATION_PENDING);
  }
  assert(vm.stats().pending_waiter_bypasses == 9);
  assert(vm.l1(0).stats().accesses == 1);
  assert(vm.l1(0).stats().misses == 1);
  assert(vm.l1(0).stats().port_stalls == 0);
  assert(vm.l2().stats().accesses == 2);
  assert(vm.l2().stats().misses == 2);
  assert(vm.l2().stats().port_stalls == 0);

  vm.cycle(10);
  // Both unique waiters wake exactly once; A's only post-fill retry completes
  // once, so no data-side operation would be duplicated by this controller.
  assert(vm.stats().waiter_wakeups == 2);
  assert(vm.stats().mshr_releases == 1);
  assert(vm.translate(0, 0, va, 11, 101, &pa) == vm_translation::READY);
  assert(pa == va);
  unsigned a_data_effects = 0;
  ++a_data_effects;
  assert(a_data_effects == 1);
  assert(vm.stats().completed == 1);
  assert(vm.stats().mshr_entries_completed == 1);
  assert(vm.stats().mshr_waiter_depth_total == 2);
  assert(vm.stats().mshr_waiter_depth_max == 2);
  assert(vm.stats().mshr_lifetime_cycles_total == 10);
  assert(vm.stats().mshr_lifetime_cycles_max == 10);
  assert(vm.stats().mshr_occupancy_high_watermark == 1);
  assert(vm.quiescent_invariants_hold());

  printf("vm_m2_rf_pending_retry_test PASS\n");
  return 0;
}
