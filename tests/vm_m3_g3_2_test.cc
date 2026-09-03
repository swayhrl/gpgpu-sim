#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

static vm_translation::pte_request issue_next(
    vm_translation::translation_controller *controller, uint64_t cycle) {
  vm_translation::pte_request request;
  assert(controller->next_pte_request(&request));
  assert(request.is_physical && request.bypass_translation);
  assert(controller->pte_request_issued(request, cycle));
  return request;
}

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  vm_translation::translation_controller controller(
      vm_translation::translation_config(
          2, page, vm_translation::tlb_config(2, 2, 2),
          vm_translation::tlb_config(4, 2, 2), 4, 4, 2, 1,
          vm_translation::page_table_config(), 1,
          vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1)));

  uint64_t pa_a = 0;
  uint64_t pa_b = 0;
  assert(controller.uses_real_memory_ptw());
  assert(controller.translate(0, 0, page + 3, 0, 11, &pa_a) ==
         vm_translation::TRANSLATION_PENDING);
  assert(controller.translate(1, 0, 2 * page + 7, 0, 12, &pa_b) ==
         vm_translation::TRANSLATION_PENDING);
  controller.cycle(0);
  assert(controller.active_walkers() == 2);

  // Both walkers issue their first PTE request.  Neither may complete before
  // the matching response, and the request identity permits out-of-order
  // completion of independent walks.
  vm_translation::pte_request a = issue_next(&controller, 1);
  vm_translation::pte_request b = issue_next(&controller, 1);
  assert(a.key.vpn != b.key.vpn && a.request_id != b.request_id);
  controller.cycle(100);
  assert(controller.translate(0, 0, page + 3, 100, 11, &pa_a) ==
         vm_translation::TRANSLATION_PENDING);

  // Finish B before A, alternating L2-only and lower-memory completions.
  for (unsigned level = 0; level < 4; ++level) {
    assert(b.level == level);
    assert(controller.complete_pte_response(b.request_id, b.physical_address,
                                            (level & 1) != 0,
                                            level == 0 ? 10 : 20 + level));
    if (level + 1 < 4) b = issue_next(&controller, 20 + level);
  }
  assert(controller.translate(1, 0, 2 * page + 7, 30, 12, &pa_b) ==
         vm_translation::READY);
  assert(pa_b == 2 * page + 7);
  assert(controller.translate(0, 0, page + 3, 30, 11, &pa_a) ==
         vm_translation::TRANSLATION_PENDING);

  for (unsigned level = 0; level < 4; ++level) {
    assert(a.level == level);
    assert(controller.complete_pte_response(a.request_id, a.physical_address,
                                            (level & 1) == 0, 50 + level));
    if (level + 1 < 4) a = issue_next(&controller, 50 + level);
  }
  assert(controller.translate(0, 0, page + 3, 60, 11, &pa_a) ==
         vm_translation::READY);
  assert(pa_a == page + 3);
  assert(controller.quiescent_invariants_hold());
  assert(controller.stats().pte_requests == 8);
  assert(controller.stats().pte_responses == 8);
  assert(controller.stats().pte_l2_only_responses == 4);
  assert(controller.stats().pte_dram_responses == 4);
  assert(controller.stats().pte_response_misassociations == 0);

  printf("vm_m3_g3_2_test PASS\n");
  return 0;
}
