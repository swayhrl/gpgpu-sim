#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

using vm_translation::MSHR_FULL;
using vm_translation::TRANSLATION_PENDING;
using vm_translation::translation_config;
using vm_translation::translation_controller;
using vm_translation::translation_key;
using vm_translation::tlb_config;

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  uint64_t pa = 0;

  // Two distinct waiters for one key allocate one entry and merge once.
  translation_controller merge_vm(
      translation_config(2, page, tlb_config(4, 4, 8), tlb_config(4, 4, 8),
                         2));
  assert(merge_vm.translate(0, 0, 3 * page, 0, 100, &pa) ==
         TRANSLATION_PENDING);
  assert(merge_vm.translate(1, 0, 3 * page + 4, 1, 200, &pa) ==
         TRANSLATION_PENDING);
  // Replay of waiter 100 must not become a second registration or merge.
  assert(merge_vm.translate(0, 0, 3 * page, 2, 100, &pa) ==
         TRANSLATION_PENDING);
  assert(merge_vm.stats().mshr_allocations == 1);
  assert(merge_vm.stats().mshr_merges == 1);
  assert(merge_vm.stats().waiter_registrations == 2);
  assert(merge_vm.l1(0).stats().hits == 0);
  assert(merge_vm.invariants_hold());
  assert(merge_vm.complete_translation(translation_key(0, 3, page), 3));
  assert(merge_vm.stats().waiter_wakeups == 2);
  assert(merge_vm.stats().mshr_releases == 1);
  assert(merge_vm.quiescent_invariants_hold());
  assert(merge_vm.translate(0, 0, 3 * page, 4, 100, &pa) ==
         vm_translation::READY);
  assert(pa == 3 * page);

  // A finite one-entry MSHR backpressures without losing the later request.
  translation_controller full_vm(
      translation_config(1, page, tlb_config(4, 4, 8), tlb_config(4, 4, 8),
                         1));
  assert(full_vm.translate(0, 0, 0, 0, 1, &pa) == TRANSLATION_PENDING);
  assert(full_vm.translate(0, 0, page, 1, 2, &pa) == MSHR_FULL);
  assert(full_vm.stats().mshr_full_events == 1);
  assert(full_vm.active_mshrs() == 1);
  assert(full_vm.invariants_hold());
  assert(full_vm.complete_translation(translation_key(0, 0, page), 2));
  assert(full_vm.translate(0, 0, page, 3, 2, &pa) == TRANSLATION_PENDING);
  assert(full_vm.complete_translation(translation_key(0, 1, page), 4));
  assert(full_vm.quiescent_invariants_hold());

  printf("vm_m2_g2_2_test PASS\n");
  return 0;
}
