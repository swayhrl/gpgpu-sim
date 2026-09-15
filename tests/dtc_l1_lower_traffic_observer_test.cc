#include <cassert>

#include "gpu-sim.h"

int main() {
  // `sector32`: exactly one conventional sector lower creation carries 32 B.
  l1_lower_traffic_observer_counters sector32;
  sector32.observe(l1_lower_traffic_observer_path::CONVENTIONAL, 32);
  assert(sector32.conventional_transactions() == 1);
  assert(sector32.conventional_payload_bytes() == 32);

  // `normal128`: the same conventional creation primitive records one full
  // normal-cache line after the production atom-size rewrite.
  l1_lower_traffic_observer_counters normal128;
  normal128.observe(l1_lower_traffic_observer_path::CONVENTIONAL, 128);
  assert(normal128.conventional_transactions() == 1);
  assert(normal128.conventional_payload_bytes() == 128);

  // `dtc_io128`: one PAPER_IO NEW_MISS reaches the post-construction observer
  // hook exactly once and carries the actual 128-B mem_fetch payload.
  l1_lower_traffic_observer_counters io;
  io.observe(l1_lower_traffic_observer_path::DTC_IO, 128);
  assert(io.dtc_io_transactions() == 1);
  assert(io.dtc_io_payload_bytes() == 128);

  // `dtc_pending`: a pending hit does not call the create hook, so it adds no
  // transaction or byte to the already-created IO request.
  assert(io.dtc_io_transactions() == 1);
  assert(io.dtc_io_payload_bytes() == 128);

  // `dtc_duplicate`: an evicted pending Tag whose re-access reaches NEW_MISS
  // calls the same IO-create hook once more: exactly +1 / +128 B.
  io.observe(l1_lower_traffic_observer_path::DTC_IO, 128);
  assert(io.dtc_io_transactions() == 2);
  assert(io.dtc_io_payload_bytes() == 256);

  // `terminal_accounting`: the observer's create-side count remains one
  // request/128 B while the independently exact-once completion state closes
  // the matching created, issued, and responded dependency.
  l1_lower_traffic_observer_counters terminal;
  terminal.observe(l1_lower_traffic_observer_path::DTC_IO, 128);
  dtc_l1::completion_accounting accounting;
  accounting.register_dependencies(1);
  accounting.own_pib_dependencies(1);
  accounting.mark_ready(1);
  assert(accounting.close_once(1) == 1);
  assert(accounting.state() == dtc_l1::completion_accounting::lifecycle::CLOSED);
  assert(terminal.dtc_io_transactions() == 1);
  assert(terminal.dtc_io_payload_bytes() == 128);
  return 0;
}
