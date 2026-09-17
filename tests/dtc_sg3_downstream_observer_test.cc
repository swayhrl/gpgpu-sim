#include <cassert>

#include "gpu-sim.h"

int main() {
  sg3_downstream_observer_counters counters;

  // Exact once-per-sample integrals, with no cache/queue object involved.
  counters.sample_dtc_outstanding(3);
  counters.sample_dtc_outstanding(5);
  assert(counters.dtc_core_samples() == 2);
  assert(counters.dtc_outstanding_integral() == 8);
  counters.sample_l2_occupancy(7, 2);
  counters.sample_l2_occupancy(1, 4);
  assert(counters.l2_bank_samples() == 2);
  assert(counters.l2_mshr_integral() == 8);
  assert(counters.l2_miss_queue_integral() == 6);

  // Request identity includes its SM, so equal request UIDs from different
  // SMs cannot alias; completion measures creation to final response only.
  counters.lower_created(1, 9, 100);
  counters.lower_created(2, 9, 120);
  counters.lower_completed(1, 9, 145);
  counters.lower_completed(2, 9, 180);
  assert(counters.lower_lifetime_completed() == 2);
  assert(counters.lower_lifetime_sum_cycles() == 105);
  assert(counters.lower_lifetime_max_cycles() == 60);
  assert(counters.lower_lifetime_live_records() == 0);
  counters.lower_completed(3, 9, 200);
  assert(counters.lower_lifetime_unmatched_completions() == 1);
  return 0;
}
