#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  vm_translation::translation_controller vm(vm_translation::translation_config(
      1, page, vm_translation::tlb_config(4, 4, 1),
      vm_translation::tlb_config(4, 4, 1), 2, 2, 1, 1));
  const uint64_t va = 5 * page + 31;
  uint64_t pa = 0;

  // Kernel 1 warms this translation and reaches a normal quiescent boundary.
  assert(vm.translate(0, 0, va, 0, 11, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  vm.cycle(0);
  vm.cycle(1);
  assert(vm.quiescent_invariants_hold());
  const uint64_t l2_accesses_at_boundary = vm.l2().stats().accesses;
  assert(l2_accesses_at_boundary == 1);

  // Ordinary kernel boundary: the simulator retains this controller object.
  // Kernel 2's new transaction uses the retained L1 translation, with no new
  // L2 access or walk.
  assert(vm.translate(0, 0, va, 2, 22, &pa) == vm_translation::READY);
  assert(pa == va);
  assert(vm.l1(0).stats().hits == 1);
  assert(vm.l2().stats().accesses == l2_accesses_at_boundary);
  assert(vm.stats().mshr_allocations == 1);
  assert(vm.stats().walk_completions == 1);
  assert(vm.quiescent_invariants_hold());

  printf("vm_m2_rf_kernel_persistence_test PASS\n");
  return 0;
}
