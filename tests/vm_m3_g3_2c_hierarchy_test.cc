#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_core.h"
#include "gpgpu-sim/vm_translation.h"

static void expect_widths(const vm_translation::radix_page_table_backend &pt,
                          uint64_t page_size, const unsigned *widths,
                          unsigned count) {
  assert(pt.levels() == count);
  unsigned accumulated = 0;
  for (unsigned level = 0; level < count; ++level) {
    assert(pt.level_width(page_size, level) == widths[level]);
    accumulated += widths[level];
    assert(pt.prefix_bits_for_level(page_size, level) == accumulated);
  }
}

static uint64_t pte_pa(const vm_translation::radix_page_table_backend &pt,
                       const vm_translation::translation_key &key,
                       unsigned level) {
  const vm_translation::pte_request request =
      pt.make_pte_request(key, level, level + 1);
  assert(request.is_physical && request.bypass_translation);
  assert(pt.owns_pte_physical_address(request.physical_address));
  return request.physical_address;
}

static unsigned log2_exact(uint64_t value) {
  unsigned result = 0;
  assert(value != 0 && (value & (value - 1)) == 0);
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

static void assert_disjoint_ranges(
    const vm_translation::radix_page_table_backend &pt) {
  const uint64_t pages[] = {64ULL * 1024ULL, 2ULL * 1024ULL * 1024ULL};
  uint64_t previous_max = 0;
  bool have_previous = false;
  for (unsigned page_class = 0; page_class < 2; ++page_class) {
    const uint64_t page = pages[page_class];
    const unsigned vpn_bits = pt.config().virtual_address_bits - log2_exact(page);
    const vm_translation::translation_key low(0, 0, page);
    const vm_translation::translation_key high(0, (1ULL << vpn_bits) - 1,
                                                page);
    for (unsigned level = 0; level < pt.levels(); ++level) {
      const uint64_t begin = pte_pa(pt, low, level);
      const uint64_t end = pte_pa(pt, high, level);
      assert(begin <= end);
      if (have_previous) assert(previous_max < begin);
      previous_max = end;
      have_previous = true;
    }
  }
}

int main() {
  const uint64_t page64k = 64ULL * 1024ULL;
  const uint64_t page2m = 2ULL * 1024ULL * 1024ULL;
  const unsigned widths56_64k[] = {10, 10, 10, 10};
  const unsigned widths56_2m[] = {8, 9, 9, 9};
  const unsigned widths49_64k[] = {6, 9, 9, 9};
  const unsigned widths49_2m[] = {7, 7, 7, 7};

  const vm_translation::page_table_config config56;
  assert(config56.valid());
  vm_translation::radix_page_table_backend pt56(config56);
  expect_widths(pt56, page64k, widths56_64k, 4);
  expect_widths(pt56, page2m, widths56_2m, 4);
  assert_disjoint_ranges(pt56);

  // A/B share the 30 high VPN bits in the 56-bit/64KB hierarchy: levels
  // 0..2 therefore name the same PTEs, but their distinct low ten bits keep
  // their leaves distinct.
  const uint64_t shared_upper30 = 0x1234567ULL;
  const vm_translation::translation_key a(0, (shared_upper30 << 10) | 1,
                                          page64k);
  const vm_translation::translation_key b(0, (shared_upper30 << 10) | 2,
                                          page64k);
  assert(pt56.supports_key(a) && pt56.supports_key(b));
  for (unsigned level = 0; level < 3; ++level)
    assert(pte_pa(pt56, a, level) == pte_pa(pt56, b, level));
  assert(pte_pa(pt56, a, 3) != pte_pa(pt56, b, 3));

  // Partial sharing stops exactly at the level where the prefix differs.
  const vm_translation::translation_key partial_a(0, (5ULL << 30) | (1ULL << 20),
                                                  page64k);
  const vm_translation::translation_key partial_b(0, (5ULL << 30) | (2ULL << 20),
                                                  page64k);
  assert(pte_pa(pt56, partial_a, 0) == pte_pa(pt56, partial_b, 0));
  assert(pte_pa(pt56, partial_a, 1) != pte_pa(pt56, partial_b, 1));
  const vm_translation::translation_key unrelated(0, 6ULL << 30, page64k);
  assert(pte_pa(pt56, partial_a, 0) != pte_pa(pt56, unrelated, 0));

  unsigned page_class = 99;
  uint64_t prefix = 0;
  assert(pt56.pte_prefix_identity(a, 2, &page_class, &prefix));
  assert(page_class == 0 && prefix == shared_upper30);

  // 49-bit remains a first-class tested generic configuration; no address is
  // rewritten between this identity and the raw/coalesced SimVA contract.
  const vm_translation::page_table_config config49(4, 49, 1ULL << 49,
                                                     1ULL << 49, 1ULL << 39);
  assert(config49.valid());
  vm_translation::radix_page_table_backend pt49(config49);
  expect_widths(pt49, page64k, widths49_64k, 4);
  expect_widths(pt49, page2m, widths49_2m, 4);
  assert_disjoint_ranges(pt49);

  const uint64_t offender = 0xfffdc0000000c0ULL;
  const vm_translation::translation_key offender_key(0, offender / page64k,
                                                      page64k);
  assert(pt56.supports_key(offender_key));
  assert(pt56.resolve_ppn(offender_key) * page64k + offender % page64k ==
         offender);
  const vm_translation::pte_request offender_pte =
      pt56.make_pte_request(offender_key, 0, 77);
  assert(offender_pte.physical_address != offender);
  assert(offender_pte.is_physical && offender_pte.bypass_translation);
  assert(!pt49.supports_key(offender_key));

  printf("vm_m3_g3_2c_hierarchy_test PASS\n");
  return 0;
}
