#include <cassert>
#include <cstdint>
#include <vector>

#include "dtc-l1-common.h"

using dtc_l1::config;
using dtc_l1::group_128b_references;
using dtc_l1::mode;
using dtc_l1::paper_frontend;
using dtc_l1::sector_access;

static std::vector<sector_access> one_per_line(unsigned count) {
  std::vector<sector_access> accesses;
  for (unsigned lane = 0; lane < count; ++lane)
    accesses.push_back({static_cast<uint64_t>(lane) * 128, 1});
  return accesses;
}

int main() {
  // B01A-D: grouping represents logical 128B references, not a new coalescer.
  assert(group_128b_references(std::vector<sector_access>(32, {0, 1})).size() == 1);
  assert(group_128b_references(one_per_line(2)).size() == 2);
  assert(group_128b_references(one_per_line(4)).size() == 4);
  assert(group_128b_references(one_per_line(32)).size() == 32);

  // B01E: different existing 32B accesses within a 128B line preserve masks.
  const auto masked = group_128b_references({{0, 0x1}, {32, 0x2}, {64, 0x4}});
  assert(masked.size() == 1);
  assert(masked.front().sector_mask == 0x7);

  config cfg;
  cfg.selected_mode = mode::PAPER_BASE;
  cfg.pib_entries = 2;
  paper_frontend front_end(cfg);

  // B02/B03 accounting: the third live instruction cannot enter until a
  // caller reports the first instruction's true completion.
  assert(front_end.try_admit(10));
  assert(front_end.try_admit(11));
  assert(!front_end.try_admit(12));
  assert(front_end.pib_occupancy() == 2);
  assert(front_end.pib_peak() == 2);
  assert(front_end.pib_full_events() == 1);
  front_end.retire(10);
  assert(front_end.try_admit(12));
  front_end.sample_cycle(99);
  assert(front_end.pib_occupancy_cycle_sum() == 2);
  assert(front_end.pib_occupancy_sample_cycles() == 1);
  assert(front_end.admits() - front_end.retires() == front_end.pib_occupancy());

  // B04: same-bank work serializes.  With 32 logical sets, lines 0 and 4
  // map to logical sets 0 and 4, hence Tag bank 0 in both cases.
  assert(front_end.try_serve_tag(100, 0, 32));
  assert(!front_end.try_serve_tag(100, 4 * 128, 32));
  assert(front_end.try_serve_tag(101, 4 * 128, 32));

  // B05: four different Tag banks can each serve one reference in one cycle.
  for (unsigned bank = 0; bank < 4; ++bank)
    assert(front_end.try_serve_tag(200, bank * 128, 32));
  assert(!front_end.try_serve_tag(200, 4 * 128, 32));
  assert(front_end.requests_per_bank().size() == 4);
  for (const uint64_t requests : front_end.requests_per_bank())
    assert(requests >= 1);
  front_end.assert_accounting();
  return 0;
}
