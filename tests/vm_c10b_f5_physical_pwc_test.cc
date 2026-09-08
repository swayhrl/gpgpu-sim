#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

// C10B-4 directed F5 model test. This is a controller-level configuration
// and timing proof, not a performance replay.
static const uint64_t kPage = 64ULL * 1024ULL;

static vm_translation::translation_config f5_config() {
  vm_translation::translation_config config(
      1, kPage, vm_translation::tlb_config(8, 8, 2),
      vm_translation::tlb_config(800, 16, 2), 16, 16, 2, 1,
      vm_translation::page_table_config(), 1,
      // configure_fair_arm() preserves this explicit F5 timing point while
      // overwriting the generic PWC mode/geometry.
      vm_translation::pwc_config(vm_translation::PWC_FINITE, 128, 2), 0, 0);
  assert(vm_translation::configure_fair_arm(
      &config, vm_translation::FAIR_ARM_F5_PHYSICAL_PWC));
  assert(config.valid());
  assert(config.page_size == kPage && config.page_table.levels == 4 &&
         config.page_table.virtual_address_bits == 49);
  assert(config.l2.entries == 656 && config.l2.assoc == 16 &&
         config.l2.sets() == 41 &&
         vm_translation::fair_arm_charged_bits(config) == 64745);
  assert(config.pwc.mode == vm_translation::PWC_PHYSICAL_F5 &&
         config.pwc.entries == 120 && config.pwc.lookup_latency == 2);
  return config;
}

static void issue(vm_translation::translation_controller *controller,
                  uint64_t va, uint64_t uid, uint64_t cycle) {
  uint64_t pa = 0;
  assert(controller->translate(0, 0, va, 32, cycle, uid, &pa) ==
         vm_translation::TRANSLATION_PENDING);
}

static uint64_t drain(vm_translation::translation_controller *controller,
                      uint64_t start_cycle) {
  for (uint64_t cycle = start_cycle; cycle < start_cycle + 1024; ++cycle) {
    controller->cycle(cycle);
    vm_translation::pte_request request;
    while (controller->next_pte_request(&request)) {
      assert(request.is_physical && request.bypass_translation);
      assert(controller->pte_request_issued(request, cycle));
      assert(controller->complete_pte_response(
          request.request_id, request.physical_address, false, cycle));
    }
    if (controller->active_mshrs() == 0 && controller->active_walkers() == 0)
      return cycle + 1;
  }
  assert(false && "F5 directed walk did not drain");
  return 0;
}

static void complete(vm_translation::translation_controller *controller,
                     uint64_t va, uint64_t uid, uint64_t cycle) {
  uint64_t pa = 0;
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  assert(controller->translate(0, 0, va, 32, cycle, uid, &pa, &source) ==
         vm_translation::READY);
  assert(pa == va && source == vm_translation::TRANSLATION_SOURCE_PTW);
}

int main() {
  vm_translation::translation_controller controller(f5_config());

  // A 49-bit, 64KiB hierarchy has [6,9,9,9] radix widths. A/B share all
  // three non-leaf identities. They arrive together, so the explicit F5
  // one-port queue must defer one PWC request instead of implicitly probing
  // both in the same cycle.
  const uint64_t prefix24 =
      (0x15ULL << 27) | (0x1aULL << 18) | (0x10ULL << 9);
  const uint64_t a = (prefix24 | 1) * kPage + 5;
  const uint64_t b = (prefix24 | 2) * kPage + 7;
  issue(&controller, a, 1, 0);
  issue(&controller, b, 2, 0);
  uint64_t cycle = drain(&controller, 0);
  complete(&controller, a, 1, cycle++);
  complete(&controller, b, 2, cycle++);
  const vm_translation::translation_stats &cold = controller.stats();
  assert(cold.pwc_port_accepts >= 6 && cold.pwc_port_denials != 0 &&
         cold.pwc_queue_wait_cycles_total != 0 &&
         cold.pwc_queue_high_watermark != 0);
  assert(cold.pwc_accesses == cold.pwc_hits + cold.pwc_misses &&
         cold.pwc_misses >= 3 && cold.pwc_occupancy_by_level[0] == 1 &&
         cold.pwc_occupancy_by_level[1] == 1 &&
         cold.pwc_occupancy_by_level[2] == 1);

  // C has the same C9 non-leaf prefixes but a distinct leaf. Its three F5
  // hits validate stored next-table PPN+attributes before suppressing those
  // three PTE requests; only the leaf PTE is issued.
  const uint64_t c = (prefix24 | 3) * kPage + 9;
  const uint64_t pte_before = controller.stats().pte_requests;
  const uint64_t payload_before = controller.stats().pwc_physical_payload_validations;
  issue(&controller, c, 3, cycle++);
  cycle = drain(&controller, cycle);
  complete(&controller, c, 3, cycle++);
  assert(controller.stats().pte_requests == pte_before + 1);
  assert(controller.stats().pwc_physical_payload_validations ==
         payload_before + 3);

  // Five independent L0 prefixes map to one of the ten four-way physical
  // sets (prefix modulo ten for ASID zero). This directly exercises the
  // C9 4-way tree-PLRU replacement path rather than legacy vector LRU.
  for (uint64_t root = 0; root <= 40; root += 10) {
    const uint64_t va = (root << 27) * kPage + 13;
    issue(&controller, va, 10 + root, cycle++);
    cycle = drain(&controller, cycle);
    complete(&controller, va, 10 + root, cycle++);
  }
  assert(controller.stats().pwc_evictions != 0);
  assert(controller.stats().pwc_occupancy <= 120 &&
         controller.quiescent_invariants_hold());

  // ASID reuse must not retain a physical next-table pointer from the old
  // address space. This F5-only flush is intentionally separate from the
  // unchanged generic M3 PWC behavior.
  controller.flush_translation_asid(0);
  assert(controller.stats().pwc_occupancy == 0 &&
         controller.stats().pwc_occupancy_by_level[0] == 0 &&
         controller.stats().pwc_occupancy_by_level[1] == 0 &&
         controller.stats().pwc_occupancy_by_level[2] == 0 &&
         controller.quiescent_invariants_hold());

  controller.print_stats(stdout);
  printf("vm_c10b_f5_physical_pwc_test PASS\n");
  return 0;
}
