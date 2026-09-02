#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

using vm_translation::READY;
using vm_translation::TRANSLATION_PENDING;
using vm_translation::translation_config;
using vm_translation::translation_controller;
using vm_translation::tlb_config;

static void translate(translation_controller &vm, unsigned sid, uint64_t va,
                      uint64_t cycle) {
  static uint64_t next_uid = 1;
  const uint64_t uid = next_uid++;
  uint64_t pa = 0;
  const vm_translation::lookup_result first =
      vm.translate(sid, 0, va, cycle, uid, &pa);
  if (first == TRANSLATION_PENDING) {
    assert(vm.complete_translation(
        vm_translation::translation_key(0, va / (64ULL * 1024ULL),
                                        64ULL * 1024ULL),
        cycle + 1));
    assert(vm.translate(sid, 0, va, cycle + 2, uid, &pa) == READY);
  } else {
    assert(first == READY);
  }
  assert(pa == va);
}

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  translation_controller one_page(
      translation_config(1, page, tlb_config(2, 2, 4), tlb_config(4, 2, 4)));
  translate(one_page, 0, page + 17, 0);
  translate(one_page, 0, page + 31, 1);
  assert(one_page.l1(0).stats().accesses == 3);
  assert(one_page.l1(0).stats().hits == 2);
  assert(one_page.l1(0).stats().misses == 1);
  assert(one_page.l2().stats().misses == 1);
  assert(one_page.stats().mapper_lookups == 1);

  translation_controller l1_capacity(
      translation_config(1, page, tlb_config(1, 1, 4), tlb_config(8, 1, 4)));
  translate(l1_capacity, 0, 0 * page, 0);
  translate(l1_capacity, 0, 1 * page, 1);
  translate(l1_capacity, 0, 0 * page, 2);
  assert(l1_capacity.l1(0).stats().evictions == 2);
  assert(l1_capacity.l1(0).stats().hits == 2);
  assert(l1_capacity.l2().stats().hits == 1);

  translation_controller l2_capacity(
      translation_config(1, page, tlb_config(1, 1, 4), tlb_config(1, 1, 4)));
  translate(l2_capacity, 0, 0 * page, 0);
  translate(l2_capacity, 0, 1 * page, 1);
  translate(l2_capacity, 0, 0 * page, 2);
  assert(l2_capacity.l2().stats().evictions == 2);
  assert(l2_capacity.stats().mapper_lookups == 3);

  translation_controller two_sm(
      translation_config(2, page, tlb_config(2, 2, 4), tlb_config(4, 2, 4)));
  translate(two_sm, 0, 7 * page + 3, 0);
  translate(two_sm, 1, 7 * page + 9, 1);
  assert(two_sm.l1(0).stats().misses == 1);
  assert(two_sm.l1(1).stats().misses == 1);
  assert(two_sm.l2().stats().accesses == 2);
  assert(two_sm.l2().stats().hits == 1);
  assert(two_sm.stats().mapper_lookups == 1);

  // Page-size is part of the tag even though G2-1 enables one size at a time.
  vm_translation::set_associative_tlb tagged(tlb_config(2, 2, 2));
  uint64_t ppn = 0;
  tagged.fill(vm_translation::translation_key(0, 1, page), 1, 0);
  assert(tagged.probe(vm_translation::translation_key(0, 1, page), 1, &ppn));
  assert(!tagged.probe(vm_translation::translation_key(0, 1, 2 * 1024 * 1024),
                       1, &ppn));

  translation_controller ports(
      translation_config(1, page, tlb_config(2, 2, 1), tlb_config(2, 2, 1)));
  uint64_t pa = 0;
  assert(ports.translate(0, 0, 0, 0, 1, &pa) == TRANSLATION_PENDING);
  assert(ports.translate(0, 0, page, 0, 2, &pa) ==
         vm_translation::L1_PORT_STALL);
  assert(ports.l1(0).stats().port_stalls == 1);
  printf("vm_m2_g2_1_test PASS\n");
  return 0;
}
