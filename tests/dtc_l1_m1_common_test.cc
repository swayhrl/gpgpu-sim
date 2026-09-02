#include <cassert>
#include <cstdint>
#include <vector>

#include "dtc-l1-common.h"

using dtc_l1::config;
using dtc_l1::group_128b_references;
using dtc_l1::mode;
using dtc_l1::paper_frontend;
using dtc_l1::sector_access;

static std::vector<sector_access> one_per_line(unsigned count) {
  std::vector<sector_access> accesses;
  for (unsigned lane = 0; lane < count; ++lane)
    accesses.push_back({static_cast<uint64_t>(lane) * 128, 1});
  return accesses;
}

int main() {
  // B01A-D: grouping represents logical 128B references, not a new coalescer.
  assert(group_128b_references(std::vector<sector_access>(32, {0, 1})).size() == 1);
  assert(group_128b_references(one_per_line(2)).size() == 2);
  assert(group_128b_references(one_per_line(4)).size() == 4);
  assert(group_128b_references(one_per_line(32)).size() == 32);

  // B01E: different existing 32B accesses within a 128B line preserve masks.
  const auto masked = group_128b_references({{0, 0x1}, {32, 0x2}, {64, 0x4}});
  assert(masked.size() == 1);
  assert(masked.front().sector_mask == 0x7);

  config cfg;
  cfg.selected_mode = mode::PAPER_BASE;
  cfg.pib_entries = 2;
  paper_frontend front_end(cfg);

  // B02/B03 accounting: the third live instruction cannot enter until a
  // caller reports the first instruction's true completion.
  assert(front_end.try_admit(10));
  assert(front_end.try_admit(11));
  assert(!front_end.try_admit(12));
  assert(front_end.pib_occupancy() == 2);
  assert(front_end.pib_peak() == 2);
  assert(front_end.pib_full_events() == 1);
  assert(front_end.pib_full_stall_cycles() == 1);
  front_end.retire(10);
  assert(front_end.try_admit(12));
  front_end.sample_cycle(99);
  assert(front_end.pib_occupancy_cycle_sum() == 2);
  assert(front_end.pib_occupancy_sample_cycles() == 1);
  assert(front_end.admits() - front_end.retires() == front_end.pib_occupancy());

  // B04: same-bank work serializes.  With 32 logical sets, lines 0 and 4
  // map to logical sets 0 and 4, hence Tag bank 0 in both cases.
  assert(front_end.try_serve_tag(100, 0, 32));
  assert(!front_end.try_serve_tag(100, 4 * 128, 32));
  assert(front_end.try_serve_tag(101, 4 * 128, 32));

  // B05: four different Tag banks can each serve one reference in one cycle.
  for (unsigned bank = 0; bank < 4; ++bank)
    assert(front_end.try_serve_tag(200, bank * 128, 32));
  assert(!front_end.try_serve_tag(200, 4 * 128, 32));
  assert(front_end.tag_conflict_stall_cycles() == 2);
  assert(front_end.frontend_stall_cycles() ==
         front_end.pib_full_stall_cycles() +
             front_end.tag_conflict_stall_cycles());
  assert(front_end.requests_per_bank().size() == 4);
  for (const uint64_t requests : front_end.requests_per_bank())
    assert(requests >= 1);

  // Kernel summaries aggregate value snapshots across SM-local front-ends.
  const dtc_l1::paper_frontend_stats one_sm = front_end.stats();
  dtc_l1::paper_frontend_stats total;
  total.add(one_sm);
  total.add(one_sm);
  assert(total.admits == 2 * one_sm.admits);
  assert(total.retires == 2 * one_sm.retires);
  assert(total.tag_requests == 2 * one_sm.tag_requests);
  assert(total.requests_per_bank.size() == 4);
  for (unsigned bank = 0; bank < 4; ++bank)
    assert(total.requests_per_bank[bank] == 2 * one_sm.requests_per_bank[bank]);
  front_end.assert_accounting();

  // R07.3: a tracked instruction that reaches true L1-hit completion must
  // release its PIB state exactly once; the completed kernel then drains.
  config hit_cfg;
  hit_cfg.selected_mode = mode::PAPER_BASE;
  paper_frontend hit_completion(hit_cfg);
  assert(hit_completion.try_admit(200));
  hit_completion.retire(200);
  hit_completion.assert_drained();

  // M2 unit foundation: a pending hit merges onto one physical allocation;
  // FIFO retirement waits for its fill and releases a replaced line only when
  // the replacing entry reaches the head.
  config io_cfg;
  io_cfg.selected_mode = mode::PAPER_IO;
  io_cfg.logical_sets = 1;
  io_cfg.logical_ways = 1;
  io_cfg.physical_lines = 2;
  io_cfg.allocation_width = 1;
  io_cfg.io_pib_entries = 4;
  dtc_l1::io_frontend io(io_cfg);
  // M2 inherits the frozen logical Tag service limits without touching the
  // conventional L1D Tag/MSHR path: same-bank references serialize while four
  // different banks can be served in one cycle.
  config io_tag_cfg;
  io_tag_cfg.selected_mode = mode::PAPER_IO;
  io_tag_cfg.logical_sets = 32;
  io_tag_cfg.tag_banks = 4;
  io_tag_cfg.tag_requests_per_bank_per_cycle = 1;
  io_tag_cfg.tag_requests_per_cycle = 4;
  dtc_l1::io_frontend io_tag(io_tag_cfg);
  assert(io_tag.try_serve_tag(1, 0));
  assert(!io_tag.try_serve_tag(1, 4 * 128));
  assert(io_tag.try_serve_tag(1, 128));
  assert(io_tag.try_serve_tag(1, 2 * 128));
  assert(io_tag.try_serve_tag(1, 3 * 128));
  assert(!io_tag.try_serve_tag(1, 5 * 128));
  assert(io_tag.try_serve_tag(2, 4 * 128));
  assert(io_tag.tag_requests() == 5);
  assert(io_tag.tag_conflicts() == 2);
  assert(io.admit(1));
  const auto first = io.access(1, 1, 0);
  assert(first.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(io.admit(2));
  const auto merge = io.access(2, 2, 0);
  assert(merge.kind == dtc_l1::io_access_kind::PENDING_HIT);
  assert(merge.physical.id == first.physical.id);
  // Existing 32B-sector accesses of one 128B logical line must not turn into
  // additional DTC line references or lower reads.
  const auto same_line_sector = io.access(2, 2, 96);
  assert(same_line_sector.kind == dtc_l1::io_access_kind::PENDING_HIT);
  assert(same_line_sector.physical.id == first.physical.id);
  assert(io.new_misses() == 1);
  assert(!io.retire_head());
  io.complete(first.physical);
  assert(io.retire_head());
  assert(io.retire_head());
  assert(io.admit(3));
  const auto replacement = io.access(3, 3, 128);
  assert(replacement.kind == dtc_l1::io_access_kind::NEW_MISS);
  io.complete(replacement.physical);
  assert(io.retire_head());
  assert(io.free_lines() == 1);

  // M2 allocator semantics: allocation width is finite, partial allocations
  // remain held, and an undersized pool naturally stops making progress.
  config partial_cfg;
  partial_cfg.selected_mode = mode::PAPER_IO;
  partial_cfg.logical_sets = 2;
  partial_cfg.logical_ways = 1;
  partial_cfg.physical_lines = 1;
  partial_cfg.allocation_width = 1;
  dtc_l1::io_frontend partial(partial_cfg);
  assert(partial.admit(10));
  const auto held = partial.access(10, 10, 0);
  assert(held.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(partial.access(10, 10, 128).kind ==
         dtc_l1::io_access_kind::NO_FREE_LINE);
  assert(partial.free_lines() == 0);
  partial.complete(held.physical);
  assert(!partial.retire_head());

  // R2.4: an allocation block records only the unresolved line.  Once a
  // later retry finds that line Pending, the historical block cannot keep the
  // FIFO head permanently non-retirable.
  config transient_cfg;
  transient_cfg.selected_mode = mode::PAPER_IO;
  transient_cfg.logical_sets = 2;
  transient_cfg.logical_ways = 1;
  transient_cfg.physical_lines = 2;
  transient_cfg.allocation_width = 1;
  dtc_l1::io_frontend transient(transient_cfg);
  assert(transient.admit(40));
  const auto transient_first = transient.access(1, 40, 0);
  assert(transient_first.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(transient.access(1, 40, 128).kind ==
         dtc_l1::io_access_kind::NO_FREE_LINE);
  assert(transient.admit(41));
  const auto transient_second = transient.access(2, 41, 128);
  assert(transient_second.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(transient.access(3, 40, 128).kind ==
         dtc_l1::io_access_kind::PENDING_HIT);
  transient.complete(transient_first.physical);
  transient.complete(transient_second.physical);
  assert(transient.retire_head());
  assert(transient.retire_head());

  config width_cfg;
  width_cfg.selected_mode = mode::PAPER_IO;
  width_cfg.logical_sets = 2;
  width_cfg.logical_ways = 1;
  width_cfg.physical_lines = 2;
  width_cfg.allocation_width = 1;
  dtc_l1::io_frontend width(width_cfg);
  assert(width.admit(20));
  assert(width.access(20, 20, 0).kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(width.access(20, 20, 128).kind ==
         dtc_l1::io_access_kind::NO_FREE_LINE);
  assert(width.access(21, 20, 128).kind == dtc_l1::io_access_kind::NEW_MISS);

  // I02/I04/I05: ready lines never generate a lower miss, width four admits
  // four independent allocations in one cycle, and an eighth request waits
  // for a later allocation cycle.
  config width4_cfg;
  width4_cfg.selected_mode = mode::PAPER_IO;
  width4_cfg.logical_sets = 8;
  width4_cfg.logical_ways = 1;
  width4_cfg.physical_lines = 16;
  width4_cfg.allocation_width = 4;
  dtc_l1::io_frontend width4(width4_cfg);
  for (unsigned uid = 0; uid < 4; ++uid) {
    assert(width4.admit(100 + uid));
    const auto miss = width4.access(50, 100 + uid, uid * 128);
    assert(miss.kind == dtc_l1::io_access_kind::NEW_MISS);
    width4.complete(miss.physical);
  }
  assert(width4.admit(104));
  assert(width4.access(51, 104, 0).kind ==
         dtc_l1::io_access_kind::VALID_HIT);
  assert(width4.new_misses() == 4);

  config width8_cfg = width4_cfg;
  dtc_l1::io_frontend width8(width8_cfg);
  for (unsigned uid = 0; uid < 4; ++uid) {
    assert(width8.admit(200 + uid));
    assert(width8.access(60, 200 + uid, uid * 128).kind ==
           dtc_l1::io_access_kind::NEW_MISS);
  }
  assert(width8.admit(204));
  assert(width8.access(60, 204, 4 * 128).kind ==
         dtc_l1::io_access_kind::NO_FREE_LINE);
  assert(width8.access(61, 204, 4 * 128).kind ==
         dtc_l1::io_access_kind::NEW_MISS);

  // M2 identity/LRU basis: replacing a ready logical Tag holds its old
  // physical allocation through FIFO retirement, then a later allocation may
  // reuse that slot only with a fresh generation.
  config eviction_cfg;
  eviction_cfg.selected_mode = mode::PAPER_IO;
  eviction_cfg.logical_sets = 1;
  eviction_cfg.logical_ways = 1;
  eviction_cfg.physical_lines = 2;
  dtc_l1::io_frontend eviction(eviction_cfg);
  assert(eviction.admit(30));
  const auto old = eviction.access(30, 30, 0);
  eviction.complete(old.physical);
  assert(eviction.retire_head());
  assert(eviction.admit(31));
  const auto newer = eviction.access(31, 31, 128);
  assert(newer.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(newer.physical.id != old.physical.id);
  eviction.complete(newer.physical);
  assert(eviction.retire_head());
  assert(eviction.admit(32));
  const auto reused = eviction.access(32, 32, 256);
  assert(reused.physical.id == old.physical.id);
  assert(reused.physical.generation > old.physical.generation);

  // I07: a hit refreshes exact 4-way LRU, so the untouched oldest Tag is the
  // eviction victim when a fifth line arrives.
  config lru_cfg;
  lru_cfg.selected_mode = mode::PAPER_IO;
  lru_cfg.logical_sets = 1;
  lru_cfg.logical_ways = 4;
  lru_cfg.physical_lines = 6;
  dtc_l1::io_frontend lru(lru_cfg);
  for (unsigned line = 0; line < 4; ++line) {
    assert(lru.admit(300 + line));
    const auto miss = lru.access(70 + line, 300 + line, line * 128);
    lru.complete(miss.physical);
    assert(lru.retire_head());
  }
  assert(lru.admit(304));
  assert(lru.access(80, 304, 0).kind == dtc_l1::io_access_kind::VALID_HIT);
  assert(lru.retire_head());
  assert(lru.admit(305));
  assert(lru.access(81, 305, 4 * 128).kind ==
         dtc_l1::io_access_kind::NEW_MISS);
  assert(lru.access(82, 305, 128).kind ==
         dtc_l1::io_access_kind::NEW_MISS);
  assert(lru.tag_evictions() == 2);

  // I08/I09: evicting a Pending logical Tag does not redirect its fill; a
  // later access to that evicted line allocates a distinct request/identity.
  config pending_evict_cfg;
  pending_evict_cfg.selected_mode = mode::PAPER_IO;
  pending_evict_cfg.logical_sets = 1;
  pending_evict_cfg.logical_ways = 1;
  pending_evict_cfg.physical_lines = 3;
  dtc_l1::io_frontend pending_evict(pending_evict_cfg);
  assert(pending_evict.admit(400));
  const auto pending_old = pending_evict.access(90, 400, 0);
  assert(pending_evict.admit(401));
  const auto replacement_pending = pending_evict.access(91, 401, 128);
  assert(pending_evict.admit(402));
  const auto duplicate = pending_evict.access(92, 402, 0);
  assert(duplicate.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(duplicate.physical.id != pending_old.physical.id);
  assert(pending_evict.duplicate_after_eviction() == 1);
  pending_evict.complete(pending_old.physical);
  pending_evict.complete(replacement_pending.physical);
  pending_evict.complete(duplicate.physical);
  assert(pending_evict.retire_head());
  assert(pending_evict.retire_head());
  assert(pending_evict.retire_head());

  // A completed evicted allocation is no longer duplicate in-flight traffic.
  // Evict the ready duplicate and re-access the original address: the fresh
  // cold miss must not inflate I09 after its original fill already completed.
  assert(pending_evict.admit(403));
  const auto post_fill_eviction = pending_evict.access(93, 403, 256);
  assert(post_fill_eviction.kind == dtc_l1::io_access_kind::NEW_MISS);
  pending_evict.complete(post_fill_eviction.physical);
  assert(pending_evict.retire_head());
  assert(pending_evict.admit(404));
  const auto post_fill_reaccess = pending_evict.access(94, 404, 0);
  assert(post_fill_reaccess.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(pending_evict.duplicate_after_eviction() == 1);

  // I10/I11: a ready younger entry cannot retire before an unready FIFO head;
  // retiring an eviction releases its old physical line for same-cycle reuse.
  config hol_cfg;
  hol_cfg.selected_mode = mode::PAPER_IO;
  hol_cfg.logical_sets = 1;
  hol_cfg.logical_ways = 2;
  hol_cfg.physical_lines = 3;
  dtc_l1::io_frontend hol(hol_cfg);
  assert(hol.admit(500));
  const auto valid_seed = hol.access(100, 500, 0);
  hol.complete(valid_seed.physical);
  assert(hol.retire_head());
  assert(hol.admit(501));
  const auto hol_head = hol.access(101, 501, 128);
  assert(hol.admit(502));
  assert(hol.access(102, 502, 0).kind == dtc_l1::io_access_kind::VALID_HIT);
  assert(hol.younger_ready_exists());
  assert(!hol.retire_head());
  hol.complete(hol_head.physical);
  assert(hol.retire_head());
  assert(hol.retire_head());

  // I11: the logical replacement retains its old physical allocation until
  // the replacing FIFO entry retires. Its release is visible to a new
  // allocator attempt in that same modeled cycle.
  config release_cfg;
  release_cfg.selected_mode = mode::PAPER_IO;
  release_cfg.logical_sets = 1;
  release_cfg.logical_ways = 1;
  release_cfg.physical_lines = 2;
  dtc_l1::io_frontend same_cycle_release(release_cfg);
  assert(same_cycle_release.admit(503));
  const auto release_seed = same_cycle_release.access(110, 503, 0);
  same_cycle_release.complete(release_seed.physical);
  assert(same_cycle_release.retire_head());
  assert(same_cycle_release.admit(504));
  const auto release_replacing = same_cycle_release.access(111, 504, 128);
  same_cycle_release.complete(release_replacing.physical);
  assert(same_cycle_release.retire_head());
  assert(same_cycle_release.admit(505));
  const auto same_cycle_reuse = same_cycle_release.access(111, 505, 256);
  assert(same_cycle_reuse.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(same_cycle_reuse.physical.id == release_seed.physical.id);
  assert(same_cycle_reuse.physical.generation > release_seed.physical.generation);

  // No-MSHR high-MLP basis: the IO model can hold 64 independent pending
  // whole-line dependencies (above Baseline PIB=8 and MSHR=32) while a
  // second reader merges without creating another physical/lower miss.
  config high_mlp_cfg;
  high_mlp_cfg.selected_mode = mode::PAPER_IO;
  high_mlp_cfg.logical_sets = 64;
  high_mlp_cfg.logical_ways = 2;
  high_mlp_cfg.physical_lines = 96;
  high_mlp_cfg.allocation_width = 4;
  high_mlp_cfg.io_pib_entries = 64;
  dtc_l1::io_frontend high_mlp(high_mlp_cfg);
  std::vector<dtc_l1::physical_identity> outstanding;
  for (unsigned uid = 0; uid < 64; ++uid) {
    assert(high_mlp.admit(600 + uid));
    const auto miss = high_mlp.access(200 + uid / 4, 600 + uid, uid * 128);
    assert(miss.kind == dtc_l1::io_access_kind::NEW_MISS);
    outstanding.push_back(miss.physical);
  }
  assert(high_mlp.occupancy() == 64);
  assert(high_mlp.new_misses() == 64);
  assert(high_mlp.admit(700) == false);
  assert(high_mlp.access(217, 600, 0).kind ==
         dtc_l1::io_access_kind::PENDING_HIT);
  assert(high_mlp.new_misses() == 64);
  assert(high_mlp.pending_hits() == 1);
  for (const auto physical : outstanding) high_mlp.complete(physical);
  for (unsigned uid = 0; uid < 64; ++uid) assert(high_mlp.retire_head());
  assert(high_mlp.retires() == 64);
  assert(high_mlp.allocated_lines_peak() >= 64);
  assert(high_mlp.partial_allocation_events() == 0);

  // M3 whole-line O01-O13 foundation.  The OO PIB chooses the oldest ready
  // entry (not FIFO head), while physical lifetime is guarded by exactly one
  // line Ref per live coalesced 128B reference.
  config oo_cfg;
  oo_cfg.selected_mode = mode::PAPER_OO;
  oo_cfg.logical_sets = 1;
  oo_cfg.logical_ways = 1;
  oo_cfg.physical_lines = 3;
  oo_cfg.allocation_width = 4;
  oo_cfg.oo_pib_entries = 4;
  dtc_l1::oo_frontend oo(oo_cfg);
  assert(oo.admit(800));
  const auto old_pending = oo.access(300, 800, 0);
  assert(old_pending.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(oo.ref_count(old_pending.physical) == 1);
  assert(oo.admit(801));
  const auto younger_ready = oo.access(300, 801, 128);
  assert(younger_ready.kind == dtc_l1::io_access_kind::NEW_MISS);
  // O07: replacement clears only logical visibility while the old live Ref
  // keeps its pending physical allocation valid for its original fill.
  assert(!oo.tag_visible(old_pending.physical));
  assert(oo.is_allocated(old_pending.physical));
  assert(oo.ref_count(old_pending.physical) == 1);
  oo.complete(younger_ready.physical);
  uint64_t retired_uid = 0;
  assert(oo.retire_one_ready(301, &retired_uid));
  assert(retired_uid == 801);  // O01 oldest ready bypasses unready 800.
  assert(oo.out_of_order_retires() == 1);
  assert(!oo.retire_one_ready(301));  // O02 width one/cycle.
  oo.complete(old_pending.physical);
  assert(oo.retire_one_ready(302, &retired_uid));
  assert(retired_uid == 800);
  // O08: final Ref after Tag eviction immediately reclaims the old line.
  assert(!oo.is_allocated(old_pending.physical));
  assert(oo.final_ref_reclaims() == 1);

  // O03/O04/O10/O11: a valid hit and two pending readers each add exactly one
  // Ref; one fill wakes every pending dependency exactly once.
  assert(oo.admit(802));
  const auto valid_ref = oo.access(303, 802, 128);
  assert(valid_ref.kind == dtc_l1::io_access_kind::VALID_HIT);
  assert(oo.ref_count(valid_ref.physical) == 1);
  assert(oo.retire_one_ready(303, &retired_uid));
  assert(retired_uid == 802);
  assert(oo.ref_count(valid_ref.physical) == 0);
  assert(oo.admit(803));
  const auto merged_pending = oo.access(304, 803, 256);
  assert(merged_pending.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(oo.admit(804));
  const auto second_waiter = oo.access(304, 804, 256);
  assert(second_waiter.kind == dtc_l1::io_access_kind::PENDING_HIT);
  assert(oo.new_misses() == 3);
  assert(oo.ref_count(merged_pending.physical) == 2);
  oo.complete(merged_pending.physical);
  assert(oo.wakeups() == 4);  // 800/801 plus both merged waiters.
  assert(oo.entry_ready_for(803) && oo.entry_ready_for(804));
  assert(oo.retire_one_ready(305, &retired_uid));
  assert(retired_uid == 803);
  assert(oo.retire_one_ready(306, &retired_uid));
  assert(retired_uid == 804);

  // O09: a zero-Ref logical victim reclaims immediately and is allocator
  // visible in the same modeled cycle.
  assert(oo.admit(805));
  const auto immediate_reuse = oo.access(307, 805, 384);
  assert(immediate_reuse.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(oo.immediate_reclaims() >= 1);

  // O05/O06: one divergent instruction owns one Ref for each of four unique
  // 128B lines; shadow verification runs at every transition.
  config divergent_cfg;
  divergent_cfg.selected_mode = mode::PAPER_OO;
  divergent_cfg.logical_sets = 4;
  divergent_cfg.logical_ways = 1;
  divergent_cfg.physical_lines = 8;
  divergent_cfg.allocation_width = 4;
  divergent_cfg.oo_pib_entries = 4;
  dtc_l1::oo_frontend divergent(divergent_cfg);
  assert(divergent.admit(900));
  std::vector<dtc_l1::physical_identity> divergent_refs;
  for (unsigned line = 0; line < 4; ++line) {
    const auto miss = divergent.access(400, 900, line * 128);
    assert(miss.kind == dtc_l1::io_access_kind::NEW_MISS);
    assert(divergent.ref_count(miss.physical) == 1);
    divergent_refs.push_back(miss.physical);
  }
  for (const auto identity : divergent_refs) divergent.complete(identity);
  assert(divergent.retire_one_ready(401));
  for (const auto identity : divergent_refs)
    assert(divergent.ref_count(identity) == 0);

  // O12/O13: a reused PIB slot gets a fresh slot generation, while a delayed
  // fill with an old physical generation is rejected by the invariant query.
  config slot_cfg;
  slot_cfg.selected_mode = mode::PAPER_OO;
  slot_cfg.logical_sets = 1;
  slot_cfg.logical_ways = 1;
  slot_cfg.physical_lines = 1;
  slot_cfg.oo_pib_entries = 1;
  dtc_l1::oo_frontend slot_reuse(slot_cfg);
  assert(slot_reuse.admit(910));
  const auto slot_old = slot_reuse.access(500, 910, 0);
  const unsigned old_slot = slot_reuse.entry_slot(910);
  const uint64_t old_slot_generation = slot_reuse.entry_slot_generation(910);
  slot_reuse.complete(slot_old.physical);
  assert(slot_reuse.retire_one_ready(501));
  assert(slot_reuse.admit(911));
  const auto slot_new = slot_reuse.access(502, 911, 128);
  assert(slot_reuse.entry_slot(911) == old_slot);
  assert(slot_reuse.entry_slot_generation(911) != old_slot_generation);
  assert(slot_new.physical.id == slot_old.physical.id);
  assert(!slot_reuse.fill_identity_matches(slot_old.physical));
  assert(!slot_reuse.fill_identity_matches(
      {slot_new.physical.id, slot_new.physical.generation + 1}));

  // M3 sector S01-S09: physical allocation and Ref Count remain 128B-line
  // granularity, but each requested 32B sector independently transitions
  // INVALID -> PENDING -> VALID and wakes only its matching waiters.
  config sector_cfg;
  sector_cfg.selected_mode = mode::MODERN_OO_SECTOR;
  sector_cfg.logical_sets = 1;
  sector_cfg.logical_ways = 1;
  sector_cfg.physical_lines = 2;
  sector_cfg.allocation_width = 4;
  sector_cfg.oo_pib_entries = 8;
  dtc_l1::sector_oo_frontend sector(sector_cfg);
  assert(sector.admit(1000));
  const auto s0_miss = sector.access(600, 1000, 0, 0x1);
  assert(s0_miss.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(s0_miss.new_request_mask == 0x1);
  assert(sector.allocated_lines() == 1);
  assert(sector.ref_count(s0_miss.physical) == 1);
  assert(sector.state(s0_miss.physical, 0) ==
         dtc_l1::sector_oo_frontend::sector_state::PENDING);
  sector.complete_sector(s0_miss.physical, 0);
  assert(sector.entry_ready_for(1000));
  assert(sector.retire_one_ready(601));

  // S01: a valid sector is a hit, creates no lower request, and retains the
  // same 128B physical identity.
  assert(sector.admit(1001));
  const auto s0_hit = sector.access(602, 1001, 0, 0x1);
  assert(s0_hit.kind == dtc_l1::io_access_kind::VALID_HIT);
  assert(s0_hit.valid_mask == 0x1 && !s0_hit.pending_mask &&
         !s0_hit.new_request_mask);
  assert(s0_hit.physical.id == s0_miss.physical.id);
  assert(sector.retire_one_ready(602));

  // S03/S04: two invalid sectors of an existing Tag start two sector reads
  // but retain one physical allocation and one Ref for this instruction.
  assert(sector.admit(1002));
  const auto partial_sector = sector.access(603, 1002, 0, 0x6);
  assert(partial_sector.kind == dtc_l1::io_access_kind::NEW_MISS);
  assert(partial_sector.physical.id == s0_miss.physical.id);
  assert(partial_sector.new_request_mask == 0x6);
  assert(sector.allocated_lines() == 1);
  assert(sector.ref_count(partial_sector.physical) == 1);

  // S02/S05: another instruction merges both pending sectors, adds only one
  // line Ref, and receives one waiter for each unresolved sector.
  assert(sector.admit(1003));
  const auto merged = sector.access(604, 1003, 0, 0x6);
  assert(merged.kind == dtc_l1::io_access_kind::PENDING_HIT);
  assert(merged.pending_mask == 0x6 && !merged.new_request_mask);
  assert(sector.ref_count(partial_sector.physical) == 2);
  sector.complete_sector(partial_sector.physical, 1);
  assert(!sector.entry_ready_for(1002));
  assert(!sector.entry_ready_for(1003));
  sector.complete_sector(partial_sector.physical, 2);
  assert(sector.entry_ready_for(1002));
  assert(sector.entry_ready_for(1003));
  assert(sector.wakeups() == 5);  // one S0 plus two waiters for S1/S2.
  assert(sector.retire_one_ready(605));
  assert(sector.retire_one_ready(606));

  // S07/S08: evicting the logical Tag cannot redirect an old pending-sector
  // response; its live Ref preserves the old physical allocation through the
  // original response, then final retirement makes it reclaimable.
  assert(sector.admit(1004));
  const auto old_sector = sector.access(607, 1004, 0, 0x8);
  assert(old_sector.new_request_mask == 0x8);
  assert(sector.admit(1005));
  const auto replacement_sector = sector.access(608, 1005, 128, 0x1);
  assert(replacement_sector.new_request_mask == 0x1);
  assert(!sector.tag_visible(old_sector.physical));
  assert(sector.is_allocated(old_sector.physical));
  assert(sector.ref_count(old_sector.physical) == 1);
  sector.complete_sector(old_sector.physical, 3);
  assert(sector.entry_ready_for(1004));
  assert(sector.retire_one_ready(609));
  assert(!sector.is_allocated(old_sector.physical));
  sector.complete_sector(replacement_sector.physical, 0);
  assert(sector.retire_one_ready(610));

  // S09: all model masks are precisely four sector bits; every issued lower
  // sector got exactly one response in this directed sequence.
  assert(sector.new_sector_requests() == 5);
  assert(sector.sector_responses() == 5);
  assert(sector.valid_sector_hits() == 1);
  assert(sector.pending_sector_hits() == 2);
  sector.assert_shadow_refs();

  // M3.8 causal HOL proof: the same two dynamic line reads leave PAPER_IO
  // blocked behind an older long-latency head, while PAPER_OO may retire the
  // younger short-latency completion first.  Both paths still retire exactly
  // the same two instruction identities after their original fills arrive.
  config causal_cfg;
  causal_cfg.logical_sets = 1;
  causal_cfg.logical_ways = 2;
  causal_cfg.physical_lines = 3;
  causal_cfg.allocation_width = 4;
  causal_cfg.io_pib_entries = 4;
  causal_cfg.oo_pib_entries = 4;
  causal_cfg.selected_mode = mode::PAPER_IO;
  dtc_l1::io_frontend causal_io(causal_cfg);
  assert(causal_io.admit(1100));
  const auto io_long = causal_io.access(700, 1100, 0);
  assert(causal_io.admit(1101));
  const auto io_short = causal_io.access(701, 1101, 128);
  causal_io.complete(io_short.physical);
  assert(causal_io.younger_ready_exists());
  assert(!causal_io.retire_head());  // FIFO HOL is causal, not incidental.
  causal_io.complete(io_long.physical);
  assert(causal_io.retire_head());
  assert(causal_io.retire_head());
  assert(causal_io.retires() == 2);

  causal_cfg.selected_mode = mode::PAPER_OO;
  dtc_l1::oo_frontend causal_oo(causal_cfg);
  assert(causal_oo.admit(1100));
  const auto oo_long = causal_oo.access(700, 1100, 0);
  assert(causal_oo.admit(1101));
  const auto oo_short = causal_oo.access(701, 1101, 128);
  causal_oo.complete(oo_short.physical);
  uint64_t causal_retired = 0;
  assert(causal_oo.retire_one_ready(702, &causal_retired));
  assert(causal_retired == 1101);
  assert(causal_oo.out_of_order_retires() == 1);
  causal_oo.complete(oo_long.physical);
  assert(causal_oo.retire_one_ready(703, &causal_retired));
  assert(causal_retired == 1100);
  assert(causal_oo.retires() == 2);
  return 0;
}
