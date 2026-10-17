// Compiles unchanged against both the frozen header and this branch's header.
// Compares deterministic mechanism traces, not host time or full simulations.
#include "dtc-l1-common.h"
#include <cstdio>

template <class Frontend> static void trace(const char *name, bool finite) {
  dtc_l1::config cfg;
  cfg.finite_service = finite;
  cfg.logical_sets = cfg.logical_ways = 1;
  cfg.physical_lines = 4;
  Frontend f(cfg);
  f.advance_service(1);
  assert(f.admit(1) && f.admit(2));
  const auto old = f.access(1, 1, 0);
  const auto merge = f.access(1, 2, 0);
  assert(merge.kind == dtc_l1::io_access_kind::PENDING_HIT);
  assert(f.admit(3));
  const auto next = f.access(1, 3, 128);
  f.complete(old.physical);
  f.advance_service(2);
  std::printf("%s finite=%u miss=%llu pending=%llu tag=%llu allocated=%zu "
              "old=%u:%llu new=%u:%llu\n", name, finite,
              (unsigned long long)f.new_misses(),
              (unsigned long long)f.pending_hits(),
              (unsigned long long)f.tag_requests(), f.allocated_lines(),
              old.physical.id, (unsigned long long)old.physical.generation,
              next.physical.id, (unsigned long long)next.physical.generation);
  f.complete(next.physical);
  f.advance_service(3);
}

int main() {
  for (bool finite : {false, true}) {
    trace<dtc_l1::io_frontend>("IO", finite);
    trace<dtc_l1::oo_frontend>("OO", finite);
  }
  dtc_l1::config cfg;
  cfg.selected_mode = dtc_l1::mode::PAPER_BASE;
  dtc_l1::paper_frontend base(cfg);
  assert(base.try_admit(1));
  assert(base.try_serve_tag(1, 0, 32));
  base.retire(1);
  base.assert_accounting();
  std::printf("BASE pib=%zu admits=%llu retires=%llu\n",base.pib_occupancy(),
      (unsigned long long)base.admits(), (unsigned long long)base.retires());
  dtc_l1::sector_oo_frontend modern(cfg);
  assert(modern.admit(1));
  const auto line = modern.access(1, 1, 0, 0x5);
  modern.complete_sector(line.physical, 2);
  modern.complete_sector(line.physical, 0);
  assert(modern.retire_ready_uid(2, 1));
  modern.assert_shadow_refs();
  std::printf("MODERN refs=%llu allocated=%zu pib=%zu\n",
      (unsigned long long)modern.active_refs(), modern.allocated_lines(),
      modern.occupancy());
}
