#include "dtc-l1-invalidation.h"
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

using namespace dtc_l1;

static config small(bool finite, unsigned physical = 4) {
  config cfg;
  cfg.selected_mode = mode::PAPER_OO;
  cfg.finite_service = finite;
  cfg.logical_sets = cfg.logical_ways = 1;
  cfg.physical_lines = physical;
  cfg.oo_pib_entries = 16;
  return cfg;
}

static void witness(const char *id, const oo_frontend &oo) {
  oo.assert_shadow_refs();
  std::printf("{\"case\":\"%s\",\"local_cpu_check\":\"PASS\","
              "\"allocated\":%zu,\"refs\":%llu,\"pib\":%zu}\n",
              id, oo.allocated_lines(),
              (unsigned long long)oo.active_refs(), oo.occupancy());
}

template <class Action> static void must_abort(Action action) {
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) { action(); _exit(3); }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

static void retained_ready(bool finite) {
  oo_frontend oo(small(finite, 1));
  oo.advance_service(1);
  assert(oo.admit(1));
  const auto old = oo.access(1, 1, 0).physical;
  oo.complete(old);
  oo.advance_service(2);
  assert(oo.retire_ready_uid(2, 1));
  assert(oo.tag_visible(old) && !oo.ref_count(old));
  service_entry granted;
  assert(oo.try_serve_tag(4, 0));
  granted.tag_grant_cycle = 4; // No physical identity is latched by the grant.
  // Source event may follow the last service edge. It must not advance that
  // edge, wrap the map-loss duration, or require another instruction to free.
  const auto delta = oo.invalidate_lookup(5);
  assert(delta.changed == 1 && delta.reclaimed == 1 && !delta.retained);
  assert(!oo.is_allocated(old));
  assert(!oo.invalidate_lookup(5).changed);
  assert(!oo.invalidate_lookup(6).changed);
  assert(oo.map_loss_to_free_count() == (finite ? 1U : 0U));
  assert(oo.map_loss_to_free_sum() == 0);
  assert(granted.tag_can_commit(6));
  assert(oo.admit(2));
  oo.advance_service(6);
  const auto next = oo.access(6, 2, 0).physical;
  assert(next.id == old.id && next.generation == old.generation + 1);
  must_abort([&] { oo.complete(old); });
  oo.complete(next);
  oo.advance_service(7);
  assert(oo.retire_ready_uid(7, 2));
  assert(oo.invalidate_lookup(7).reclaimed == 1);
  witness(finite ? "K1-ready-ref0-finite-stale-generation" :
                   "K1-ready-ref0-legacy-service-stale-generation", oo);
}

static void old_new_and_sectors(bool finite) {
  oo_frontend oo(small(finite, 2));
  oo.advance_service(1);
  assert(oo.admit(11));
  const auto old = oo.access(1, 11, 0).physical;
  const unsigned old_slot = oo.entry_slot(11);
  const uint64_t old_slot_generation = oo.entry_slot_generation(11);
  const auto delta = oo.invalidate_lookup(1);
  assert(delta.changed == 1 && delta.retained == 1 && !delta.reclaimed);
  assert(oo.fill_identity_matches(old) && !oo.tag_visible(old));
  assert(oo.ref_count(old) == 1);
  assert(oo.admit(12));
  const auto next = oo.access(1, 12, 0).physical;
  assert(next.id != old.id);
  assert(oo.entry_slot_generation(11) == old_slot_generation);
  assert(oo.will_new_miss(0) == false);
  // The integration's unchanged sector predicate keeps separate masks per
  // UID. No old fragment is routed by re-looking up the new address mapping.
  uint8_t old_mask = 0, new_mask = 0;
  for (unsigned sector : {2U, 0U, 3U}) {
    assert(response_sector_is_new(0xF, old_mask, sector));
    old_mask |= 1U << sector;
  }
  assert(!oo.entry_ready_for(11) && !oo.entry_ready_for(12));
  assert(!response_sector_is_new(0xF, old_mask, 2));
  assert(!response_sector_is_new(0x1, 0, 3));
  for (unsigned sector : {3U, 1U, 0U, 2U}) {
    assert(response_sector_is_new(0xF, new_mask, sector));
    new_mask |= 1U << sector;
  }
  assert(new_mask == 0xF);
  oo.complete(next);
  oo.advance_service(2);
  assert(oo.entry_ready_for(12) && !oo.entry_ready_for(11));
  assert(oo.retire_ready_uid(2, 12));
  assert(response_sector_is_new(0xF, old_mask, 1));
  old_mask |= 1U << 1;
  assert(old_mask == 0xF);
  oo.complete(old);
  oo.advance_service(3);
  assert(oo.entry_ready_for(11) && oo.is_allocated(old));
  assert(oo.ref_count(old) == 1);  // WB has not accepted this consumer yet.
  assert(oo.retire_ready_uid(3, 11));
  assert(!oo.is_allocated(old));
  assert(oo.map_loss_to_free_count() == (finite ? 1U : 0U));
  assert(oo.map_loss_to_free_sum() == (finite ? 2U : 0U));
  assert(oo.admit(13));
  assert(oo.entry_slot(13) == old_slot);
  assert(oo.entry_slot_generation(13) > old_slot_generation);
  oo.add_external_dependency(13);
  oo.complete_external_dependency(13);
  assert(oo.retire_ready_uid(4, 13));
  assert(oo.invalidate_lookup(4).reclaimed == 1);
  witness(finite ? "K1-old-new-sectors-slot-reuse-finite" :
                   "K1-old-new-sectors-slot-reuse", oo);
}

static void metadata_and_late_waiter() {
  oo_frontend oo(small(true));
  oo.advance_service(1);
  assert(oo.admit(1));
  const auto old = oo.access(1, 1, 0).physical;
  for (unsigned uid = 2; uid <= 6; ++uid) {
    assert(oo.admit(uid));
    assert(oo.access(1, uid, 0).kind == io_access_kind::PENDING_HIT);
  }
  oo.complete(old);  // Integration may now erase inflight, but Ref/service live.
  oo.advance_service(2);
  assert(oo.wakeups() == 4 && !oo.entry_ready_for(1));
  assert(oo.admit(7));
  assert(oo.access(2, 7, 0).kind == io_access_kind::PENDING_HIT);
  assert(oo.invalidate_lookup(2).retained == 1);
  assert(!oo.invalidate_lookup(2).changed);
  oo.advance_service(2);
  assert(oo.wakeups() == 4);  // Invalidation cannot replenish metadata quota.
  assert(oo.admit(8));
  const auto next = oo.access(2, 8, 0).physical;
  assert(next.id != old.id && oo.ref_count(old) == 7);
  oo.advance_service(3);
  assert(oo.wakeups() == 7 && oo.entry_ready_for(7));
  for (unsigned uid = 1; uid <= 7; ++uid) {
    oo.advance_service(uid + 3);
    assert(oo.retire_ready_uid(uid + 3, uid));
    assert(oo.is_allocated(old) == (uid != 7));
  }
  assert(oo.map_loss_to_free_sum() == 8 && oo.map_loss_to_free_count() == 1);
  oo.complete(next);
  oo.advance_service(11);
  assert(oo.retire_ready_uid(11, 8));
  assert(oo.invalidate_lookup(11).reclaimed == 1);
  witness("K1-finite-return-cursor-late-waiter-idempotent-map-loss", oo);
}

static void grant_partial_and_pressure() {
  config cfg = small(true, 2);
  cfg.allocation_width = 1;
  oo_frontend oo(cfg);
  oo.advance_service(1);
  assert(oo.admit(1));
  service_entry grant;
  assert(oo.try_serve_tag(1, 0));
  grant.tag_grant_cycle = 1;
  assert(oo.invalidate_lookup(1).changed == 0); // Grant binds no identity.
  assert(grant.tag_can_commit(2));
  oo.advance_service(2);
  const auto first = oo.access(2, 1, 0).physical;
  assert(oo.access(2, 1, 128).kind == io_access_kind::NO_FREE_LINE);
  assert(oo.invalidate_lookup(2).retained == 1);
  assert(oo.access(2, 1, 128).kind == io_access_kind::NO_FREE_LINE);
  assert(oo.ref_count(first) == 1); // Partial allocation survives; no rollback.
  oo.advance_service(3);
  const auto second = oo.access(3, 1, 128).physical;
  assert(oo.active_refs() == 2);
  assert(oo.admit(2));
  assert(oo.access(4, 2, 0).kind == io_access_kind::NO_FREE_LINE);
  oo.complete(first);
  oo.complete(second);
  oo.advance_service(4);
  completion_accounting completion;
  completion.register_dependencies(2);
  completion.own_pib_dependencies(2);
  completion.mark_ready(2);
  assert(completion.close_once(2) == 2);
  assert(oo.retire_ready_uid(4, 1));
  assert(!oo.is_allocated(first));
  oo.advance_service(5);
  const auto replacement = oo.access(5, 2, 0).physical;
  assert(replacement.id == first.id &&
         replacement.generation == first.generation + 1);
  oo.complete(replacement);
  oo.advance_service(6);
  assert(oo.retire_ready_uid(6, 2));
  assert(oo.invalidate_lookup(6).reclaimed == 1);
  witness("K1-grant-cross-event-partial-allocation-pressure-completion", oo);
}

static void collection_and_sidecars() {
  oo_frontend oo(small(true));
  oo.advance_service(1);
  assert(oo.admit(1));
  const auto line = oo.access(1, 1, 0).physical;
  oo.complete(line);
  oo.advance_service(2);
  service_entry collect;
  service_quota port(4);
  collect.observe_ready(2, 6);
  assert(!collect.collect(3, port) && collect.sectors_left == 2);
  assert(oo.invalidate_lookup(3).retained == 1);
  assert(!collect.collect(3, port) && collect.sectors_left == 2);
  assert(!collect.collect(4, port) && !collect.sectors_left);
  assert(collect.collect(5, port));
  // Simulate source WB refusal by withholding the existing retire call.
  assert(!oo.invalidate_lookup(5).changed && oo.ref_count(line) == 1);
  oo.advance_service(6);
  assert(oo.retire_ready_uid(6, 1));
  assert(!oo.is_allocated(line));
  for (unsigned uid = 2; uid <= 5; ++uid) {
    // STORE, ATOMIC, BYPASS, PROXY_FENCE sidecars own no address reference.
    assert(oo.admit(uid));
    oo.add_external_dependency(uid);
    assert(!oo.entry_ready_for(uid));
  }
  assert(oo.active_refs() == 0);
  assert(!oo.invalidate_lookup(6).changed);
  for (unsigned uid = 2; uid <= 5; ++uid) {
    oo.complete_external_dependency(uid);
    assert(oo.retire_ready_uid(uid + 5, uid));
  }
  witness("K1-collection-WB-backpressure-sidecar-no-address-owner", oo);
}

int main() {
  retained_ready(false);
  retained_ready(true);
  old_new_and_sectors(false);
  old_new_and_sectors(true);
  metadata_and_late_waiter();
  grant_partial_and_pressure();
  collection_and_sidecars();
  for (unsigned selected = 0; selected <= 4; ++selected) {
    check_source_invalidation_mode(false, selected);
    if (selected == 3) check_source_invalidation_mode(true, selected);
    else must_abort([=] { check_source_invalidation_mode(true, selected); });
  }
  std::puts("{\"case\":\"K1-scope-OO-only-K0-all-modes\",\"local_cpu_check\":\"PASS\"}");
}
