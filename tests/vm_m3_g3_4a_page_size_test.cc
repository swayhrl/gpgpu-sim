#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

static uint64_t complete_fixed_walk(vm_translation::translation_controller *vm,
                                    uint64_t start_cycle) {
  for (uint64_t cycle = start_cycle; cycle < start_cycle + 32; ++cycle) {
    vm->cycle(cycle);
    if (vm->active_mshrs() == 0) return cycle;
  }
  assert(false && "fixed walk did not complete");
  return 0;
}

static void verify_page_size(uint64_t page_size, uint64_t base) {
  vm_translation::translation_controller vm(vm_translation::translation_config(
      1, page_size, vm_translation::tlb_config(4, 4, 2),
      vm_translation::tlb_config(4, 4, 2), 4, 4, 1, 2));
  const uint64_t first = base + page_size - 1;
  const uint64_t same_page = base + 17;
  const uint64_t next_page = base + page_size + 9;
  uint64_t pa = 0;

  assert(vm.translate(0, 0, first, 0, 1, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  const uint64_t finished = complete_fixed_walk(&vm, 0);
  assert(vm.translate(0, 0, first, finished + 1, 2, &pa) ==
         vm_translation::READY);
  assert(pa == first);

  // One filled entry covers exactly the configured page, including both
  // offsets, and cannot cover the first address of the following page.
  assert(vm.translate(0, 0, same_page, finished + 2, 3, &pa) ==
         vm_translation::READY);
  assert(pa == same_page);
  assert(vm.translate(0, 0, next_page, finished + 3, 4, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  complete_fixed_walk(&vm, finished + 3);
  assert(vm.quiescent_invariants_hold());
  assert(vm.l1(0).stats().misses == 2 && vm.l1(0).stats().hits == 2);
  assert(vm.l2().stats().misses == 2);
  assert(vm.config().page_size == page_size);
}

int main() {
  const uint64_t page64k = 64ULL * 1024ULL;
  const uint64_t page2m = 2ULL * 1024ULL * 1024ULL;
  verify_page_size(page64k, 3 * page64k);
  verify_page_size(page2m, 5 * page2m);
  printf("vm_m3_g3_4a_page_size_test PASS\n");
  return 0;
}
