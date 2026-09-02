#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

class replacement_backend : public vm_translation::page_table_backend {
 public:
  bool valid() const { return true; }
  unsigned levels() const { return 1; }
  uint64_t resolve_ppn(const vm_translation::translation_key &key) const {
    return key.vpn + 7;
  }
  vm_translation::pte_request make_pte_request(
      const vm_translation::translation_key &key, unsigned level,
      uint64_t request_id) const {
    return vm_translation::pte_request(key, level, 0x90000000ULL + level * 8,
                                       request_id);
  }
  bool owns_pte_physical_address(uint64_t pa) const {
    return pa >= 0x90000000ULL && pa < 0x90001000ULL;
  }
};

int main() {
  const uint64_t page64k = 64ULL * 1024ULL;
  const uint64_t page2m = 2ULL * 1024ULL * 1024ULL;
  vm_translation::radix_page_table_backend backend;
  assert(backend.valid());
  assert(backend.levels() == 4);

  const vm_translation::translation_key key64k(3, 0x12345, page64k);
  const vm_translation::pte_request l0 =
      backend.make_pte_request(key64k, 0, 101);
  const vm_translation::pte_request l1 =
      backend.make_pte_request(key64k, 1, 102);
  assert(l0.is_physical && l0.bypass_translation);
  assert(l0.request_id == 101 && l0.level == 0);
  assert(l0.physical_address != l1.physical_address);
  assert(backend.owns_pte_physical_address(l0.physical_address));
  assert(backend.resolve_ppn(key64k) == key64k.vpn);

  const vm_translation::translation_key key2m(3, 0x12345, page2m);
  const vm_translation::pte_request large =
      backend.make_pte_request(key2m, 0, 103);
  assert(large.physical_address != l0.physical_address);
  assert(backend.owns_pte_physical_address(large.physical_address));

  // The requested PTE range is separate from the identity-mapped application
  // range; this is the no-overlap half of the M3 physical contract.
  assert(!backend.owns_pte_physical_address((1ULL << 49) - 1));

  // This was the review-found collision under the old page-size-dependent
  // shift.  The fixed 33-bit namespace width keeps both requests distinct.
  const uint64_t collision_x = 0x12345;
  const vm_translation::translation_key collision64k(
      3, (4ULL << 28) | collision_x, page64k);
  const vm_translation::translation_key collision2m(3, collision_x, page2m);
  const vm_translation::pte_request collision64k_pte =
      backend.make_pte_request(collision64k, 0, 104);
  const vm_translation::pte_request collision2m_pte =
      backend.make_pte_request(collision2m, 0, 105);
  assert(collision64k_pte.physical_address !=
         collision2m_pte.physical_address);

  // Each (page-size class, level) namespace is ordered by a shared 33-bit
  // slot width.  Test min/max valid VPN values for all eight namespaces and
  // assert every completed interval lies below the next one.
  uint64_t previous_max = 0;
  bool have_previous_namespace = false;
  for (unsigned page_class = 0; page_class < 2; ++page_class) {
    const uint64_t page_size = page_class == 0 ? page64k : page2m;
    const unsigned vpn_bits = page_class == 0 ? 33 : 28;
    const uint64_t max_vpn = (1ULL << vpn_bits) - 1;
    for (unsigned level = 0; level < backend.levels(); ++level) {
      const vm_translation::translation_key min_key(3, 0, page_size);
      const vm_translation::translation_key max_key(3, max_vpn, page_size);
      const vm_translation::pte_request min_pte =
          backend.make_pte_request(min_key, level, 200 + level + 10 * page_class);
      const vm_translation::pte_request max_pte =
          backend.make_pte_request(max_key, level, 300 + level + 10 * page_class);
      assert(min_pte.physical_address <= max_pte.physical_address);
      assert(backend.owns_pte_physical_address(min_pte.physical_address));
      assert(backend.owns_pte_physical_address(max_pte.physical_address));
      if (have_previous_namespace)
        assert(previous_max < min_pte.physical_address);
      previous_max = max_pte.physical_address;
      have_previous_namespace = true;
    }
  }

  // A replacement backend changes only mapping policy; the MSHR/replay path
  // remains the M2 controller contract.
  replacement_backend replacement;
  vm_translation::translation_controller controller(
      vm_translation::translation_config(
          1, page64k, vm_translation::tlb_config(2, 2, 2),
          vm_translation::tlb_config(2, 2, 2), 2, 2, 1, 1),
      &replacement);
  uint64_t pa = 0;
  assert(controller.translate(0, 0, page64k + 9, 0, 7, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  controller.cycle(0);
  controller.cycle(1);
  assert(controller.translate(0, 0, page64k + 9, 2, 7, &pa) ==
         vm_translation::READY);
  assert(pa == (1 + 7) * page64k + 9);
  assert(controller.quiescent_invariants_hold());

  printf("vm_m3_g3_1_test PASS\n");
  return 0;
}
