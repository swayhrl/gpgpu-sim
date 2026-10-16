// Header-only regression source; execution requires approved build admission.
#include <cassert>
#include <cstdio>
#include "../../src/gpgpu-sim/ep-l2-ideal-reference.h"

int main() {
  using ep_l2_ideal::ReferenceMemory;
  ReferenceMemory memory;
  const uint64_t line = 0x100004000ULL;
  ReferenceMemory::Mask all{}, selected{}, other{};
  all.fill(true);
  for (unsigned b = 32; b < 64; ++b) selected[b] = true;
  for (unsigned b = 0; b < 32; ++b) other[b] = true;
  ReferenceMemory::Data data{}; data.fill(91);
  memory.seed(line, std::vector<uint8_t>(128, 17));
  memory.write(line, all, &data);
  auto before = memory.read(line, all);
  assert(before[0].version > 0);
  memory.host_prefill(line, selected);
  auto after = memory.read(line, all);
  for (unsigned b = 0; b < 128; ++b) {
    if (selected[b]) {
      // Replacement host bytes are unknown symbols, never fabricated zero data.
      assert(after[b].version == 0 && !after[b].value_known);
    } else assert(after[b] == before[b]);
  }
  // Snapshots already handed to responses remain immutable value copies.
  assert(before[32].version > 0 && before[32].value_known && before[32].value == 91);
  memory.host_prefill(line, selected);
  assert(memory.read(line, all) == after);
  memory.write(line, other);
  auto next = memory.read(line, all);
  assert(next[0].version > before[0].version && !next[0].value_known);
  assert(next[32].version == 0 && !next[32].value_known);
  // No truncation or reference aliasing of addresses above 4GiB.
  memory.seed(line & 0xffffffffULL, std::vector<uint8_t>(128, 7));
  memory.host_prefill(line, other);
  for (auto b : memory.read(line & 0xffffffffULL, all))
    assert(b.value_known && b.value == 7);
  ReferenceMemory fresh;
  fresh.host_prefill(line, selected);
  for (auto b : fresh.read(line, selected)) assert(b.version == 0 && !b.value_known);
  std::puts("IDEAL_HOST_PREFILL_REFERENCE_SECTOR_SNAPSHOT_MONOTONIC_ADDRESS PASS");
}
