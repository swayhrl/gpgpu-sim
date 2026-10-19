// PREPARED_NOT_TESTED. Execute only after coordinator authority/role admission.
// Include the separately patched c8090115 diagnostic Core src/gpgpu-sim.
#include <cassert>
#include <csignal>
#include <cstdint>
#include <initializer_list>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include "dtc-l1-common.h"

static void rejects(unsigned width, unsigned mode, bool finite) {
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    dtc_l1::diagnostic_collection_width(width, mode, finite);
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

static void conservation(unsigned width) {
  dtc_l1::service_quota port(width);
  dtc_l1::service_entry a, b;
  a.observe_ready(10, 7);
  b.observe_ready(10, 3);
  assert(!a.collect(10, port) && !b.collect(10, port));
  unsigned moved = 0;
  for (uint64_t cycle = 11; cycle < 24; ++cycle) {
    const unsigned before = a.sectors_left + b.sectors_left;
    a.collect(cycle, port);
    b.collect(cycle, port);
    // Multiple LD/ST invocations in one absolute cycle do not refill quota.
    a.collect(cycle, port);
    b.collect(cycle, port);
    const unsigned delta = before - a.sectors_left - b.sectors_left;
    assert(delta <= width);
    moved += delta;
    assert(moved + a.sectors_left + b.sectors_left == 10);
  }
  assert(moved == 10 && !a.sectors_left && !b.sectors_left);
  assert(a.collect_start_cycle == 11);
  assert(a.collect_done_cycle > a.ready_cycle);
  // Model a denied WB at eligibility; production WB integration is separate.
  const auto done = a.collect_done_cycle;
  assert(a.collect(done + 1, port));
  assert(a.collect(done + 2, port));
  assert(a.collect_done_cycle == done && !a.sectors_left);
}

int main() {
  rlimit limit{0,0}; assert(setrlimit(RLIMIT_CORE, &limit) == 0);
  constexpr auto oo = static_cast<unsigned>(dtc_l1::mode::PAPER_OO);
  assert(dtc_l1::diagnostic_collection_width(0, oo, true) == 4);
  assert(dtc_l1::diagnostic_collection_width(0, 0, false) == 4);
  assert(dtc_l1::diagnostic_collection_width(1, oo, true) == 1);
  for (unsigned width : {1U, 4U}) conservation(width);
  // ExplicitOFF has exactly the same service-entry trace as original4.
  dtc_l1::service_quota old4(4), off(dtc_l1::diagnostic_collection_width(0,oo,true));
  dtc_l1::service_entry a, b;
  a.observe_ready(2, 9); b.observe_ready(2, 9);
  for (uint64_t cycle = 2; cycle < 12; ++cycle) {
    assert(a.collect(cycle, old4) == b.collect(cycle, off));
    assert(a.sectors_left == b.sectors_left);
    assert(a.collect_done_cycle == b.collect_done_cycle);
    assert(a.collect_start_cycle == b.collect_start_cycle);
  }
  // Completion edge and exact per-cycle1 cap.
  dtc_l1::service_quota one(1);
  dtc_l1::service_entry edge; edge.observe_ready(20, 2);
  assert(!edge.collect(20, one));
  assert(!edge.collect(21, one) && edge.sectors_left == 1);
  assert(!edge.collect(21, one) && edge.sectors_left == 1);
  assert(!edge.collect(22, one) && edge.sectors_left == 0);
  assert(edge.collect(23, one));
  rejects(2, oo, true); rejects(4, oo, true); rejects(1, oo, false);
  for (auto mode : {dtc_l1::mode::LEGACY, dtc_l1::mode::PAPER_BASE,
                    dtc_l1::mode::PAPER_IO, dtc_l1::mode::MODERN_OO_SECTOR})
    rejects(1, static_cast<unsigned>(mode), true);
}
