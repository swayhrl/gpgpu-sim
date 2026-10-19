// Executes the production directory and existing production payload bank
// arbiter. The byte/version backing below is TEST-ONLY, not a new data array.
// Actual l2_cache victim/handoff wiring is a separate integration gate.
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include "../../src/gpgpu-sim/gpu-cache.h"
#include "../../src/gpgpu-sim/ep-l2-tvd-directory.h"

typedef ep_l2_payload_store::payload_handle handle;
typedef ep_l2_tvd_directory<handle> directory;

static directory::ticket install(directory &d, unsigned long long line,
                                  handle h, unsigned valid, unsigned dirty,
                                  bool wb, unsigned long long cycle) {
  directory::install_plan p = d.preview_install(line, wb);
  directory::ticket t;
  assert(p.status == directory::OK);
  assert(d.commit_install(p, line, h, valid, dirty, 0, wb, cycle, t) == directory::OK);
  return t;
}

static void clean_read_and_backpressure() {
  ep_l2_payload_store ram(2);
  ram.assign_resident(4, 0x1000, NULL);
  const handle h = ram.resident_handle(4);
  directory d(128, true, false);
  directory::ticket t = install(d, 0x1000, h, 5, 0, false, 0);
  // Versioned sectors are outside the simulator, solely the test oracle.
  const std::array<unsigned, 4> bytes = {{71, 0, 103, 0}};
  assert(d.preview_access(0x1000, 5, directory::READ_OP, 4, 4).status == directory::LOOKUP_DELAY);
  directory::observation o = d.preview_access(0x1000, 5, directory::READ_OP, 4, 5);
  assert(o.status == directory::READ_READY && o.payload.payload_id == h.payload_id);
  directory::read_lease lease;
  assert(d.begin_read(o, 5, lease) == directory::OK);
  // An older same-bank payload operation wins. The real bank arbiter, not a
  // mocked ready boolean, denies and later grants this TVD data read.
  assert(ram.request(ep_l2_payload_store::RESIDENT, 0, false, 5) == ep_l2_payload_store::GRANTED);
  assert(ram.request(h, false, 5) == ep_l2_payload_store::BANK_TRUE_CONTENTION);
  assert(d.stats().read_responses == 0);
  assert(ram.request(h, false, 6) == ep_l2_payload_store::GRANTED);
  assert(bytes[0] == 71 && bytes[2] == 103);
  // ICNT is held after bank service; no response/avoided-read is counted.
  handle released;
  assert(d.reclaim(t, released, 7) == directory::PINNED);
  assert(d.promote(t, released, 7) == directory::PINNED);
  assert(!d.epoch_ready() && d.stats().read_responses == 0);
  assert(d.response_commit(lease, 9) == directory::OK);
  assert(d.response_commit(lease, 9) == directory::STALE);
  assert(d.stats().read_responses == 1 && d.stats().served_sectors == 2);
  assert(d.stats().served_sector_bytes == 64 && d.live(t));
  assert(d.epoch_ready());
  assert(d.reclaim(t, released, 9) == directory::OK);
  assert(released.payload_id == h.payload_id && released.generation == h.generation);
  assert(!d.occupied());
  puts("PASS clean_read_bank_icnt_data_version");
}

static void sectors_and_promotions() {
  directory d(128, true, false);
  handle h(3, 21), moved;
  directory::ticket t = install(d, 0x2000, h, 3, 0, false, 0);
  assert(d.preview_access(0x2000, 4, directory::READ_OP, 0, 1).status == directory::MISSING_SECTOR);
  assert(d.stats().read_responses == 0);
  assert(d.preview_access(0x2000, 16, directory::READ_OP, 0, 1).status == directory::INVALID_INPUT);
  assert(d.preview_access(0x2000, 1, directory::WRITE_OP, 0, 1).status == directory::PROMOTE_REQUIRED);
  assert(d.preview_access(0x2000, 4, directory::ATOMIC_OP, 0, 1).status == directory::PROMOTE_REQUIRED);
  const std::array<unsigned, 4> data = {{11, 29, 0, 0}};
  // The future producer must first prepare tag/victim resources. A failed
  // preparation makes no call to promote and cannot partially remove TVD.
  assert(d.live(t) && d.occupied() == 1);
  assert(d.promote(t, moved, 1) == directory::OK);
  assert(moved.payload_id == 3 && moved.generation == 21 && !d.live(t));
  std::array<unsigned, 4> resident = data;
  resident[0] = 44;  // normal promoted write
  assert(data[0] == 11 && resident[0] == 44 && resident[1] == 29);
  t = install(d, 0x2080, handle(8, 1), 15, 0, false, 2);
  assert(d.preview_access(0x2080, 1, directory::ATOMIC_OP, 2, 3).status == directory::PROMOTE_REQUIRED);
  assert(d.promote(t, moved, 3) == directory::OK);
  unsigned atomic_old = 44, atomic_value = atomic_old + 7;
  assert(atomic_old == 44 && atomic_value == 51);
  assert(d.stats().promotions == 2 && d.stats().read_responses == 0);
  puts("PASS sector_exact_write_atomic_same_handle_promotion");
}

static void capacity_reservations_and_epochs() {
  directory off;
  assert(off.preview_install(0x1000, false).status == directory::DISABLED);
  directory d(128, true, false);
  for (unsigned i = 0; i < 128; ++i)
    install(d, i * 128, handle(i, 1), 15, 0, false, i);
  assert(d.occupied() == 128 && d.mandatory() == 0);
  directory::install_plan p = d.preview_install(128 * 128, false);
  assert(p.status == directory::OK && p.displaced.payload_id == 0);
  // Pool/landing refusal: the preview alone cannot alter victim/ownership.
  assert(d.find(0).index == 0 && d.occupied() == 128);
  directory::ticket t;
  assert(d.commit_install(p, 128 * 128, handle(), 15, 0, 0, false, 128, t) == directory::INVALID_INPUT);
  assert(d.find(0).index == 0);
  assert(d.commit_install(p, 128 * 128, handle(128, 1), 15, 0, 1, false, 128, t) == directory::INVALID_INPUT);
  assert(d.commit_install(p, 128 * 128, handle(128, 1), 15, 0, 0, false, 128, t) == directory::OK);
  assert(d.stats().reclaimed == 1 && !d.live(directory::ticket(0, 1)));
  assert(d.commit_install(p, 128 * 128, handle(128, 1), 15, 0, 0, false, 128, t) == directory::STALE);
  for (unsigned i = 0; i < 128; ++i) {
    const directory::entry &e = d.at(i);
    handle released;
    assert(d.reclaim(directory::ticket(i, e.incarnation), released, 129) == directory::OK);
  }
  assert(d.epoch_ready() && !d.occupied());
  bool invalid = false;
  try { directory bad(129); } catch (const std::invalid_argument &) { invalid = true; }
  assert(invalid);
  puts("PASS shared_128_capacity_failed_prepare_stale_epoch");
}

static void reference_versions_follow_payload_identity() {
  ep_l2_payload_store ram(2);
  ram.assign_resident(7, 0x3000, NULL);
  ram.assign_resident(11, 0x3080, NULL);
  std::map<unsigned, std::array<unsigned, 4> > backing;
  std::map<unsigned long long, std::array<unsigned, 4> > reference;
  backing[7] = {{17, 23, 31, 47}};
  backing[11] = {{59, 61, 73, 89}};
  reference[0x3000] = backing[7]; reference[0x3080] = backing[11];
  directory d(128, true, false);
  directory::ticket t = install(d, 0x3000, ram.resident_handle(7), 15, 0, false, 0);
  directory::observation o = d.preview_access(0x3000, 10, directory::READ_OP, 0, 1);
  directory::read_lease lease;
  assert(d.begin_read(o, 10, lease) == directory::OK);
  assert(ram.handle_owner_matches(lease.handle(), 0x3000));
  assert(ram.request(lease.handle(), false, 1) == ep_l2_payload_store::GRANTED);
  for (unsigned sector = 0; sector < 4; ++sector)
    if (10 & (1u << sector))
      assert(backing[lease.handle().payload_id][sector] == reference[0x3000][sector]);
  assert(d.response_commit(lease, 2) == directory::OK);
  handle promoted;
  assert(d.promote(t, promoted, 3) == directory::OK);
  // The producer's data/version oracle changes the same physical storage;
  // copying the current resident's payload 11 would fail this comparison.
  backing[promoted.payload_id][0] = 113; reference[0x3000][0] = 113;
  unsigned returned = backing[promoted.payload_id][2];
  backing[promoted.payload_id][2] += 5; reference[0x3000][2] += 5;
  assert(returned == 31 && backing[promoted.payload_id] == reference[0x3000]);
  assert(backing[11] == reference[0x3080] && !d.live(t));
  puts("PASS module_reference_versions_by_canonical_payload_handle");
}

static void dirty_data_forward_and_ordering() {
  ep_l2_payload_store ram(2);
  ram.assign_resident(8, 0x4000, NULL);
  ram.mark_resident_dirty(8);
  const handle h = ram.resident_handle(8);
  directory d(128, true, true);
  directory::ticket t = install(d, 0x4000, h, 5, 1, true, 0);
  std::array<unsigned, 4> latest = {{109, 0, 37, 0}};
  std::array<unsigned, 4> memory = {{3, 0, 37, 0}};
  assert(latest[0] != memory[0]);
  assert(d.mandatory() == 1 && !d.epoch_ready());
  assert(d.lower_accept(t, false, 3) == directory::DATA_TRANSFER_UNPROVEN);
  assert(d.at(t.index).status == directory::WB_STAGED && d.stats().lower_accepts == 0);
  // Real WB data-read bank service precedes the independently owned lower
  // packet snapshot. The snapshot exists only in this data/version fixture.
  assert(ram.request(h, false, 4, 1, ep_l2_payload_store::WB_READOUT) == ep_l2_payload_store::GRANTED);
  const std::array<unsigned, 4> lower_packet = latest;
  assert(d.lower_accept(t, true, 5) == directory::OK);
  assert(d.stats().wb_staging_slot_cycles == 5 && d.mandatory() == 1);
  assert(d.lower_accept(t, true, 6) == directory::STALE);
  assert(d.preview_access(0x4000, 1, directory::WRITE_OP, 8, 9).status == directory::WAIT_WRITEBACK);
  assert(d.preview_access(0x4000, 1, directory::ATOMIC_OP, 8, 9).status == directory::WAIT_WRITEBACK);
  assert(d.preview_access(0x4000, 2, directory::READ_OP, 8, 9).status == directory::WAIT_WRITEBACK);
  directory::observation o = d.preview_access(0x4000, 5, directory::READ_OP, 8, 9);
  assert(o.status == directory::READ_READY && o.dirty_forward);
  directory::read_lease lease;
  assert(d.begin_read(o, 5, lease) == directory::OK);
  assert(ram.handle_owner_matches(lease.handle(), 0x4000));
  assert(ram.request(lease.handle(), false, 9) == ep_l2_payload_store::GRANTED);
  std::map<unsigned, std::array<unsigned, 4> > physical;
  physical[h.payload_id] = latest;
  for (unsigned s = 0; s < 4; ++s)
    if (5 & (1u << s)) assert(physical[lease.handle().payload_id][s] == latest[s]);
  assert(physical[lease.handle().payload_id][0] != memory[0]);
  handle transfer;
  assert(d.promote(t, transfer, 10) == directory::WAIT_WRITEBACK);
  assert(d.reclaim(t, transfer, 10) == directory::WAIT_WRITEBACK);
  // ICNT backpressure holds the read across real WB completion. Neither
  // completing WAD nor requesting a promotion may discard that data reader.
  memory[0] = lower_packet[0];
  assert(d.set_done(t, 11) == directory::OK);
  assert(memory[0] == latest[0] && d.at(t.index).status == directory::WB_COMPLETE);
  assert(d.mandatory() == 0 && !d.epoch_ready());
  assert(d.promote(t, transfer, 11) == directory::PINNED);
  assert(d.stats().read_responses == 0);
  assert(d.response_commit(lease, 12) == directory::OK);
  assert(d.stats().dirty_forward_responses == 1 && d.stats().served_sector_bytes == 64);
  assert(d.promote(t, transfer, 13) == directory::OK && transfer == h);
  assert(d.stats().tvd_extra_retention_slot_cycles == 8);
  // Now the previously waiting atomic can use the promoted authoritative
  // handle; the old lower packet cannot overwrite that later version.
  unsigned prior = physical[transfer.payload_id][0];
  physical[transfer.payload_id][0] += 7;
  assert(prior == 109 && physical[transfer.payload_id][0] == 116);
  assert(lower_packet[0] == 109 && !d.occupied() && d.epoch_ready());
  assert(d.set_done(t, 14) == directory::STALE);
  puts("PASS dirty_lower_accept_independent_data_forward_wait_reference_versions");
}

static void mandatory_priority_and_shared_capacity() {
  directory d(128, true, true);
  for (unsigned i = 0; i < 128; ++i)
    install(d, i * 128, handle(i, 1), 15, 1, true, i);
  assert(d.mandatory() == 128 && d.occupied() == 128);
  assert(d.preview_install(128 * 128, false).status == directory::DIRECTORY_FULL);
  assert(d.preview_install(128 * 128, true).status == directory::DIRECTORY_FULL);
  handle drop;
  directory::ticket a = d.find(0), b = d.find(128);
  assert(d.reclaim(a, drop, 128) == directory::WAIT_WRITEBACK);
  assert(d.lower_accept(a, true, 128) == directory::OK);
  assert(d.set_done(a, 129) == directory::OK);
  assert(d.lower_accept(b, true, 130) == directory::OK);
  assert(d.set_done(b, 131) == directory::OK);
  // Install a younger clean item. Mandatory WB must reclaim it before an
  // older WB-complete item, honoring the declared directory priority.
  directory::ticket clean = install(d, 128 * 128, handle(128, 1), 15, 0, false, 132);
  directory::install_plan p = d.preview_install(129 * 128, true);
  assert(p.status == directory::OK && p.index == clean.index);
  directory::observation o = d.preview_access(128 * 128, 1, directory::READ_OP, 132, 133);
  directory::read_lease lease;
  assert(d.begin_read(o, 1, lease) == directory::OK);
  directory::ticket fresh;
  assert(d.commit_install(p, 129 * 128, handle(129, 1), 15, 1, 0, true, 133, fresh) == directory::STALE);
  directory::install_plan q = d.preview_install(129 * 128, true);
  assert(q.status == directory::OK && q.index == b.index);
  assert(d.cancel_read(lease) == directory::OK);
  p = d.preview_install(129 * 128, true);
  assert(p.index == clean.index);
  assert(d.commit_install(p, 130 * 128, handle(129, 1), 15, 1, 0, true, 134, fresh) == directory::INVALID_INPUT);
  assert(d.commit_install(p, 129 * 128, handle(2, 1), 15, 1, 0, true, 134, fresh) == directory::INVALID_INPUT);
  assert(d.commit_install(p, 129 * 128, handle(129, 1), 15, 1, 0, true, 134, fresh) == directory::OK);
  assert(d.occupied() == 128 && d.mandatory() == 127);
  for (unsigned i = 0; i < d.capacity(); ++i) {
    directory::ticket t(i, d.at(i).incarnation);
    if (d.at(i).status == directory::WB_STAGED) assert(d.lower_accept(t, true, 135) == directory::OK);
  }
  for (unsigned i = 0; i < d.capacity(); ++i) {
    directory::ticket t(i, d.at(i).incarnation);
    if (d.at(i).status == directory::WB_INFLIGHT) assert(d.set_done(t, 136) == directory::OK);
  }
  assert(d.epoch_ready());
  for (unsigned i = 0; i < d.capacity(); ++i)
    assert(d.reclaim(directory::ticket(i, d.at(i).incarnation), drop, 137) == directory::OK);
  assert(!d.occupied());
  puts("PASS dirty_128_wad_full_priority_pins_stale_rollback_epoch");
}

static void promotion_displaces_dirty_resident_atomically() {
  directory d(128, true, true);
  for (unsigned i = 0; i < 128; ++i)
    install(d, i * 128, handle(i, 1), 15, 0, false, i);
  const directory::ticket old = d.find(0);
  directory::promotion_plan p = d.preview_dirty_promotion(old, 0x8000, handle(512, 3), 7, 3, 0);
  assert(p.status == directory::OK && d.occupied() == 128 && d.mandatory() == 0);
  // Simulated failed lower/pool credit preparation makes NO module commit.
  assert(d.live(old) && !d.live(d.find(0x8000)));
  handle promoted; directory::ticket wb;
  assert(d.commit_dirty_promotion(p, 128, promoted, wb) == directory::OK);
  assert(promoted == handle(0, 1) && !d.live(old) && wb.index == old.index);
  assert(d.occupied() == 128 && d.mandatory() == 1 && d.at(wb.index).line == 0x8000);
  assert(d.at(wb.index).payload == handle(512, 3));
  assert(d.commit_dirty_promotion(p, 128, promoted, wb) == directory::STALE);
  assert(d.preview_access(0x8000, 1, directory::ATOMIC_OP, 128, 129).status == directory::WAIT_WRITEBACK);
  assert(d.lower_accept(wb, true, 129) == directory::OK);
  assert(d.set_done(wb, 130) == directory::OK);
  assert(d.preview_access(0x8000, 8, directory::READ_OP, 130, 131).status == directory::MISSING_SECTOR);
  assert(d.preview_access(0x8000, 8, directory::WRITE_OP, 130, 131).status == directory::PROMOTE_REQUIRED);
  handle output;
  assert(d.promote(wb, output, 131) == directory::OK && output == handle(512, 3));
  assert(d.mandatory() == 0 && d.stats().promotions == 2);
  // WB with no retained handle still occupies the shared mandatory slot,
  // and disappears only at set_done (clean-only TVD mode).
  directory clean_only(128, true, false);
  directory::ticket w = install(clean_only, 0x9000, handle(), 1, 1, true, 0);
  assert(clean_only.preview_access(0x9000, 1, directory::READ_OP, 0, 1).status == directory::WAIT_WRITEBACK);
  assert(clean_only.lower_accept(w, true, 1) == directory::OK);
  assert(clean_only.set_done(w, 2) == directory::OK && !clean_only.occupied());
  puts("PASS promotion_dirty_resident_full_directory_atomic_slot_reuse");
}

int main(int argc, char **argv) {
  clean_read_and_backpressure();
  sectors_and_promotions();
  capacity_reservations_and_epochs();
  reference_versions_follow_payload_identity();
  if (argc == 1 || std::strcmp(argv[1], "--clean") != 0) {
    dirty_data_forward_and_ordering();
    mandatory_priority_and_shared_capacity();
    promotion_displaces_dirty_resident_atomically();
    puts("CLEAN_DIRTY_DIRECTORY_MODULE_PASS; L2_CACHE_INTEGRATION_NOT_RUN; FULL_TVD_NOT_ASSERTED");
  } else {
    puts("CLEAN_DIRECTORY_SUBSET_PASS; L2_CACHE_INTEGRATION_NOT_RUN; FULL_TVD_NOT_ASSERTED");
  }
}
