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

  // 256 globally shared descriptors are not 128 x 32 static positions.
  {
    mshr_table table(512, 1, 256, 32);
    for (unsigned n = 0; n < 256; ++n)
      table.add_for_test(0x300000 + n * 128, request(n), sector(0));
    assert(table.full(0x400000));
    assert(table.full_reason(0x400000) ==
           mshr_table::EP_L2_BLOCK_DESCRIPTOR_POOL_FULL);
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
