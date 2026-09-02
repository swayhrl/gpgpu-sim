#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

static vm_translation::translation_config config_for(
    uint64_t page_size, const vm_translation::pwc_config &pwc,
    unsigned tlb_entries = 16) {
  return vm_translation::translation_config(
      1, page_size, vm_translation::tlb_config(tlb_entries, tlb_entries, 4),
      vm_translation::tlb_config(tlb_entries, tlb_entries, 4), 8, 8, 1, 1,
      vm_translation::page_table_config(), 1, pwc);
}

// This is a directed model of the accepted G3 real-PTE issue/response seam:
// each emitted PTE remains physical/nonrecursive and receives an explicitly
// matched response.  The controller's one-cycle PWC probes are advanced by
// cycle(), not by this helper.
static uint64_t run_walk(vm_translation::translation_controller *controller,
                         uint64_t sim_va, uint64_t waiter_uid,
                         uint64_t start_cycle) {
  uint64_t sim_pa = 0;
  assert(controller->translate(0, 0, sim_va, start_cycle, waiter_uid,
                               &sim_pa) ==
         vm_translation::TRANSLATION_PENDING);
  for (uint64_t cycle = start_cycle; cycle < start_cycle + 128; ++cycle) {
    controller->cycle(cycle);
    vm_translation::pte_request request;
    while (controller->next_pte_request(&request)) {
      assert(request.is_physical && request.bypass_translation);
      assert(controller->pte_request_issued(request, cycle));
      assert(controller->complete_pte_response(
          request.request_id, request.physical_address, false, cycle));
    }
    if (controller->active_mshrs() == 0) {
      assert(controller->translate(0, 0, sim_va, cycle + 1, waiter_uid,
                                   &sim_pa) == vm_translation::READY);
      assert(sim_pa == sim_va);
      assert(controller->quiescent_invariants_hold());
      return cycle + 2;
    }
  }
  assert(false && "directed PWC walk did not complete");
  return 0;
}

int main() {
  const uint64_t page64k = 64ULL * 1024ULL;
  const uint64_t page2m = 2ULL * 1024ULL * 1024ULL;
  const uint64_t upper30 = 0x1234567ULL;
  const uint64_t a = ((upper30 << 10) | 1) * page64k + 3;
  const uint64_t b = ((upper30 << 10) | 2) * page64k + 7;

  // vm_pwc_zero: OFF has no cache state and takes all four physical PTE
  // levels for each cold related walk.
  vm_translation::translation_controller off(
      config_for(page64k,
                 vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1)));
  uint64_t cycle = run_walk(&off, a, 1, 0);
  run_walk(&off, b, 2, cycle);
  assert(off.stats().pte_requests == 8);
  assert(off.stats().pte_responses == 8);
  assert(off.stats().pwc_accesses == 0 && off.stats().pwc_occupancy == 0);

  // vm_pwc_warm + vm_pwc_no_leaf: the same upper 30 bits hit all three
  // intermediate identities.  The distinct leaf still makes one physical
  // request, so this is 5 rather than 8 and never a leaf-PWC hit.
  vm_translation::translation_controller finite(
      config_for(page64k,
                 vm_translation::pwc_config(vm_translation::PWC_FINITE, 128,
                                            1)));
  cycle = run_walk(&finite, a, 11, 0);
  run_walk(&finite, b, 12, cycle);
  const vm_translation::translation_stats &warm = finite.stats();
  assert(warm.pte_requests == 5 && warm.pte_responses == 5);
  assert(warm.pwc_accesses == 6 && warm.pwc_hits == 3 &&
         warm.pwc_misses == 3 && warm.pwc_inserts == 3 &&
         warm.pwc_evictions == 0 && warm.pwc_occupancy == 3);
  for (unsigned level = 0; level < 3; ++level) {
    assert(warm.pwc_hits_by_level[level] == 1);
    assert(warm.pwc_pte_requests_skipped_by_level[level] == 1);
  }
  assert(warm.pwc_accesses_by_level[3] == 0 &&
         warm.pwc_pte_requests_skipped_by_level[3] == 0);

  // vm_pwc_partial_share: only the root prefix is shared, therefore only
  // level 0 hits on the second walk and it emits three physical PTE requests.
  const uint64_t partial_a = ((5ULL << 30) | (1ULL << 20)) * page64k;
  const uint64_t partial_b = ((5ULL << 30) | (2ULL << 20)) * page64k;
  vm_translation::translation_controller partial(
      config_for(page64k,
                 vm_translation::pwc_config(vm_translation::PWC_FINITE, 128,
                                            1)));
  cycle = run_walk(&partial, partial_a, 21, 0);
  run_walk(&partial, partial_b, 22, cycle);
  assert(partial.stats().pte_requests == 7);
  assert(partial.stats().pwc_hits == 1);
  assert(partial.stats().pwc_hits_by_level[0] == 1);
  assert(partial.stats().pwc_hits_by_level[1] == 0 &&
         partial.stats().pwc_hits_by_level[2] == 0);

  // vm_pwc_capacity_lru: capacity three retains A's three intermediate
  // entries.  An unrelated C replaces them in deterministic LRU order; a
  // later B walk has no surviving intermediate hit.
  const uint64_t unrelated = (6ULL << 30) * page64k;
  vm_translation::translation_controller lru(
      config_for(page64k,
                 vm_translation::pwc_config(vm_translation::PWC_FINITE, 3,
                                            1),
                 1));
  cycle = run_walk(&lru, a, 31, 0);
  cycle = run_walk(&lru, b, 32, cycle);
  cycle = run_walk(&lru, unrelated, 33, cycle);
  run_walk(&lru, b, 34, cycle);
  assert(lru.stats().pte_requests == 13);
  assert(lru.stats().pwc_hits == 3 && lru.stats().pwc_evictions == 6);
  assert(lru.stats().pwc_occupancy == 3 &&
         lru.stats().pwc_occupancy_high_watermark == 3);

  // IDEAL is unbounded/no-eviction and has the same logical intermediate
  // sharing as the finite warm case.
  vm_translation::translation_controller ideal(
      config_for(page64k,
                 vm_translation::pwc_config(vm_translation::PWC_IDEAL, 0,
                                            1)));
  cycle = run_walk(&ideal, a, 41, 0);
  run_walk(&ideal, b, 42, cycle);
  assert(ideal.stats().pte_requests == 5 && ideal.stats().pwc_hits == 3 &&
         ideal.stats().pwc_evictions == 0);

  // vm_pwc_2mb: a page-size-class-specific 2MB hierarchy has [8,9,9,9], so
  // equal upper 26 VPN bits share only intermediate identities.
  const uint64_t upper26 = 0x1abcdeULL;
  const uint64_t large_a = ((upper26 << 9) | 1) * page2m + 11;
  const uint64_t large_b = ((upper26 << 9) | 2) * page2m + 13;
  vm_translation::translation_controller large(
      config_for(page2m,
                 vm_translation::pwc_config(vm_translation::PWC_FINITE, 128,
                                            1)));
  cycle = run_walk(&large, large_a, 51, 0);
  run_walk(&large, large_b, 52, cycle);
  assert(large.stats().pte_requests == 5 && large.stats().pwc_hits == 3 &&
         large.stats().pwc_hits_by_level[0] == 1 &&
         large.stats().pwc_hits_by_level[1] == 1 &&
         large.stats().pwc_hits_by_level[2] == 1);

  printf("vm_m3_g3_3_pwc_test PASS\n");
  return 0;
}
