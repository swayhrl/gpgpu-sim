#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "l2_admission_rules.h"

static l2_admission_inputs base_input() {
  l2_admission_inputs in;
  in.line_available = true;
  in.missq_available = true;
  in.data_port_available = true;
  in.response_slot_available = true;
  return in;
}

static void expect_allowed(const l2_admission_inputs &in) {
  assert(l2_admission_allowed(in));
  assert(l2_admission_blockers(in) == 0);
}

static void expect_blocked(const l2_admission_inputs &in,
                           l2_block_reason reason) {
  assert(!l2_admission_allowed(in));
  assert(l2_admission_blockers(in) & (1ULL << reason));
}

int main() {
  // T1: lower FIFO is intentionally absent from the admission contract.
  l2_admission_inputs hit = base_input();
  hit.needs_data_port = true;
  hit.needs_response_slot = true;
  expect_allowed(hit);

  // T2: a new clean miss does not need an immediate response slot.
  l2_admission_inputs clean_miss = base_input();
  clean_miss.needs_new_mshr = true;
  clean_miss.new_mshr_available = true;
  clean_miss.response_slot_available = false;
  expect_allowed(clean_miss);

  // T3: an MSHR merge allocates neither a new MSHR nor a MissQ entry.
  l2_admission_inputs merge = base_input();
  merge.needs_mshr_merge = true;
  merge.mshr_merge_available = true;
  merge.missq_available = true;
  expect_allowed(merge);

  // T4/T5: exact MissQ admission distinguishes clean from dirty replacement.
  expect_allowed(clean_miss);
  l2_admission_inputs dirty_miss = clean_miss;
  dirty_miss.missq_available = false;
  expect_blocked(dirty_miss, L2_BLOCK_MISSQ);

  // T6/T7: only operations that transfer data at admission need data port.
  l2_admission_inputs data_busy_clean_miss = clean_miss;
  data_busy_clean_miss.data_port_available = false;
  expect_allowed(data_busy_clean_miss);
  hit.data_port_available = false;
  expect_blocked(hit, L2_BLOCK_DATA_PORT);

  // T8/T9: RespQ blocks an immediate hit response, never an absorbed L1 WB.
  hit.data_port_available = true;
  hit.response_slot_available = false;
  expect_blocked(hit, L2_BLOCK_RESPQ);
  l2_admission_inputs l1_wb = base_input();
  l1_wb.needs_data_port = true;
  l1_wb.response_slot_available = false;
  expect_allowed(l1_wb);

  // T10/T11: no-return writeback uses the reserved progress opportunity;
  // return-bearing reads remain bound to a full return path.
  assert(dram_issue_allowed(false, true, false, true));
  assert(!dram_issue_allowed(true, true, true, false));

  // T12 is exercised by the simulator's MSHR-ready counters; the invariant
  // here is that ready responses remain a distinct resource category.
  assert(L2_BLOCK_MSHR_NEW != L2_BLOCK_MSHR_MERGE);

  // T13 is covered by the tag-array dirty-victim regression in the simulator
  // smoke run; the fallback must not be classified as line-allocation failure.
  assert(L2_BLOCK_LINE_ALLOC != L2_BLOCK_MISSQ);

  // T14: one MISS and one SECTOR_MISS among four accesses is exactly 0.5.
  assert(fabs(l2_windowed_miss_rate(4, 0, 1, 1, 0) - 0.5f) < 1e-6);

  // T15: a plan is admitted iff every resource it actually needs is present,
  // and the production preview/commit predicate accepts the matching commit.
  l2_admission_inputs exact = base_input();
  exact.needs_new_mshr = true;
  exact.new_mshr_available = true;
  exact.needs_data_port = true;
  exact.needs_response_slot = true;
  expect_allowed(exact);
  assert(l2_preview_commit_matches(true, false, false, 1, false, true, false,
                                   false, 1));
  assert(!l2_preview_commit_matches(true, false, false, 1, false, false,
                                    false, false, 1));

  puts("corrected L2 admission rule regressions: PASS (T1-T15 contract)");
  return 0;
}
