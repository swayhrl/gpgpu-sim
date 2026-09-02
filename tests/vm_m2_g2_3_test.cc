#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  uint64_t pa = 0;
  vm_translation::translation_controller vm(vm_translation::translation_config(
      1, page, vm_translation::tlb_config(8, 8, 8),
      vm_translation::tlb_config(8, 8, 8), 4, 2, 1, 3));
  assert(vm.translate(0, 0, 0, 0, 1, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(vm.translate(0, 0, page, 0, 2, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(vm.translate(0, 0, 2 * page, 0, 3, &pa) ==
         vm_translation::PWQ_FULL);
  vm.cycle(0);
  assert(vm.active_walkers() == 1);
  vm.cycle(2);
  assert(vm.stats().walk_completions == 0);
  vm.cycle(3);
  assert(vm.stats().walk_completions == 1);
  assert(vm.active_walkers() == 1);
  vm.cycle(6);
  assert(vm.stats().walk_starts == 2);
  assert(vm.stats().walk_completions == 2);
  assert(vm.stats().pwq_full_events == 1);
  assert(vm.quiescent_invariants_hold());
  printf("vm_m2_g2_3_test PASS\n");
  return 0;
}
