#include <cassert>
#include <cstdint>
#include <cstdio>
#include "../../src/gpgpu-sim/gpu-cache.h"

static mem_fetch *token(unsigned i) {
  return reinterpret_cast<mem_fetch *>(static_cast<uintptr_t>(i + 1));
}
static mem_access_sector_mask_t sector(unsigned i) {
  mem_access_sector_mask_t s; s.set(i); return s;
}
int main() {
  // The ordinary OFF allocator still refuses at the independent limits.
  mshr_table off(2, 1, 4, 2, false);
  off.add_for_test(0x1000, token(0), sector(0));
  off.add_for_test(0x1000, token(1), sector(0));
  assert(off.full_reason(0x1000) == mshr_table::EP_L2_BLOCK_PER_ADDRESS_CAP);
  off.add_for_test(0x2000, token(2), sector(0));
  assert(off.full_reason(0x3000) == mshr_table::EP_L2_BLOCK_LINE_MSHR_FULL);

  // Cross the original pool, line, and same-address limits many times. No
  // guessed large capacity is substituted: real descriptor storage grows.
  for (unsigned count : {1024u, 2048u}) {
    mshr_table ideal(2, 1, 4, 2, true);
    for (unsigned i = 0; i < count; ++i) {
      const new_addr_type line = i < count / 2 ? 0x4000 : 0x8000 + i * 128;
      assert(ideal.full_reason(line) == mshr_table::EP_L2_BLOCK_NONE);
      ideal.add_for_test(line, token(i), sector(i % 4));
    }
    assert(ideal.descriptor_count_used() == count);
    assert(ideal.descriptor_pool_capacity() >= count);
    assert(ideal.num_entries_used() == 1 + count / 2);
    bool atomic = false;
    // Partial fill only releases the matching sectors. A response remains
    // owned until commit; repeated peek is non-destructive.
    ideal.mark_ready(0x4000, atomic, sector(0));
    assert(!atomic);
    assert(ideal.num_response_ready_targets() == count / 8);
    assert(ideal.peek_next_access() == ideal.peek_next_access());
    assert(ideal.descriptor_count_used() == count);
    for (unsigned s = 1; s < 4; ++s) ideal.mark_ready(0x4000, atomic, sector(s));
    for (unsigned i = count / 2; i < count; ++i)
      ideal.mark_ready(0x8000 + i * 128, atomic, sector(i % 4));
    std::set<mem_fetch *> responded;
    while (ideal.access_ready()) {
      assert(responded.insert(ideal.peek_next_access()).second);
      ideal.commit_next_access();
    }
    assert(responded.size() == count);
    assert(ideal.descriptor_count_used() == 0 && ideal.num_entries_used() == 0);
    // Reuse all dynamic IDs after drain.
    ideal.add_for_test(0x100000, token(count + 1), sector(3));
    ideal.mark_ready(0x100000, atomic, sector(3));
    assert(ideal.peek_next_access() == token(count + 1));
    ideal.commit_next_access();
    assert(ideal.descriptor_count_used() == 0);
  }
  std::puts("Lane4 I_META allocator/sector/response/doubling tests: PASS");
}
