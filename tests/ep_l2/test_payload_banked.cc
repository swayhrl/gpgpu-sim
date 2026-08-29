#include <assert.h>
#include <stdio.h>
#include "../../src/gpgpu-sim/gpu-cache.h"
int main() {
  ep_l2_payload_store banked(2);
  // IDs 0,1,2,3 map to four independent banks; ID 4 collides with oldest ID 0.
  for (unsigned id = 0; id < 4; ++id)
    assert(banked.request(ep_l2_payload_store::RESIDENT, id, false, 9));
  assert(!banked.request(ep_l2_payload_store::RESIDENT, 4, true, 9));
  assert(banked.bank_requests() == 5 && banked.bank_grants() == 4 &&
         banked.bank_conflicts() == 1);
  // The retried loser receives the next cycle's bank grant; no request is lost.
  assert(banked.request(ep_l2_payload_store::RESIDENT, 4, true, 10));
  assert(banked.bank_grants() == 5);
  for (unsigned i = 0; i < 1024; ++i) banked.assign_resident(i, i, NULL);
  for (unsigned i = 0; i < 128; ++i) banked.assign_bypass(i, 1024 + i, NULL);
  assert(banked.resident_used() == 1024 && banked.bypass_used() == 128);
  puts("EP-L2 C6 Banked payload regression: PASS"); return 0;
}
