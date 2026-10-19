// Small 1-way native l2_cache fixture, finite symbolic sector-version scope.
// Full-budget partition/queue/observer replay lives in test_ms_partition.cc.
#include "ft_test_fixture.h"
int main(int argc, char **argv) {
  assert(argc == 5); setvbuf(stdout, NULL, _IOLBF, 0);
  const unsigned mode = std::stoul(argv[3]), diagnostic = std::stoul(argv[4]);
  fixture f(argv[1], argv[2], mode, 16);
  using P = ep_l2_payload_store;
  auto &pool = f.cache->ep_l2_payload_for_test();
  assert(pool.physical_slots() == 1168);
  f.port.blocked = false;
  const new_addr_type a = 0x1000, b = 0x2000, c = 0x3000;
  auto first = f.make(a, false, GLOBAL_ACC_R);
  auto second = f.make(a + 32, false, GLOBAL_ACC_R);
  assert(f.access(first) && f.access(second));
  for (unsigned n = 0; f.port.packets.size() != 2; ++n) { assert(n < 64); f.cycle(); }
  assert(f.port.packets.front() == first && f.port.packets.back() == second);
  f.port.packets.clear();  // External lower fixture now owns these real packets.
  const std::array<unsigned long long, 4> lower = {{101, 202, 0, 0}};
  // Actual sector responses return in reverse order.
  for (auto r : {second, first}) {
    r->set_ft_wb_data(lower);
    for (unsigned n = 0; f.cache->ep_l2_payload_fill_request(r, f.time) != P::GRANTED; ++n) {
      assert(n < 64); f.cycle();
    }
    f.cache->fill(r, f.time); f.cycle();
  }
  const auto held = f.resident();
  assert(pool.inspect(held).pending_sector_mask.none());
  assert(pool.inspect(held).pins == 2 && pool.inspect(held).versions == lower);
  auto forbidden = f.make(a, true);
  assert(!f.access(forbidden, 4)); delete forbidden;
  assert(f.resident() == held && pool.inspect(held).versions == lower);
  unsigned replies = 0;
  for (unsigned n = 0; replies != 2; ++n) {
    assert(n < 128);
    if (f.cache->access_ready() && f.cache->ft_response_ready(f.time)) {
      auto r = f.cache->peek_next_access();
      assert(r == first || r == second);
      assert(r->get_ft_wb_data() == lower);
      f.cache->commit_next_access(); ++replies;
    }
    f.cycle();
  }
  delete first; delete second;
  auto hot = f.make(a, false, GLOBAL_ACC_R); std::list<cache_event> events;
  assert(f.cache->access(a, hot, f.time, events) == HIT);
  assert(!was_read_sent(events) && hot->get_ft_wb_data() == lower);
  delete hot; f.cycle();

  std::map<new_addr_type, std::array<unsigned long long, 4>> reference;
  auto w = f.make(a, true); assert(f.access(w));
  reference[a] = {{(unsigned long long)w->get_request_uid() + 1, 202, 0, 0}}; delete w;
  f.port.blocked = true;
  w = f.make(b, true); const auto start = f.time; assert(f.access(w));
  const auto copy_admission_cycles = f.time - start;
  reference[b] = {{(unsigned long long)w->get_request_uid() + 1, 0, 0, 0}}; delete w;
  assert(f.staged() == 1 && f.wad() == 1 && f.cache->ep_l2_wad_contains(a));
  if (mode == 1) {
    assert(f.cache->ft_copy_lines() == 1);
    assert(pool.logical_copy_reads() == 1 && pool.logical_copy_writes() == 1);
    if (diagnostic == 1) assert(copy_admission_cycles == 1);
    else assert(copy_admission_cycles >= 2);  // Fixed copy latch retained in DATA_FREE.
  } else assert(f.cache->ft_copy_lines() == 0);
  auto ordered = f.make(a, false, GLOBAL_ACC_R);
  assert(!f.access(ordered, 4)); delete ordered;
  assert(f.staged() == 1 && f.cache->ep_l2_wad_contains(a));

  // A WB packet is a value copy, not a pointer to reusable local staging.
  f.port.blocked = false;
  for (unsigned n = 0; f.port.packets.empty(); ++n) { assert(n < 128); f.cycle(); }
  auto packet = f.port.packets.front(); f.port.packets.pop_front();
  assert(packet->get_access_type() == L2_WRBK_ACC);
  assert(packet->get_ft_wb_data() == reference.at(a));
  assert(!f.staged() && f.cache->ep_l2_wad_contains(a));
  assert(!pool.handle_live(ep_l2::PayloadHandle(packet->get_ep_l2_payload_id(),
                                             packet->get_ep_l2_payload_generation())));
  f.port.blocked = true;
  w = f.make(c, true); assert(f.access(w));
  reference[c] = {{(unsigned long long)w->get_request_uid() + 1, 0, 0, 0}}; delete w;
  assert(packet->get_ft_wb_data() == reference.at(a));
  if (mode == 1) {
    const auto &slot = pool.resident(packet->get_ep_l2_payload_id());
    assert(slot.occupied() && slot.owner == b);  // Actual staging slot was reused.
  }
  assert(f.cache->ep_l2_wad_contains(a));
  f.cache->ep_l2_wad_complete(a, f.time); delete packet;
  f.drain_wb(reference);
  if (diagnostic == 1) {
    assert(pool.diagnostic_services(P::COPY_READ) == 2);
    assert(pool.diagnostic_services(P::COPY_WRITE) == 2);
    assert(pool.diagnostic_services(P::LOWER_WB) == 0 && f.cache->ft_staging_reads() == 2);
  }
  if (diagnostic == 2) {
    assert(pool.bank_grants() == 0);
    assert(pool.diagnostic_services(P::FILL) == 2);
    assert(pool.diagnostic_services(P::LOWER_WB) == 2);
  }
  for (unsigned bank = 0; bank < 4; ++bank) assert(pool.bank_peak(bank) <= 1);
  assert(f.cache->ft_drained() && pool.ownership_consistent());
  f.cache->ft_print(stdout, 0);
  printf("EPL2MS_TEST|LOCAL|mode=%u|diagnostic=%u|cold_reads=2|hot_hits=1|reverse_sectors=1|wb=2|held_snapshot=1|copy_admission_cycles=%u|data_version_scope=symbolic|PASS\n",
      mode, diagnostic, copy_admission_cycles);
}
