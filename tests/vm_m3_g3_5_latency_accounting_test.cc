#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

static vm_translation::translation_config fixed_config() {
  const uint64_t page = 64ULL * 1024ULL;
  return vm_translation::translation_config(
      1, page, vm_translation::tlb_config(1, 1, 1),
      vm_translation::tlb_config(8, 8, 1), 8, 8, 1, 20,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 2, 3);
}

// Drive one physical walk with exactly one deliberate four-cycle PTE-memory
// interval.  A related second walk must receive the three intermediate PWC
// hits established by the first walk.
static uint64_t run_real_walk(vm_translation::translation_controller *vm,
                              uint64_t va, uint64_t uid,
                              uint64_t start, bool delay_first) {
  uint64_t pa = 0;
  assert(vm->translate(0, 0, va, start, uid, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  bool outstanding = false;
  bool delayed = false;
  uint64_t due = 0;
  vm_translation::pte_request request;
  for (uint64_t cycle = start; cycle < start + 256; ++cycle) {
    vm->cycle(cycle);
    if (outstanding && due <= cycle) {
      assert(vm->complete_pte_response(request.request_id,
                                       request.physical_address, false, cycle));
      outstanding = false;
    }
    if (!outstanding && vm->next_pte_request(&request)) {
      assert(request.is_physical && request.bypass_translation);
      assert(vm->pte_request_issued(request, cycle));
      outstanding = true;
      due = cycle + ((!delayed && delay_first) ? 4 : 0);
      delayed = true;
      if (due == cycle) {
        assert(vm->complete_pte_response(request.request_id,
                                         request.physical_address, false,
                                         cycle));
        outstanding = false;
      }
    }
    if (vm->active_mshrs() == 0) {
      assert(vm->translate(0, 0, va, cycle + 1, uid, &pa) ==
             vm_translation::READY);
      assert(pa == va);
      return cycle + 2;
    }
  }
  assert(false && "real directed walk did not complete");
  return 0;
}

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  uint64_t pa = 0;

  // Known fixed-latency timeline: A joins its MSHR at c5 and completes at
  // c25; B begins at c6, joins at c11, and shares the same completion.
  vm_translation::translation_controller fixed(fixed_config());
  assert(fixed.translate(0, 0, 8 * page + 1, 0, 1, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  for (uint64_t cycle = 0; cycle <= 5; ++cycle) fixed.cycle(cycle);
  assert(fixed.active_mshrs() == 1);
  assert(fixed.translate(0, 0, 8 * page + 7, 6, 2, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  for (uint64_t cycle = 6; cycle <= 25; ++cycle) fixed.cycle(cycle);
  const vm_translation::translation_stats &shared = fixed.stats();
  assert(shared.requester_completions == 2);
  assert(shared.requester_latency_cycles_total == 25 + 19);
  assert(shared.requester_latency_cycles_max == 25);
  assert(shared.requester_l1_queue_cycles_total == 0);
  assert(shared.requester_l1_service_cycles_total == 2 + 2);
  assert(shared.requester_l2_queue_cycles_total == 0);
  assert(shared.requester_l2_service_cycles_total == 3 + 3);
  assert(shared.requester_mshr_wait_cycles_total == 20 + 14);
  assert(shared.mshr_entries_completed == 1 && shared.mshr_merges == 1);
  assert(shared.mshr_lifetime_cycles_total == 20);

  // An L1 hit is ready at exactly c32: its critical-path service is two
  // cycles and it does not inherit shared-walk latency.
  assert(fixed.translate(0, 0, 8 * page + 9, 30, 3, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  fixed.cycle(30);
  fixed.cycle(31);
  fixed.cycle(32);
  assert(fixed.translate(0, 0, 8 * page + 9, 32, 3, &pa) ==
         vm_translation::READY);
  assert(pa == 8 * page + 9);
  assert(fixed.stats().requester_completions == 3);
  assert(fixed.stats().requester_latency_cycles_total == 25 + 19 + 2);

  // PWC is observable independently from the PTE memory interval.  The
  // first related walk has four PTE requests; the second has exactly one leaf
  // request after three intermediate PWC hits.  Only the first response was
  // delayed, so the memory-wait accounting is exactly four cycles and is not
  // multiplied by later PWC sharing.
  vm_translation::translation_controller real(
      vm_translation::translation_config(
          1, page, vm_translation::tlb_config(8, 8, 4),
          vm_translation::tlb_config(8, 8, 4), 8, 8, 1, 1,
          vm_translation::page_table_config(), 1,
          vm_translation::pwc_config(vm_translation::PWC_FINITE, 128, 1),
          0, 0));
  const uint64_t high = 0x1234567ULL;
  const uint64_t first = ((high << 10) | 1) * page + 3;
  const uint64_t second = ((high << 10) | 2) * page + 5;
  uint64_t cycle = run_real_walk(&real, first, 11, 0, true);
  run_real_walk(&real, second, 12, cycle, false);
  const vm_translation::translation_stats &pwc = real.stats();
  assert(pwc.pte_requests == 5 && pwc.pte_responses == 5);
  assert(pwc.pwc_hits == 3 && pwc.pwc_pte_requests_skipped_by_level[0] == 1 &&
         pwc.pwc_pte_requests_skipped_by_level[1] == 1 &&
         pwc.pwc_pte_requests_skipped_by_level[2] == 1);
  assert(pwc.pte_memory_wait_cycles_total == 4 &&
         pwc.pte_memory_wait_cycles_max == 4);
  assert(real.quiescent_invariants_hold());

  printf("vm_m3_g3_5_latency_accounting_test PASS\n");
  return 0;
}
