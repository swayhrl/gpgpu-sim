#include <assert.h>
#include <stdio.h>
#include "../../src/gpgpu-sim/gpu-cache.h"
int main() {
  ep_l2_payload_store legacy(1);
  for (unsigned i = 0; i < 1024; ++i) legacy.assign_resident(i, i * 128, NULL);
  for (unsigned i = 0; i < 128; ++i) legacy.assign_bypass(i, 0x100000 + i, NULL);
  assert(legacy.resident_used() == 1024 && legacy.bypass_used() == 128);
  // Separate physical RAMs: one resident and one bypass read are concurrent.
  assert(legacy.request(ep_l2_payload_store::RESIDENT, 0, false, 7) ==
         ep_l2_payload_store::GRANTED);
  assert(legacy.request(ep_l2_payload_store::BYPASS, 0, false, 7) ==
         ep_l2_payload_store::GRANTED);
  // A second resident read waits, while the resident write port remains free.
  assert(legacy.request(ep_l2_payload_store::RESIDENT, 1, false, 7) ==
         ep_l2_payload_store::LEGACY_PORT_BUSY);
  assert(legacy.request(ep_l2_payload_store::RESIDENT, 0, true, 7) ==
         ep_l2_payload_store::GRANTED);
  legacy.release_bypass(3); assert(legacy.bypass_used() == 127);
  // M1 keeps the legacy static partition, but names both partitions in one
  // physical payload-ID namespace.  The static mapping preserves the bank
  // class used by existing arbitration (resident ID i remains i % 4).
  ep_l2_payload_store m1(1);
  m1.assign_resident(7, 0x7000, NULL);
  const ep_l2_payload_store::payload_handle resident = m1.resident_handle(7);
  assert(resident.payload_id == 7 && resident.generation != 0);
  assert(m1.handle_live(resident));
  assert(m1.request(resident, false, 19) == ep_l2_payload_store::GRANTED);
  m1.assign_bypass(9, 0x9000, NULL);
  const ep_l2_payload_store::payload_handle bypass = m1.bypass_handle(9);
  assert(bypass.payload_id == 1024 + 9 && bypass.generation != 0);
  assert(m1.handle_live(bypass));
  // Releasing a slot invalidates the old handle before its next incarnation.
  m1.release_bypass(9);
  assert(!m1.handle_live(bypass));
  m1.assign_bypass(9, 0x9010, NULL);
  const ep_l2_payload_store::payload_handle bypass_reused =
      m1.bypass_handle(9);
  assert(bypass_reused.payload_id == bypass.payload_id &&
         bypass_reused.generation != bypass.generation &&
         m1.handle_live(bypass_reused));
  assert(m1.ownership_consistent());
  puts("EP-L2 C5 Legacy payload-store regression: PASS"); return 0;
}
