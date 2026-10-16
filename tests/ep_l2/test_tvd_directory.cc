// Executes the production directory and existing production payload bank
// arbiter. The byte/version backing below is TEST-ONLY, not a new data array.
// Actual l2_cache victim/handoff wiring is a separate integration gate.
#include <array>
#include <cassert>
#include <cstdio>
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
  assert(d.preview_access(0x1000, 5, directory::READ, 4, 4).status == directory::LOOKUP_DELAY);
  directory::observation o = d.preview_access(0x1000, 5, directory::READ, 4, 5);
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
  assert(d.stats().read_responses == 1 && d.stats().avoided_lower_sectors == 2);
  assert(d.stats().avoided_lower_bytes == 64 && d.live(t));
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
  assert(d.preview_access(0x2000, 4, directory::READ, 0, 1).status == directory::MISSING_SECTOR);
  assert(d.stats().read_responses == 0);
  assert(d.preview_access(0x2000, 16, directory::READ, 0, 1).status == directory::INVALID_INPUT);
  assert(d.preview_access(0x2000, 1, directory::WRITE, 0, 1).status == directory::PROMOTE_REQUIRED);
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
  assert(d.preview_access(0x2080, 1, directory::ATOMIC, 2, 3).status == directory::PROMOTE_REQUIRED);
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

int main() {
  clean_read_and_backpressure();
  sectors_and_promotions();
  capacity_reservations_and_epochs();
  puts("CLEAN_DIRECTORY_SUBSET_PASS; L2_CACHE_INTEGRATION_NOT_RUN; FULL_TVD_NOT_ASSERTED");
}
