#include <assert.h>
#include <stdio.h>

#include "../../src/gpgpu-sim/gpu-cache.h"

static mem_access_sector_mask_t sector(unsigned index) {
  mem_access_sector_mask_t mask;
  mask.set(index);
  return mask;
}

static void exercise(unsigned mode) {
  ep_l2_payload_store payload(mode);

  // T1: fresh read miss owns one pending 32B sector, then a matching fill
  // clears it and leaves a valid resident payload.
  payload.reserve_resident(0, 0x1000, NULL);
  unsigned g0 = payload.resident(0).generation;
  payload.note_resident_lower_read(0, 0x1000, g0, sector(0));
  assert(payload.resident_pending_sectors(0) == sector(0));
  payload.complete_resident_fill(0, 0x1000, g0, sector(0), false);
  assert(payload.resident_pending_sectors(0).none());
  assert(payload.resident(0).status == ep_l2_payload_store::RESIDENT_VALID);

  // T2: a lazy-fetch local write has no lower read and no pending sector.
  payload.reserve_resident(1, 0x1100, NULL);
  unsigned g1 = payload.resident(1).generation;
  payload.complete_resident_no_fill(1, 0x1100, g1, true);
  assert(payload.resident_pending_sectors(1).none());
  assert(payload.resident(1).status == ep_l2_payload_store::RESIDENT_DIRTY);

  // T3: two sectors of one fresh 128B line use one payload identity and two
  // independent pending bits.  The first fill must not erase the second.
  payload.reserve_resident(2, 0x1200, NULL);
  unsigned g2 = payload.resident(2).generation;
  payload.note_resident_lower_read(2, 0x1200, g2, sector(0));
  payload.note_resident_lower_read(2, 0x1200, g2, sector(1));
  mem_access_sector_mask_t first_two = sector(0) | sector(1);
  assert(payload.resident_pending_sectors(2) == first_two);
  payload.complete_resident_fill(2, 0x1200, g2, sector(0), false);
  assert(payload.resident(2).status == ep_l2_payload_store::RESIDENT_VALID);
  assert(payload.resident_pending_sectors(2) == sector(1));
  payload.complete_resident_fill(2, 0x1200, g2, sector(1), false);
  assert(payload.resident_pending_sectors(2).none());

  // T4: a sector miss after another sector is valid keeps the same payload
  // identity and adds/removes only the missing sector's pending bit.
  payload.assign_resident(3, 0x1300, NULL);
  unsigned g3 = payload.resident(3).generation;
  payload.note_resident_lower_read(3, 0x1300, g3, sector(2));
  assert(payload.resident(3).status == ep_l2_payload_store::RESIDENT_VALID);
  payload.complete_resident_fill(3, 0x1300, g3, sector(2), false);
  assert(payload.resident_pending_sectors(3).none());

  // T5: a dirty resident retains dirty ownership while a missing sector fills.
  payload.assign_resident(4, 0x1400, NULL);
  unsigned g4 = payload.resident(4).generation;
  payload.mark_resident_dirty(4);
  payload.note_resident_lower_read(4, 0x1400, g4, sector(3));
  payload.complete_resident_fill(4, 0x1400, g4, sector(3), false);
  assert(payload.resident(4).status == ep_l2_payload_store::RESIDENT_DIRTY);

  // T6: same-sector requesters merge into one lower fill / one pending bit.
  payload.reserve_resident(5, 0x1500, NULL);
  unsigned g5 = payload.resident(5).generation;
  payload.note_resident_lower_read(5, 0x1500, g5, sector(0));
  assert(payload.resident_pending_sectors(5) == sector(0));
  payload.complete_resident_fill(5, 0x1500, g5, sector(0), false);

  // T7: stale generation and wrong-sector responses are rejected without
  // mutating the current payload; the production completion path asserts on
  // either condition before changing payload state.
  payload.reserve_resident(6, 0x1600, NULL);
  unsigned g6 = payload.resident(6).generation;
  payload.note_resident_lower_read(6, 0x1600, g6, sector(1));
  assert(!payload.resident_fill_expected(6, 0x1600, g6 + 1, sector(1)));
  assert(!payload.resident_fill_expected(6, 0x1600, g6, sector(2)));
  assert(payload.resident_pending_sectors(6) == sector(1));
  payload.complete_resident_fill(6, 0x1600, g6, sector(1), false);

  // T8: a later static landing for the same line retires a stale, completed
  // resident identity.  This models a tag invalidation/replacement boundary
  // without permitting a duplicated payload owner to survive into a later
  // kernel.
  payload.assign_resident(7, 0x1700, NULL);
  payload.reserve_resident(8, 0x1700, NULL);
  assert(payload.resident(7).status == ep_l2_payload_store::FREE);
  assert(payload.resident_owner_matches(8, 0x1700,
                                        payload.resident(8).generation));
  assert(payload.ownership_consistent());
}

int main() {
  exercise(1);  // Legacy
  exercise(2);  // Banked
  puts("EP-L2 C5c sector-aware payload lifetime regression: PASS");
  return 0;
}
