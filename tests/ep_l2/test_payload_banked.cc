#include <assert.h>
#include <stdio.h>
#include "../../src/gpgpu-sim/gpu-cache.h"

typedef ep_l2_payload_store store;

static void idle_bank_direct_grant() {
  store s(2);
  assert(s.request(store::RESIDENT, 0, false, 10) == store::GRANTED);
  assert(s.bank_logical_ops() == 1 && s.bank_attempts() == 1);
  assert(s.bank_grants() == 1 && s.bank_retry_attempts() == 0);
  assert(s.bank_true_conflict_ops() == 0 && s.bank_true_conflict_events() == 0);
  assert(s.bank_wait_cycles() == 0 && s.pending_operations() == 0);
}

static void different_banks_parallel_grant() {
  store s(2);
  assert(s.request(store::RESIDENT, 0, false, 20) == store::GRANTED);
  assert(s.request(store::RESIDENT, 1, false, 20) == store::GRANTED);
  assert(s.bank_logical_ops() == 2 && s.bank_attempts() == 2 &&
         s.bank_grants() == 2 && s.bank_true_conflict_events() == 0);
}

static void same_bank_contention() {
  store s(2);
  assert(s.request(store::RESIDENT, 0, false, 30, 10) == store::GRANTED);
  assert(s.request(store::RESIDENT, 4, true, 30, 20) ==
         store::BANK_TRUE_CONTENTION);
  assert(s.bank_logical_ops() == 2 && s.bank_grants() == 1 &&
         s.bank_true_conflict_ops() == 1 && s.bank_true_conflict_events() == 1 &&
         s.pending_operations() == 1);
  assert(s.request(store::RESIDENT, 4, true, 31, 20) == store::GRANTED);
  assert(s.bank_grants() == 2 && s.bank_retry_attempts() == 1 &&
         s.bank_wait_cycles() == 1 && s.pending_operations() == 0);
}

static void older_pending_wins() {
  store s(2);
  // ID4 becomes the older pending op after ID0 consumes bank 0 at cycle 40.
  assert(s.request(store::RESIDENT, 0, false, 40, 10) == store::GRANTED);
  assert(s.request(store::RESIDENT, 4, true, 40, 20) ==
         store::BANK_TRUE_CONTENTION);
  // At cycle 41 ID4 is selected before the newer ID8 arrives.  Calling ID8
  // first must not let it bypass that selected oldest op.
  assert(s.request(store::RESIDENT, 8, false, 41, 30) ==
         store::BANK_TRUE_CONTENTION);
  assert(s.request(store::RESIDENT, 4, true, 41, 20) == store::GRANTED);
  assert(s.request(store::RESIDENT, 8, false, 42, 30) == store::GRANTED);
  assert(s.bank_grants() == 3 && s.pending_operations() == 0);
}

static void repeated_contention_drains_once() {
  store s(2);
  const unsigned ids[] = {0, 4, 8, 12, 16};
  // Five ready operations map to bank 0.  The first executes immediately;
  // all later operations must remain pending exactly once and drain oldest
  // first despite deliberately hostile retry order.
  for (unsigned i = 0; i < 5; ++i) {
    store::request_result result = s.request(store::RESIDENT, ids[i], false, 50, i);
    assert(result == (i == 0 ? store::GRANTED : store::BANK_TRUE_CONTENTION));
  }
  for (unsigned cycle = 51; cycle < 55; ++cycle) {
    const unsigned selected = cycle - 50;
    for (unsigned i = 4; i > selected; --i)
      assert(s.request(store::RESIDENT, ids[i], false, cycle, i) ==
             store::BANK_TRUE_CONTENTION);
    assert(s.request(store::RESIDENT, ids[selected], false, cycle,
                     selected) == store::GRANTED);
  }
  assert(s.bank_logical_ops() == 5 && s.bank_grants() == 5 &&
         s.pending_operations() == 0);
}

int main() {
  idle_bank_direct_grant();
  different_banks_parallel_grant();
  same_bank_contention();
  older_pending_wins();
  repeated_contention_drains_once();

  // Keep the explicit directed oldest-ready API covered too.
  store banked(2);
  banked.enqueue(store::RESIDENT, 4, true, 20);
  banked.enqueue(store::RESIDENT, 0, false, 10);
  banked.enqueue(store::RESIDENT, 1, false, 30);
  unsigned long long granted = 0;
  assert(banked.grant_oldest(store::RESIDENT, 0, false, 9, &granted));
  assert(granted == 10);
  assert(banked.grant_oldest(store::RESIDENT, 1, false, 9, &granted));
  assert(granted == 30);
  assert(banked.grant_oldest(store::RESIDENT, 4, true, 10, &granted));
  assert(granted == 20 && banked.pending_operations() == 0);
  for (unsigned i = 0; i < 1024; ++i) banked.assign_resident(i, i, NULL);
  for (unsigned i = 0; i < 128; ++i) banked.assign_bypass(i, 1024 + i, NULL);
  assert(banked.resident_used() == 1024 && banked.bypass_used() == 128);
  puts("EP-L2 C6c Banked payload arbitration regression: PASS");
  return 0;
}
