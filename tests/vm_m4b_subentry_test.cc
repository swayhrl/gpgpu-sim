#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

using vm_translation::L2_TLB_STANDARD;
using vm_translation::L2_TLB_SUBENTRY_16;
using vm_translation::READY;
using vm_translation::TRANSLATION_PENDING;

static const uint64_t kPage = 64ULL * 1024ULL;

static vm_translation::translation_config config(unsigned mode,
                                                  uint64_t page = kPage) {
  return vm_translation::translation_config(
      1, page, vm_translation::tlb_config(1, 1, 4),
      vm_translation::tlb_config(2, 2, 2), 8, 8, 1, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 0, 0, "",
      mode);
}

static void complete(vm_translation::translation_controller *vm, uint64_t vpn,
                     uint64_t cycle, uint64_t uid) {
  uint64_t pa = 0;
  const uint64_t va = vpn * kPage + 9;
  assert(vm->translate(0, 0, va, cycle, uid, &pa) == TRANSLATION_PENDING);
  assert(vm->complete_translation(vm_translation::translation_key(0, vpn,
                                                                   kPage),
                                  cycle + 1));
  assert(vm->translate(0, 0, va, cycle + 2, uid, &pa) == READY);
  assert(pa == va);
}

int main() {
  // A tag match with an invalid selected leaf is a real TLB miss.  A later
  // fill of that leaf preserves the sibling translation and group residency.
  vm_translation::subentry_tlb direct(vm_translation::tlb_config(2, 2, 1));
  uint64_t ppn = 0;
  const vm_translation::translation_key page0(3, 0, kPage);
  const vm_translation::translation_key page1(3, 1, kPage);
  const vm_translation::translation_key page2(3, 2, kPage);
  direct.fill(page0, 100, 0, vm_translation::OBJECT_WEIGHT);
  assert(!direct.probe(page1, 1, &ppn));
  assert(direct.subentry_stats().base_tag_hits == 1);
  assert(direct.subentry_stats().selected_subentry_misses == 1);
  direct.fill(page1, 101, 2, vm_translation::OBJECT_WEIGHT);
  assert(direct.subentry_stats().existing_group_fills == 1);
  assert(direct.probe(page0, 3, &ppn) && ppn == 100);
  assert(direct.probe(page1, 4, &ppn) && ppn == 101);

  // Group LRU, not leaf LRU: touching group zero protects it while a full
  // two-way set evicts group one.  The valid-leaf eviction count remains
  // independent from the group-eviction counter.
  const vm_translation::translation_key group1(3, 16, kPage);
  const vm_translation::translation_key group2(3, 32, kPage);
  direct.fill(group1, 116, 5, vm_translation::OBJECT_KV_CACHE);
  assert(direct.probe(page0, 6, &ppn));
  direct.fill(group2, 132, 7, vm_translation::OBJECT_UNKNOWN);
  assert(direct.stats().evictions == 1);
  assert(direct.subentry_stats().group_evictions == 1);
  assert(direct.subentry_stats().valid_subentries_evicted == 1);
  assert(!direct.probe(group1, 8, &ppn));
  assert(direct.probe(page0, 9, &ppn) && ppn == 100);
  assert(direct.valid_subentries() == 3);
  assert(direct.subentry_stats()
             .valid_subentries_by_object[vm_translation::OBJECT_WEIGHT] == 2);

  // Candidate port arbitration is the same finite shared-L2 contract.
  vm_translation::subentry_tlb ports(vm_translation::tlb_config(2, 2, 1));
  assert(ports.try_consume_port(10));
  assert(!ports.try_consume_port(10));
  assert(ports.stats().port_stalls == 1);
  assert(ports.try_consume_port(11));

  // The candidate is intentionally rejected for 2MB rather than silently
  // grouping a page class whose leaf semantics have not been authorized.
  assert(!config(L2_TLB_SUBENTRY_16, 2ULL * 1024ULL * 1024ULL).valid());
  assert(config(L2_TLB_STANDARD, 2ULL * 1024ULL * 1024ULL).valid());

  // Controller integration: adjacent 64KB pages share a group, but an
  // initially absent adjacent leaf still performs exactly one normal walk.
  // Once filled, L1 eviction exposes the sub-entry L2 hit without changing
  // the accepted identity SimPA mapping.
  vm_translation::translation_controller vm(config(L2_TLB_SUBENTRY_16));
  complete(&vm, 0, 0, 1);
  complete(&vm, 1, 10, 2);
  const vm_translation::subentry_tlb_stats &stats =
      vm.l2_subentries().subentry_stats();
  assert(vm.l2().stats().accesses == 0);
  assert(vm.l2_subentries().stats().accesses == 2);
  assert(vm.l2_subentries().stats().misses == 2);
  assert(stats.base_tag_hits == 1 && stats.selected_subentry_misses == 1);
  assert(stats.group_fills == 1 && stats.existing_group_fills == 1);
  uint64_t pa = 0;
  // The retained zero-latency diagnostic API reports an L2 hit
  // synchronously, while nonzero modeled latencies use the pending/retry
  // path validated by the M3 timing suite.
  assert(vm.translate(0, 0, 3, 20, 3, &pa) == READY);
  assert(pa == 3);
  assert(vm.l2_subentries().stats().hits == 1);
  assert(vm.l2_subentries().stats().evictions == 0);
  assert(vm.quiescent_invariants_hold());

  // Explicit standard selection remains equivalent to the default selection.
  vm_translation::translation_controller standard_default(config(L2_TLB_STANDARD));
  vm_translation::translation_controller standard_explicit(config(L2_TLB_STANDARD));
  complete(&standard_default, 4, 0, 10);
  complete(&standard_explicit, 4, 0, 10);
  assert(standard_default.l2().stats().accesses ==
         standard_explicit.l2().stats().accesses);
  assert(standard_default.l2().stats().misses ==
         standard_explicit.l2().stats().misses);
  assert(standard_default.l2().stats().evictions ==
         standard_explicit.l2().stats().evictions);

  printf("vm_m4b_subentry_test PASS\n");
  return 0;
}
