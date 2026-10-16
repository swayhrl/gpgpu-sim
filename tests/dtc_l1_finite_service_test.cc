#include <cassert>
#include "dtc-l1-common.h"

template <class Frontend>
void pipelined_lookup_check() {
  dtc_l1::config cfg;
  cfg.finite_service = true;
  cfg.logical_sets = 32;
  cfg.logical_ways = 1;
  cfg.physical_lines = 4;
  Frontend frontend(cfg);
  frontend.advance_service(1);
  assert(frontend.admit(777));
  dtc_l1::service_entry pipe;
  assert(frontend.try_serve_tag(1, 0));
  pipe.tag_grant_cycle = 1;
  assert(!pipe.tag_can_commit(1));
  // Three same-bank references commit at cycles2,3,4: latency1, II1.
  // Committing the old lookup overlaps granting the next; it is not six cycles.
  for (unsigned ref = 0; ref < 3; ++ref) {
    const uint64_t cycle = ref + 2;
    frontend.advance_service(cycle);
    assert(pipe.tag_can_commit(cycle));
    assert(frontend.access(cycle,777,ref*512).kind == dtc_l1::io_access_kind::NEW_MISS);
    pipe.prefetch_next_tag(frontend,cycle,ref!=2,(ref+1)*512);
    assert(!pipe.tag_can_commit(cycle));
  }
  assert(frontend.tag_requests() == 3);
  assert(pipe.tag_grant_cycle == UINT64_MAX);
}

int main() {
  pipelined_lookup_check<dtc_l1::io_frontend>();
  pipelined_lookup_check<dtc_l1::oo_frontend>();
  // Same production predicate guards IO and OO sector-return ownership.
  uint8_t returned = 0;
  for (unsigned sector : {2U, 0U, 3U, 1U}) {
    assert(dtc_l1::response_sector_is_new(0xF, returned, sector));
    returned |= 1U << sector;
    assert(!dtc_l1::response_sector_is_new(0xF, returned, sector));
  }
  assert(returned == 0xF);
  assert(!dtc_l1::response_sector_is_new(0x1, 0, 2));
  assert(!dtc_l1::response_sector_is_new(0xF, 0, 4));
  // Repeated LD/ST invocations cannot replenish an absolute-cycle port.
  dtc_l1::service_quota quota(1);
  assert(quota.available(4));
  assert(quota.available(4));  // failed ownership check consumes nothing
  quota.commit(4);
  assert(!quota.available(4));
  assert(quota.available(5));
  quota.commit(5);
  assert(!quota.available(5));

  // Production collection stage: independent refill/collection ports and
  // absolute-cycle transfer quota. A blocked writeback never reacquires data.
  dtc_l1::service_quota collection(4);
  dtc_l1::service_entry a, b;
  a.observe_ready(1, 6);
  b.observe_ready(1, 1);
  assert(!a.collect(1, collection));
  assert(a.sectors_left == 6);
  assert(!a.collect(2, collection));
  assert(a.sectors_left == 2);
  assert(!a.collect(2, collection));
  assert(a.sectors_left == 2);
  assert(!b.collect(2, collection) && b.sectors_left == 1);
  assert(!a.collect(3, collection) && a.sectors_left == 0);
  assert(!b.collect(3, collection) && b.sectors_left == 0);
  assert(a.collect(4, collection) && b.collect(4, collection));
  assert(a.collect(5, collection));  // retry after collector denial
  assert(a.collect_done_cycle == 3 && a.collect_start_cycle == 2);

  dtc_l1::config cfg;
  cfg.finite_service = true;
  cfg.logical_sets = cfg.logical_ways = 1;
  cfg.physical_lines = 4;
  cfg.oo_pib_entries = 8;
  dtc_l1::oo_frontend oo(cfg);
  oo.advance_service(1);
  assert(oo.admit(1));
  const auto first = oo.access(1, 1, 0);
  for (unsigned uid = 2; uid <= 6; ++uid) {
    assert(oo.admit(uid));
    const auto merged = oo.access(1, uid, 0);
    assert(merged.kind == dtc_l1::io_access_kind::PENDING_HIT);
    assert(merged.physical.id == first.physical.id);
  }
  assert(oo.new_misses() == 1);
  oo.complete(first.physical);
  assert(!oo.entry_ready_for(1));
  oo.advance_service(1);
  assert(oo.wakeups() == 0);  // no same-step visibility
  oo.advance_service(2);
  assert(oo.wakeups() == 4);
  assert(!oo.entry_ready_for(1));  // partly updated line is not observable-ready
  oo.advance_service(2);
  assert(oo.wakeups() == 4);
  assert(oo.admit(7));
  assert(oo.access(2, 7, 0).kind == dtc_l1::io_access_kind::PENDING_HIT);
  // Replace the Tag while the original generation is still returning.
  assert(oo.admit(8));
  const auto replacement = oo.access(2, 8, 128);
  assert(!oo.tag_visible(first.physical));
  assert(oo.is_allocated(first.physical));
  oo.advance_service(3);
  assert(oo.wakeups() == 7);
  assert(oo.entry_ready_for(1) && oo.entry_ready_for(7));
  // Failed external writeback has not called retire: all seven Refs survive.
  assert(oo.ref_count(first.physical) == 7);
  for (unsigned uid = 1; uid <= 7; ++uid) {
    oo.advance_service(3 + uid);
    assert(oo.retire_ready_uid(3 + uid, uid));
    if (uid != 7) assert(oo.is_allocated(first.physical));
  }
  assert(!oo.is_allocated(first.physical));
  assert(oo.final_ref_reclaims() == 1);
  assert(oo.map_loss_to_free_sum() == 8);
  assert(oo.map_loss_to_free_count() == 1);
  const unsigned old_slot = oo.entry_slot(8);
  oo.advance_service(11);
  oo.complete(replacement.physical);
  oo.advance_service(12);
  assert(oo.retire_ready_uid(12, 8));
  assert(oo.admit(9));
  assert(oo.entry_slot(9) <= old_slot);
  const auto reused = oo.access(12, 9, 256);
  oo.complete(reused.physical);
  oo.advance_service(13);
  assert(oo.retire_ready_uid(13, 9));
  assert(oo.occupancy() == 0 && oo.active_refs() == 0);

  dtc_l1::io_frontend io(cfg);
  io.advance_service(1);
  assert(io.admit(1));
  const auto io_first = io.access(1, 1, 0);
  io.complete(io_first.physical);
  assert(!io.head_ready());
  io.advance_service(1);
  assert(!io.head_ready());
  io.advance_service(2);
  assert(io.head_ready() && io.retire_head());
  assert(io.admit(2));
  const auto io_second = io.access(2, 2, 128);
  io.complete(io_second.physical);
  io.advance_service(3);
  assert(io.retire_head());
  assert(io.free_lines() == cfg.physical_lines - 1);
}
