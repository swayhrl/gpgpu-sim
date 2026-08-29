#include <assert.h>
#include <stdio.h>
#include "../../src/gpgpu-sim/gpu-cache.h"
int main() {
  ep_l2_payload_store banked(2);
  // Same-bank ready operations use their sequence numbers, not enqueue/call
  // order: ID4 was enqueued first but ID0 is older and must win bank 0.
  banked.enqueue(ep_l2_payload_store::RESIDENT, 4, true, 20);
  banked.enqueue(ep_l2_payload_store::RESIDENT, 0, false, 10);
  banked.enqueue(ep_l2_payload_store::RESIDENT, 1, false, 30);
  unsigned long long granted = 0;
  assert(banked.grant_oldest(ep_l2_payload_store::RESIDENT, 0, false, 9, &granted));
  assert(granted == 10);
  // Bank 1 proceeds in the same cycle; different banks are independent.
  assert(banked.grant_oldest(ep_l2_payload_store::RESIDENT, 1, false, 9, &granted));
  assert(granted == 30);
  // The bank-0 loser remains pending and wins only on retry next cycle.
  assert(banked.grant_oldest(ep_l2_payload_store::RESIDENT, 4, true, 10, &granted));
  assert(granted == 20 && banked.pending_operations() == 0);
  // C6c production retry: an oldest same-bank operation can be selected on
  // a cycle where another requester calls first.  Its grant must survive to
  // the later retry; discarding it each cycle makes A/B alternate forever.
  ep_l2_payload_store retry(2);
  assert(!retry.request(ep_l2_payload_store::RESIDENT, 0, false, 20));
  assert(!retry.request(ep_l2_payload_store::RESIDENT, 4, true, 21));
  assert(retry.request(ep_l2_payload_store::RESIDENT, 0, false, 22));
  assert(retry.request(ep_l2_payload_store::RESIDENT, 4, true, 23));
  assert(retry.pending_operations() == 0);
  for (unsigned i = 0; i < 1024; ++i) banked.assign_resident(i, i, NULL);
  for (unsigned i = 0; i < 128; ++i) banked.assign_bypass(i, 1024 + i, NULL);
  assert(banked.resident_used() == 1024 && banked.bypass_used() == 128);
  puts("EP-L2 C6 Banked payload regression: PASS"); return 0;
}
