// Real 1024-Tag/1168-payload production partition, actual FIFO backpressure.
#include "micro_partition_fixture.h"

static void capacity(fixture &f, bool observer) {
  namespace M = ep_l2::mechanism_v1;
  const new_addr_type stride = 64 * 128;
  f.block_lower = true;
  for (unsigned i = 0; i < 33; ++i) {
    auto w = f.make(i * stride, true);
    f.memory[w->get_addr()] = {{(unsigned long long)w->get_request_uid() + 1, 0, 0, 0}};
    f.send(w); f.until_frontend_empty();
  }
  assert(f.cache->ft_wb_staged() == 16 && !f.sp->L2_dram_queue_empty());
  auto w = f.make(33 * stride, true);
  f.memory[w->get_addr()] = {{(unsigned long long)w->get_request_uid() + 1, 0, 0, 0}};
  f.send(w);
  for (unsigned i = 0; i < 66; ++i) f.step();
  assert(f.sp->ep_l2_icntl2_occupancy() == 1 && f.cache->ft_wb_staged() == 16);
  if (observer) {
    const auto &wait = f.cache->ms_observer_for_test().waits[M::WB_DATA_SPACE];
    assert(wait.requests == 1 && wait.observations >= 64 && wait.resolved == 0);
  }
  f.block_lower = false; f.until_frontend_empty();
  for (unsigned n = 0; f.cache->ft_wb_staged() || !f.cache->miss_queue_empty() ||
       !f.sp->L2_dram_queue_empty(); ++n) { assert(n < 2048); f.step(); }
  assert(f.wb.size() == 18 && f.cache->ep_l2_wad_occupancy() == 18);
  // Native set_done, rather than manual directory erasure, closes these WBs.
  for (auto packet : f.wb) { f.sp->set_done(packet); delete packet; }
  f.wb.clear();
  if (observer) {
    const auto &o = f.cache->ms_observer_for_test();
    const auto &wait = o.waits[M::WB_DATA_SPACE];
    assert(wait.requests == 1 && wait.resolved == 1 && wait.sum >= 64);
    assert(o.head_changes_without_admission == 0);
  }
  assert(f.cache->ft_drained() && !f.cache->ep_l2_wad_occupancy());
  assert(!f.cache->ep_l2_payload_pending_operations() && f.replies.size() == 34);
  printf("EPL2MS_TEST|SEMANTIC|case=capacity|cycles=%llu|replies=34|wb=18|drained=1\n", f.gpu->gpu_sim_cycle);
}

int main(int argc, char **argv) {
  assert(argc == 11); setvbuf(stdout, NULL, _IOLBF, 0);
  const unsigned diagnostic = std::stoul(argv[8]);
  const bool observer = std::stoul(argv[9]);
  // This finite pressure fixture deliberately holds ONE lower FIFO packet.
  // Existing micro tests retain their original32-entry lower FIFO default.
  fixture f(argv,"32:1:32:1");
  auto &pool = f.cache->ep_l2_payload_for_test();
  assert(pool.physical_slots() == 1168 && f.cache->ms_observer_for_test().enabled == observer);
  if (std::string(argv[10]) == "capacity") {
    assert(std::stoul(argv[3]) == 1); capacity(f, observer);
    f.cache->ft_print(stdout, 0); puts("MS_PARTITION_CAPACITY_NATIVE_PASS"); return 0;
  }
  if (std::string(argv[10]) == "latency7") {
    auto r = f.make(0x4000, false);
    const auto uid = r->get_request_uid();
    const auto address = r->get_addr();
    const auto pushed = f.gpu->gpu_sim_cycle;
    f.send(r);
    for (unsigned n = 0; n < 7; ++n) { f.step(); assert(f.reads.empty()); }
    for (unsigned n = 0; !f.reads.count(address); ++n) { assert(n < 64); f.step(); }
    assert(f.gpu->gpu_sim_cycle - pushed >= 8 && !f.replies.count(uid));
    f.return_read(address); f.step();
    // A real lower return fills at this cycle; native response consumption is
    // in the later cycle, even with DATA_FREE. No direct synthetic reply.
    assert(!f.replies.count(uid));
    for (unsigned n = 0; !f.replies.count(uid); ++n) { assert(n < 64); f.step(); }
    assert(f.reads.empty() && f.cache->ft_drained());
    f.cache->ft_print(stdout, 0);
    printf("EPL2MS_TEST|LATENCY|rop=7|cold_lower_reads=1|later_response_cycle=1|cycles=%llu|PASS\n", f.gpu->gpu_sim_cycle);
    return 0;
  }
  assert(std::string(argv[10]) == "service");
  mem_access_sector_mask_t mask; mask.set(0);
  assert(f.sp->ft_try_force_l2_tag_update(512, f.gpu->gpu_sim_cycle, mask));
  assert(f.sp->ft_memcpy_offset_for_test() == 1 && pool.bank_grants() == 0);
  auto seed = f.make(0, true); f.send(seed); f.until_frontend_empty();
  std::vector<mem_fetch *> pending;
  for (unsigned i = 0; i < 8; ++i) {
    auto r = f.make(128 + i * 64 * 128, false); pending.push_back(r);
    f.send(r); f.until_frontend_empty();
  }
  assert(f.reads.size() == 8 && !f.sp->ep_l2_l2icnt_occupancy());
  const unsigned bank = pending.front()->get_ep_l2_payload_id() % 4;
  mem_fetch *other = nullptr;
  for (auto r : pending)
    if (r != pending.front() && r->get_ep_l2_payload_id() % 4 == bank) other = r;
  assert(other);
  f.consume_responses = false;
  auto hit = f.make(0, true); f.send(hit); f.step();
  assert(f.sp->ep_l2_icntl2_occupancy() == 1);
  f.return_read(pending.front()->get_addr()); f.step();
  const bool data_free = diagnostic == 2;
  assert(f.sp->ep_l2_icntl2_occupancy() == (data_free ? 0u : 1u));
  f.step();
  assert(f.sp->ep_l2_l2icnt_occupancy() == 1 && f.cache->access_ready());
  f.return_read(other->get_addr()); f.step();
  // No free response queue or early unpin is donated by either diagnostic.
  assert(f.sp->ep_l2_l2icnt_occupancy() == 1);
  assert(f.sp->ep_l2_draml2_occupancy() == (data_free ? 0u : 1u));
  auto ack = f.sp->pop(); assert(ack == hit);
  assert(f.replies.insert(ack->get_request_uid()).second); delete ack;
  f.consume_responses = true;
  for (unsigned n = 0; f.sp->ep_l2_draml2_occupancy(); ++n) { assert(n < 128); f.step(); }
  std::vector<new_addr_type> remaining;
  for (auto &r : f.reads) remaining.push_back(r.first);
  for (auto address : remaining) {
    f.return_read(address);
    for (unsigned n = 0; f.sp->ep_l2_draml2_occupancy(); ++n) { assert(n < 128); f.step(); }
  }
  for (unsigned n = 0; !f.cache->ft_drained(); ++n) { assert(n < 256); f.step(); }
  auto hot = f.make(128, false); const auto hot_uid = hot->get_request_uid();
  f.send(hot); f.until_frontend_empty();
  for (unsigned n = 0; !f.replies.count(hot_uid); ++n) { assert(n < 64); f.step(); }
  assert(f.reads.empty() && f.replies.size() == 11 && f.cache->ft_drained());
  assert(!f.cache->ep_l2_payload_pending_operations() && !f.cache->ep_l2_wad_occupancy());
  if (data_free) assert(pool.bank_grants() == 0 && pool.diagnostic_services(ep_l2_payload_store::FILL) == 8);
  if (observer) {
    const auto &o = f.cache->ms_observer_for_test();
    assert(o.lower_reads == 8 && o.lower_read_bytes == 8 * 32);
    assert(o.head_changes_without_admission == 0 && o.sampled_cycles > 0);
    assert(o.bound_tag_peak <= 1024 && o.physical_resident_peak <= 1168);
  }
  f.cache->ft_print(stdout, 0);
  printf("EPL2MS_TEST|SEMANTIC|case=service|cycles=%llu|replies=11|cold_reads=8|hot_reads=1|drained=1\n", f.gpu->gpu_sim_cycle);
  puts("MS_PARTITION_REAL_FIFOS_BANK_INTERVENTION_FIXED_PIPELINE_NATIVE_PASS");
}
