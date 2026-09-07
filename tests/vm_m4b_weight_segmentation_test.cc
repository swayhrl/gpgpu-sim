#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

static const char *kObjectMap = "/tmp/vm_m4b_segment_object_map.tsv";
static const char *kSegmentMap = "/tmp/vm_m4b_segment_map.tsv";
static const uint64_t kPage = 64ULL * 1024ULL;

static void write_maps() {
  std::ofstream objects(kObjectMap);
  assert(objects.good());
  objects << "M4C_OBJECT_MAP_V1\n";
  objects << "roi\ttest\n";
  objects << "source_sha256\t"
          << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  objects << "archive_sha256\t"
          << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  objects << "sidecar_sha256\t"
          << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  objects << "range\tWEIGHT\t0x10000\t0x2ffff\n";
  objects << "range\tKV_CACHE\t0x30000\t0x3ffff\n";
  objects << "range\tWEIGHT\t0x50000\t0x5ffff\n";
  objects.close();

  std::ofstream segments(kSegmentMap);
  assert(segments.good());
  segments << "M4B_WEIGHT_SEGMENT_MAP_V1\n";
  segments << "roi\ttest\n";
  segments << "source_sha256\t"
           << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  segments << "archive_sha256\t"
           << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  segments << "object_map_sha256\t"
           << "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\n";
  segments << "segment\tWEIGHT\t0x10000\t0x2ffff\n";
}

static vm_translation::translation_config config(unsigned l2_mode) {
  return vm_translation::translation_config(
      1, kPage, vm_translation::tlb_config(1, 1, 1),
      vm_translation::tlb_config(4, 4, 1), 8, 8, 1, 4,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 3, 4,
      kObjectMap, l2_mode,
      vm_translation::segment_config(true, 1, 2, kSegmentMap));
}

static void advance(vm_translation::translation_controller *vm,
                    uint64_t begin, uint64_t end) {
  for (uint64_t cycle = begin; cycle <= end; ++cycle) vm->cycle(cycle);
}

int main() {
  write_maps();
  vm_translation::weight_segment_map direct(kSegmentMap);
  uint64_t ppn = 0;
  assert(direct.enabled() && direct.size() == 1);
  assert(direct.translate(0x10020, 32, kPage, &ppn));
  assert(ppn == 1);
  assert(!direct.translate(0x2fff0, 32, kPage, &ppn));

  // Segment and L1 launch together.  The Segment hit waits for the raw L1
  // observation, then suppresses every conventional lower translation path.
  vm_translation::translation_controller vm(
      config(vm_translation::L2_TLB_SUBENTRY_16));
  uint64_t pa = 0;
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  assert(vm.translate(0, 0, 0x10020, 32, 0, 1, &pa, &source) ==
         vm_translation::TRANSLATION_PENDING);
  assert(vm.stats().segment_lookup_launches == 1);
  assert(vm.stats().l1_lookup_launches == 1);
  advance(&vm, 0, 2);
  assert(vm.stats().segment_hits == 1);
  assert(vm.l1(0).stats().accesses == 0);
  assert(vm.active_mshrs() == 0 && vm.stats().l2_lookup_launches == 0);
  vm.cycle(3);
  assert(vm.l1(0).stats().accesses == 1);
  assert(vm.l1(0).occupancy() == 0);
  assert(vm.stats().segment_raw_l1_misses == 1);
  assert(vm.stats().segment_effective_l1_misses == 0);
  assert(vm.translate(0, 0, 0x10020, 32, 3, 1, &pa, &source) ==
         vm_translation::READY);
  assert(pa == 0x10020 &&
         source == vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  assert(vm.stats().segment_l2_suppressed == 1);
  assert(vm.stats().segment_mshr_suppressed == 1);
  assert(vm.stats().segment_pwq_suppressed == 1);
  assert(vm.stats().segment_walker_suppressed == 1);
  assert(vm.stats().segment_pwc_suppressed == 1);
  assert(vm.stats().segment_pte_suppressed == 1);
  assert(vm.stats().segment_l1_fill_suppressed == 1);
  assert(vm.stats().l2_lookup_launches == 0 &&
         vm.stats().mshr_allocations == 0 && vm.stats().walk_starts == 0 &&
         vm.stats().pte_requests == 0);

  // A Weight access outside the immutable descriptor misses Segment.  Its L1
  // result is reused after it completes: L2 starts once, without a second L1
  // probe; the later PTW delivery retry does not launch Segment again.
  assert(vm.translate(0, 0, 0x50020, 32, 10, 2, &pa, &source) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&vm, 10, 13);
  const uint64_t l1_after_miss = vm.l1(0).stats().accesses;
  assert(l1_after_miss == 2);
  assert(vm.stats().segment_misses == 1);
  assert(vm.stats().segment_effective_l1_misses == 1);
  assert(vm.stats().l2_lookup_launches == 1);
  advance(&vm, 14, 17);
  assert(vm.l1(0).stats().accesses == l1_after_miss);
  assert(vm.active_mshrs() == 1 && vm.stats().mshr_allocations == 1);
  advance(&vm, 18, 21);
  assert(vm.active_mshrs() == 0 && vm.stats().walk_starts == 1);
  assert(vm.translate(0, 0, 0x50020, 32, 22, 2, &pa, &source) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&vm, 22, 25);
  assert(vm.translate(0, 0, 0x50020, 32, 25, 2, &pa, &source) ==
         vm_translation::READY);
  assert(pa == 0x50020 && source == vm_translation::TRANSLATION_SOURCE_PTW);
  assert(vm.stats().segment_lookup_launches == 2);
  assert(vm.stats().segment_raw_l1_completions == 2);

  // KV and unknown accesses always take the normal L1/L2/MSHR path even when
  // Weight Segmentation is enabled.
  vm_translation::translation_controller kv(
      config(vm_translation::L2_TLB_STANDARD));
  assert(kv.translate(0, 0, 0x30020, 32, 0, 3, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&kv, 0, 7);
  assert(kv.stats().segment_misses == 1 && kv.stats().l2_lookup_launches == 1 &&
         kv.stats().mshr_allocations == 1);
  vm_translation::translation_controller unknown(
      config(vm_translation::L2_TLB_STANDARD));
  assert(unknown.translate(0, 0, 0x40020, 32, 0, 4, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&unknown, 0, 7);
  assert(unknown.stats().segment_misses == 1 &&
         unknown.stats().l2_lookup_launches == 1 &&
         unknown.stats().mshr_allocations == 1);

  assert(vm.invariants_hold());
  assert(vm.object_attribution_conserves());
  assert(remove(kObjectMap) == 0);
  assert(remove(kSegmentMap) == 0);
  printf("vm_m4b_weight_segmentation_test PASS\n");
  return 0;
}
