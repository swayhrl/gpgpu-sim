#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../src/gpgpu-sim/gpu-cache.h"

static mem_fetch *request(unsigned id) {
  return reinterpret_cast<mem_fetch *>(static_cast<uintptr_t>(id + 1));
}

static mem_access_sector_mask_t sector(unsigned number) {
  mem_access_sector_mask_t sectors;
  sectors.set(number);
  return sectors;
}

static void masks(mshr_table &table, new_addr_type line, unsigned pending,
                  unsigned issued, unsigned ready) {
  mem_access_sector_mask_t p, i, r;
  table.sector_masks(line, p, i, r);
  assert(p.to_ulong() == pending);
  assert(i.to_ulong() == issued);
  assert(r.to_ulong() == ready);
}

int main() {
  // 128 lines accepted; the 129th independent line must expose the line-MSHR
  // blocker even though descriptors remain available.
  {
    mshr_table table(128, 1, 256, 32);
    for (unsigned n = 0; n < 128; ++n)
      table.add_for_test(0x100000 + n * 128, request(n), sector(0));
    assert(table.full(0x200000));
    assert(table.full_reason(0x200000) == mshr_table::EP_L2_BLOCK_LINE_MSHR_FULL);
  }

  // Line-MSHR capacity is independently parameterized from descriptors.  At
  // 256 entries, verify both sides of the legacy boundary (127/128/129) and
  // the new capacity boundary (255/256), then release/reuse one entry.  Every
  // address is distinct, and descriptor capacity is deliberately larger, so
  // neither the per-address cap nor descriptor pool can mask LINE_MSHR_FULL.
  {
    const new_addr_type base = 0x220000;
    const new_addr_type replacement = 0x330000;
    mshr_table table(256, 1, 1024, 32);
    for (unsigned n = 0; n < 127; ++n)
      table.add_for_test(base + n * 128, request(10000 + n), sector(0));
    assert(table.num_entries_used() == 127);
    assert(!table.full(base + 127 * 128));
    table.add_for_test(base + 127 * 128, request(10127), sector(0));
    assert(table.num_entries_used() == 128);
    assert(!table.full(base + 128 * 128));
    table.add_for_test(base + 128 * 128, request(10128), sector(0));
    assert(table.num_entries_used() == 129);
    for (unsigned n = 129; n < 255; ++n)
      table.add_for_test(base + n * 128, request(10000 + n), sector(0));
    assert(table.num_entries_used() == 255);
    assert(!table.full(base + 255 * 128));
    table.add_for_test(base + 255 * 128, request(10255), sector(0));
    assert(table.num_entries_used() == 256);
    assert(table.full(replacement));
    assert(table.full_reason(replacement) ==
           mshr_table::EP_L2_BLOCK_LINE_MSHR_FULL);

    bool atomic = false;
    table.mark_ready(base, atomic, sector(0));
    assert(table.peek_next_access() == request(10000));
    table.commit_next_access();
    assert(table.num_entries_used() == 255);
    assert(!table.full(replacement));
    table.add_for_test(replacement, request(11000), sector(0));
    assert(table.num_entries_used() == 256);

    for (unsigned n = 1; n < 256; ++n) {
      table.mark_ready(base + n * 128, atomic, sector(0));
      assert(table.peek_next_access() == request(10000 + n));
      table.commit_next_access();
    }
    table.mark_ready(replacement, atomic, sector(0));
    assert(table.peek_next_access() == request(11000));
    table.commit_next_access();
    assert(table.num_entries_used() == 0);
    assert(table.descriptor_count_used() == 0);
  }

  // 256 globally shared descriptors are not 128 x 32 static positions.
  {
    mshr_table table(512, 1, 256, 32);
    for (unsigned n = 0; n < 256; ++n)
      table.add_for_test(0x300000 + n * 128, request(n), sector(0));
    assert(table.full(0x400000));
    assert(table.full_reason(0x400000) ==
           mshr_table::EP_L2_BLOCK_DESCRIPTOR_POOL_FULL);
  }

  // D512 keeps the same allocator/lifetime semantics while moving only the
  // global pool boundary.  Exercise the old boundary (255/256/257), the new
  // boundary (511/512), full-pool rejection, and release/reuse.
  {
    // Use more line entries than descriptor slots so this isolates the
    // global descriptor boundary from the independent line-MSHR boundary.
    mshr_table table(1024, 1, 512, 32);
    for (unsigned n = 0; n < 257; ++n)
      table.add_for_test(0x410000 + n * 128, request(1000 + n), sector(0));
    assert(table.descriptor_count_used() == 257);
    assert(!table.full(0x510000));
    for (unsigned n = 257; n < 512; ++n)
      table.add_for_test(0x410000 + n * 128, request(1000 + n), sector(0));
    assert(table.descriptor_count_used() == 512);
    assert(table.full(0x610000));
    assert(table.full_reason(0x610000) ==
           mshr_table::EP_L2_BLOCK_DESCRIPTOR_POOL_FULL);

    // Releasing one descriptor restores exactly one global-pool slot and
    // does not alter the independent 32-requester/address cap.
    bool atomic = false;
    table.mark_ready(0x410000, atomic, sector(0));
    assert(table.peek_next_access() == request(1000));
    table.commit_next_access();
    assert(table.descriptor_count_used() == 511);
    assert(!table.full(0x610000));
    table.add_for_test(0x610000, request(2000), sector(0));
    assert(table.descriptor_count_used() == 512);
    // The pool is full above, so use a fresh D512 table to prove that the
    // per-address cap is neither widened nor coupled to global capacity.
    mshr_table per_address(1024, 1, 512, 32);
    for (unsigned n = 0; n < 32; ++n)
      per_address.add_for_test(0x710000, request(2200 + n), sector(0));
    assert(per_address.descriptor_count_used() == 32);
    assert(per_address.full(0x710000));
    assert(per_address.full_reason(0x710000) ==
           mshr_table::EP_L2_BLOCK_PER_ADDRESS_CAP);
  }

  // The 33rd requester for one line is a per-address, not global, block.
  {
    mshr_table table(128, 1, 256, 32);
    for (unsigned n = 0; n < 32; ++n)
      table.add_for_test(0x500000, request(n), sector(0));
    assert(table.full(0x500000));
    assert(table.full_reason(0x500000) == mshr_table::EP_L2_BLOCK_PER_ADDRESS_CAP);
  }

  // Lower issue and fill do not free a descriptor. The only freeing operation
  // is commit_next_access(), called after successful L2->ICNT enqueue.
  {
    mshr_table table(128, 1, 256, 32);
    mem_fetch *mf = request(600);
    mem_access_sector_mask_t s0 = sector(0);
    table.add_for_test(0x600000, mf, s0);
    assert(table.descriptor_count_used() == 1);
    bool atomic = false;
    table.mark_ready(0x600000, atomic, s0);
    assert(table.descriptor_count_used() == 1);
    assert(table.peek_next_access() == mf);
    assert(table.descriptor_count_used() == 1);
    table.commit_next_access();
    assert(table.descriptor_count_used() == 0);
  }

  // Two 32-B sectors of one 128-B line share one MSHR, keep distinct masks,
  // and each response becomes ready exactly once.
  {
    const new_addr_type line = 0x700000;
    mshr_table table(128, 1, 256, 32);
    mem_fetch *s0 = request(700);
    mem_fetch *s1 = request(701);
    mem_access_sector_mask_t sector0 = sector(0);
    mem_access_sector_mask_t sector1 = sector(1);
    assert(table.needs_lower_read(line, sector0));
    table.add_for_test(line, s0, sector0);
    assert(!table.needs_lower_read(line, sector0));
    assert(table.needs_lower_read(line, sector1));
    table.add_for_test(line, s1, sector1);
    assert(table.num_entries_used() == 1);
    masks(table, line, 0x3, 0x3, 0x0);
    bool atomic = false;
    table.mark_ready(line, atomic, sector0);
    masks(table, line, 0x2, 0x3, 0x1);
    assert(table.peek_next_access() == s0);
    table.commit_next_access();
    assert(!table.access_ready());
    table.mark_ready(line, atomic, sector1);
    masks(table, line, 0x0, 0x3, 0x3);
    assert(table.peek_next_access() == s1);
    table.commit_next_access();
    assert(table.descriptor_count_used() == 0);
  }

  puts("EP-L2 C3 descriptor/MSHR directed tests: PASS");
  return 0;
}
