#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

static const char *kRegistration = "/tmp/vm_c10a_registration_v2.tsv";
static const uint64_t kPage = 64ULL * 1024ULL;

static void write_registration() {
  std::ofstream out(kRegistration);
  assert(out.good());
  out << "M4B_WEIGHT_SEGMENT_REGISTRATION_V2\n";
  out << "roi\tunit\n";
  out << "source_sha256\t"
      << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  out << "archive_sha256\t"
      << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  out << "object_map_sha256\t"
      << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  out << "provisioned_asid\t7\n";
  out << "epoch\t11\n";
  // VPN 16..17 map to non-identity PPN 256..257.
  out << "descriptor\t7\t11\t16\t17\t256\t1\t0\n";
  // A separate physical extent is legal and retains the same ASID/epoch.
  out << "descriptor\t7\t11\t32\t32\t512\t1\t0\n";
}

static vm_translation::translation_config controller_config(unsigned lseg) {
  return vm_translation::translation_config(
      1, kPage, vm_translation::tlb_config(2, 2, 1),
      vm_translation::tlb_config(4, 4, 1), 4, 4, 1, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 3, 1, "",
      vm_translation::L2_TLB_STANDARD,
      vm_translation::segment_config(true, 2, lseg, kRegistration));
}

static void advance(vm_translation::translation_controller *vm,
                    uint64_t begin, uint64_t end) {
  for (uint64_t cycle = begin; cycle <= end; ++cycle) vm->cycle(cycle);
}

int main() {
  write_registration();
  vm_translation::weight_segment_map map(kRegistration);
  assert(map.enabled() && map.registered_v2() && map.size() == 2);
  assert(map.active_asid() == 7 && map.active_epoch() == 11);
  uint64_t ppn = 0;
  vm_translation::weight_segment_map::fallback_reason reason =
      vm_translation::weight_segment_map::SEGMENT_FALLBACK_NONE;
  const vm_translation::translation_key key(7, 16, kPage);
  assert(map.translate(key, 16 * kPage + 19, 32, true, &ppn, &reason));
  assert(ppn == 256 && ppn != key.vpn);
  uint64_t conventional = 0;
  assert(map.registered_ppn(key, &conventional) && conventional == ppn);
  assert(!map.translate(vm_translation::translation_key(8, 16, kPage),
                        16 * kPage + 19, 32, true, &ppn, &reason));
  assert(reason == vm_translation::weight_segment_map::SEGMENT_FALLBACK_ASID);
  assert(!map.translate(key, 16 * kPage + kPage - 8, 16, true, &ppn,
                        &reason));
  assert(reason == vm_translation::weight_segment_map::SEGMENT_FALLBACK_BOUNDARY);
  assert(!map.translate(key, 16 * kPage + 19, 32, false, &ppn, &reason));
  assert(reason == vm_translation::weight_segment_map::SEGMENT_FALLBACK_RIGHTS);

  // An object map is deliberately absent.  Registration alone controls a
  // non-identity Segment hit, and HIT_FIRST suppresses the unfinished L1
  // result and every conventional lower request.
  vm_translation::translation_controller segment_first(controller_config(2));
  uint64_t pa = 0;
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  assert(segment_first.translate(0, 7, 16 * kPage + 19, 32, 0, 1, &pa,
                                 &source) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&segment_first, 0, 2);
  assert(segment_first.translate(0, 7, 16 * kPage + 19, 32, 2, 1, &pa,
                                 &source) == vm_translation::READY);
  assert(pa == 256 * kPage + 19 &&
         source == vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  assert(segment_first.l1(0).stats().accesses == 0 &&
         segment_first.stats().l2_lookup_launches == 0 &&
         segment_first.stats().segment_first_owners == 1 &&
         segment_first.stats().segment_late_result_discards == 1);

  // A store is visible to the controller and must fall through normal paging,
  // yet its registered PTE result agrees with the descriptor's PA extent.
  assert(segment_first.translate(0, 7, 16 * kPage + 40, 32, 3, 2, &pa,
                                 &source,
                                 vm_translation::TRANSLATION_ACCESS_WRITE) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&segment_first, 3, 9);
  assert(segment_first.stats().segment_fallback_by_reason[
             vm_translation::weight_segment_map::SEGMENT_FALLBACK_RIGHTS] ==
         1);
  assert(segment_first.stats().l2_lookup_launches == 1);
  assert(segment_first.translate(0, 7, 16 * kPage + 40, 32, 9, 2, &pa,
                                 &source,
                                 vm_translation::TRANSLATION_ACCESS_WRITE) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&segment_first, 10, 12);
  assert(segment_first.translate(0, 7, 16 * kPage + 40, 32, 12, 2, &pa,
                                 &source,
                                 vm_translation::TRANSLATION_ACCESS_WRITE) ==
         vm_translation::READY);
  assert(pa == 256 * kPage + 40 &&
         source == vm_translation::TRANSLATION_SOURCE_PTW);
  assert(segment_first.invariants_hold());

  // With a deliberately slow Segment model point, an already-resident L1
  // entry must win first rather than resurrecting C3's wait-both behavior.
  vm_translation::translation_controller l1_first(controller_config(20));
  assert(l1_first.translate(0, 7, 16 * kPage + 64, 32, 0, 3, &pa, &source,
                            vm_translation::TRANSLATION_ACCESS_WRITE) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&l1_first, 0, 23);
  assert(l1_first.translate(0, 7, 16 * kPage + 64, 32, 24, 4, &pa, &source) ==
         vm_translation::TRANSLATION_PENDING);
  advance(&l1_first, 25, 27);
  assert(l1_first.translate(0, 7, 16 * kPage + 64, 32, 27, 4, &pa, &source) ==
         vm_translation::READY);
  assert(source == vm_translation::TRANSLATION_SOURCE_L1_TLB_HIT &&
         l1_first.stats().segment_l1_first_owners == 1 &&
         l1_first.stats().segment_late_result_discards == 1 &&
         l1_first.stats().l2_lookup_launches == 1);

  // C9's fair profiles are geometry, not aliases for the old 768-group
  // experiment.  Validate their derived set counts and leaf invalidation
  // semantics directly, without a simulator replay.
  vm_translation::subentry_tlb equal_bit(vm_translation::tlb_config(96, 16, 1));
  vm_translation::subentry_tlb charged(
      vm_translation::tlb_config(32, 16, 1));
  assert(equal_bit.sets() == 6);
  assert(charged.sets() == 2);
  const vm_translation::translation_key sibling0(7, 64, kPage);
  const vm_translation::translation_key sibling1(7, 65, kPage);
  equal_bit.fill(sibling0, 1024, 0, vm_translation::OBJECT_WEIGHT);
  equal_bit.fill(sibling1, 1025, 0, vm_translation::OBJECT_WEIGHT);
  assert(equal_bit.occupancy() == 1 && equal_bit.valid_subentries() == 2);
  assert(equal_bit.invalidate(sibling0));
  assert(equal_bit.occupancy() == 1 && equal_bit.valid_subentries() == 1);
  assert(equal_bit.invalidate(sibling1));
  assert(equal_bit.occupancy() == 0 && equal_bit.valid_subentries() == 0);
  equal_bit.fill(sibling0, 1024, 1, vm_translation::OBJECT_WEIGHT);
  equal_bit.flush_asid(7);
  assert(equal_bit.occupancy() == 0 && equal_bit.valid_subentries() == 0);
  charged.fill(sibling0, 1024, 0, vm_translation::OBJECT_WEIGHT);
  charged.flush_all();
  assert(charged.occupancy() == 0 && charged.valid_subentries() == 0);
  assert(remove(kRegistration) == 0);
  printf("vm_c10a_registered_segment_test PASS\n");
  return 0;
}
