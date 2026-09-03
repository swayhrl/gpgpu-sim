#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

static vm_translation::translation_config config_for(unsigned l1_entries,
                                                      unsigned l1_ports,
                                                      unsigned fixed_walk,
                                                      unsigned l1_latency,
                                                      unsigned l2_latency) {
  const uint64_t page = 64ULL * 1024ULL;
  return vm_translation::translation_config(
      1, page, vm_translation::tlb_config(l1_entries, l1_entries, l1_ports),
      vm_translation::tlb_config(8, 8, 1), 8, 8, 1, fixed_walk,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), l1_latency,
      l2_latency);
}

static void advance(vm_translation::translation_controller *vm,
                    uint64_t begin, uint64_t end) {
  for (uint64_t cycle = begin; cycle <= end; ++cycle) vm->cycle(cycle);
}

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  uint64_t pa = 0;

  // Exact L1-miss/L2-miss handoff: ports launch at cycle 0, probes complete
  // only after their configured service intervals, then one MSHR exists.
  vm_translation::translation_controller first(config_for(1, 1, 100, 2, 3));
  assert(first.translate(0, 0, page + 7, 0, 1, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(first.l1(0).stats().accesses == 0);
  advance(&first, 0, 1);
  assert(first.l1(0).stats().accesses == 0);
  first.cycle(2);
  assert(first.l1(0).stats().accesses == 1 &&
         first.l1(0).stats().misses == 1);
  assert(first.l2().stats().accesses == 0);
  advance(&first, 3, 4);
  assert(first.l2().stats().accesses == 0);
  first.cycle(5);
  assert(first.l2().stats().accesses == 1 && first.l2().stats().misses == 1);
  assert(first.active_mshrs() == 1);

  // Same waiter retries while the lookup/walk is pending must not launch or
  // probe again; a different waiter still starts its own first lookup and
  // merges only after that lookup observes the same miss.
  const uint64_t l1_accesses_before = first.l1(0).stats().accesses;
  const uint64_t l2_accesses_before = first.l2().stats().accesses;
  assert(first.translate(0, 0, page + 7, 6, 1, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(first.l1(0).stats().accesses == l1_accesses_before);
  assert(first.l2().stats().accesses == l2_accesses_before);
  assert(first.translate(0, 0, page + 7, 6, 2, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&first, 6, 8);
  assert(first.l1(0).stats().accesses == 2);
  advance(&first, 9, 11);
  assert(first.l2().stats().accesses == 2);
  assert(first.stats().mshr_merges == 1);
  assert(first.stats().lookup_inflight_bypasses == 0);
  assert(first.stats().pending_waiter_bypasses >= 1);

  // Finite-port contention occurs at launch.  A waiting same-UID retry does
  // not consume another port or re-probe before its service completion.
  vm_translation::translation_controller ports(config_for(4, 1, 100, 2, 3));
  assert(ports.translate(0, 0, 3 * page, 0, 11, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(ports.translate(0, 0, 4 * page, 0, 12, &pa) ==
         vm_translation::L1_PORT_STALL);
  assert(ports.translate(0, 0, 3 * page, 1, 11, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(ports.l1(0).stats().port_stalls == 1 &&
         ports.l1(0).stats().accesses == 0);
  assert(ports.stats().lookup_inflight_bypasses == 1);
  assert(ports.translate(0, 0, 4 * page, 1, 12, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&ports, 0, 3);
  assert(ports.l1(0).stats().accesses == 2);

  // Fill A through the fixed-latency diagnostic path, then prove exact L1
  // hit completion.  Filling B evicts A from a one-entry L1 while preserving
  // it in L2, giving the exact L1-miss -> L2-hit completion cycle.
  vm_translation::translation_controller hits(config_for(1, 1, 1, 2, 3));
  assert(hits.translate(0, 0, 8 * page + 1, 0, 21, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&hits, 0, 6);  // L1@2, L2@5, fixed walk completion@6.
  assert(hits.active_mshrs() == 0);
  assert(hits.translate(0, 0, 8 * page + 2, 10, 22, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&hits, 10, 11);
  assert(hits.l1(0).stats().hits == 0);
  hits.cycle(12);
  assert(hits.translate(0, 0, 8 * page + 2, 12, 22, &pa) ==
         vm_translation::READY && pa == 8 * page + 2);

  assert(hits.translate(0, 0, 9 * page, 20, 23, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&hits, 20, 26);
  assert(hits.active_mshrs() == 0);
  assert(hits.translate(0, 0, 8 * page + 3, 30, 24, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&hits, 30, 34);
  hits.cycle(35);
  assert(hits.l2().stats().hits == 1);
  assert(hits.translate(0, 0, 8 * page + 3, 35, 24, &pa) ==
         vm_translation::READY && pa == 8 * page + 3);

  // Legacy zero-latency diagnostics retain synchronous accepted functional
  // counts and immediately hand a double miss to the translation MSHR.
  vm_translation::translation_controller zero(config_for(2, 1, 1, 0, 0));
  assert(zero.translate(0, 0, 12 * page, 0, 31, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  assert(zero.l1(0).stats().accesses == 1 && zero.l2().stats().accesses == 1 &&
         zero.active_mshrs() == 1);

  printf("vm_m3_g3_4b_tlb_timing_test PASS\n");
  return 0;
}
