#include <assert.h>
#include <stdio.h>
#include "../../src/gpgpu-sim/gpu-cache.h"
int main() {
  ep_l2_payload_store legacy(1);
  for (unsigned i = 0; i < 1024; ++i) legacy.assign_resident(i, i * 128, NULL);
  for (unsigned i = 0; i < 128; ++i) legacy.assign_bypass(i, 0x100000 + i, NULL);
  assert(legacy.resident_used() == 1024 && legacy.bypass_used() == 128);
  // Separate physical RAMs: one resident and one bypass read are concurrent.
  assert(legacy.request(ep_l2_payload_store::RESIDENT, 0, false, 7));
  assert(legacy.request(ep_l2_payload_store::BYPASS, 0, false, 7));
  // A second resident read waits, while the resident write port remains free.
  assert(!legacy.request(ep_l2_payload_store::RESIDENT, 1, false, 7));
  assert(legacy.request(ep_l2_payload_store::RESIDENT, 0, true, 7));
  legacy.release_bypass(3); assert(legacy.bypass_used() == 127);
  puts("EP-L2 C5 Legacy payload-store regression: PASS"); return 0;
}
