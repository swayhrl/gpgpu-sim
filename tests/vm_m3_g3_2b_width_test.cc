#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_core.h"
#include "gpgpu-sim/vm_translation.h"

int main() {
  const uint64_t page64k = 64ULL * 1024ULL;
  const uint64_t page2m = 2ULL * 1024ULL * 1024ULL;
  const uint64_t offender = 0xfffdc0000000c0ULL;

  // Current generic M3 defaults: all valid 56-bit identity-like application
  // addresses lie below the distinct 64-TiB synthetic PTE reservation.
  const vm_translation::page_table_config config56;
  assert(config56.valid());
  assert(config56.virtual_address_bits == 56);
  assert(config56.application_physical_limit == (1ULL << 56));
  assert(config56.pte_physical_base >= config56.application_physical_limit);
  assert(config56.pte_physical_bytes == (1ULL << 46));
  assert(config56.pte_physical_base <=
         ~0ULL - config56.pte_physical_bytes);

  vm_translation::radix_page_table_backend backend56(config56);
  const vm_translation::translation_key offender_key(
      0, offender / page64k, page64k);
  assert(backend56.supports_key(offender_key));
  const vm_translation::pte_request offender_pte =
      backend56.make_pte_request(offender_key, 0, 1);
  assert(offender_pte.is_physical && offender_pte.bypass_translation);
  assert(backend56.owns_pte_physical_address(offender_pte.physical_address));
  assert(backend56.resolve_ppn(offender_key) * page64k +
             (offender % page64k) ==
         offender);
  assert(vm_core::identity_translate(offender, page64k) == offender);

  const uint64_t boundary = (1ULL << 56) - 1;
  const vm_translation::translation_key boundary_key(
      0, boundary / page64k, page64k);
  assert(backend56.supports_key(boundary_key));
  assert(backend56.owns_pte_physical_address(
      backend56.make_pte_request(boundary_key, 3, 2).physical_address));

  // No modulo mapping: a 57-bit input is rejected before a PTE is formed;
  // the real PTE path asserts this same predicate as its hard correctness stop.
  const vm_translation::translation_key too_wide_key(
      0, (1ULL << 56) / page64k, page64k);
  assert(!backend56.supports_key(too_wide_key));

  // Min/max PTE addresses for all eight class/level namespaces are disjoint.
  uint64_t previous_max = 0;
  bool have_previous = false;
  for (unsigned page_class = 0; page_class < 2; ++page_class) {
    const uint64_t page_size = page_class == 0 ? page64k : page2m;
    const unsigned vpn_bits = page_class == 0 ? 40 : 35;
    const uint64_t max_vpn = (1ULL << vpn_bits) - 1;
    for (unsigned level = 0; level < backend56.levels(); ++level) {
      const vm_translation::translation_key min_key(0, 0, page_size);
      const vm_translation::translation_key max_key(0, max_vpn, page_size);
      const uint64_t min_pte =
          backend56.make_pte_request(min_key, level, 10 + level).physical_address;
      const uint64_t max_pte =
          backend56.make_pte_request(max_key, level, 20 + level).physical_address;
      assert(min_pte <= max_pte && backend56.owns_pte_physical_address(min_pte));
      assert(backend56.owns_pte_physical_address(max_pte));
      if (have_previous) assert(previous_max < min_pte);
      previous_max = max_pte;
      have_previous = true;
    }
  }

  // Retain a direct 49-bit backend configuration for the later paper adapter.
  const vm_translation::page_table_config config49(4, 49, 1ULL << 49,
                                                     1ULL << 49, 1ULL << 39);
  vm_translation::radix_page_table_backend backend49(config49);
  assert(config49.valid());
  assert(backend49.supports_key(
      vm_translation::translation_key(0, (1ULL << 33) - 1, page64k)));
  assert(!backend49.supports_key(offender_key));

  printf("vm_m3_g3_2b_width_test PASS\n");
  return 0;
}
