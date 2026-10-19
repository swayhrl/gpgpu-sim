// Real detailed DRAM, including its production per-shader read accounting.
// No application trace/replay, injected return, queue bypass or stats guard.
#include <cassert>
#include <cstdio>
#include <set>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

int main(int argc, char **argv) {
  assert(argc == 7);
  gpgpu_context ctx;
  gpgpu_sim_config cfg(&ctx);
  option_parser_t p = option_parser_create();
  ctx.ptx_reg_options(p);
  ctx.func_sim->ptx_opcocde_latency_options(p);
  icnt_reg_options(p);
  cfg.reg_options(p);
  option_parser_cfgfile(p, argv[1]);
  option_parser_cfgfile(p, argv[2]);
  option_parser_cfgfile(p, argv[3]);
  const char *options[] = {
      "ro-detailed-origin", "-gpgpu_ep_l2_ft_mode", "2",
      "-gpgpu_ep_l2_wbuf_entries", "16",
      "-gpgpu_ep_l2_ro_tag_detach", argv[5],
      "-gpgpu_ep_l2_ro_line_mshr_bypass", argv[6],
      "-gpgpu_dram_partition_queues", "512:16:64:1",
      "-gpgpu_cache:dl2", "S:1:128:1,L:B:m:L:L,A:128:32,256:0,32",
      "-gpgpu_simple_dram_model", "0", "-gpgpu_memlatency_stat", "14",
      "-gpgpu_clock_domains", "1000:1000:1000:1000"};
  option_parser_cmdline(p, sizeof(options) / sizeof(options[0]), options);
  cfg.init();
  option_parser_destroy(p);
  exec_gpgpu_sim gpu(cfg, &ctx);
  const auto mc = gpu.getMemoryConfig();
  assert(mc->gpgpu_memlatency_stat);
  // Distinct nonzero identifiers catch both sentinel and hardcoded-zero fixes.
  const unsigned wid = 7, sid = 3, tpc = 3;
  const unsigned long long stream = 17;
  assert(cfg.num_shader() > sid);
  memory_stats_t stats(cfg.num_shader(), gpu.getShaderCoreConfig(), mc, &gpu);
  memory_partition_unit part(0, mc, &stats, &gpu);
  auto sp = part.get_sub_partition(0);
  partition_mf_allocator alloc(mc);
  sp->ro_bind_commandlist(argv[4]);
  sp->ro_begin_epoch();
  active_mask_t active;
  active.set(0);
  std::set<unsigned> expected, replies, lower_packets;
  for (unsigned i = 0; i < 4; ++i) {
    // Two sectors in each of two lines; one Tag way exercises T detach/reuse.
    const unsigned address = (i % 2) * 128 + (i / 2) * 96;
    const unsigned sector = (address >> 5) & 3;
    mem_access_byte_mask_t bytes;
    for (unsigned b = sector * 32; b < sector * 32 + 32; ++b) bytes.set(b);
    auto request = alloc.alloc(address, GLOBAL_ACC_R, active, bytes,
        mem_access_sector_mask_t(1 << sector), 32, false, 0, wid, sid, tpc,
        nullptr, stream);
    expected.insert(request->get_request_uid());
    sp->push(request, 0);
  }
  unsigned ro_packets = 0, origin_mismatches = 0, max_bank_grants = 0;
  bool backpressure = false;
  for (unsigned cycle = 0; cycle < 200000; ++cycle) {
    unsigned long long before[4];
    for (unsigned b = 0; b < 4; ++b) before[b] = sp->ft_bank_grants_for_test(b);
    part.cache_cycle(gpu.gpu_sim_cycle);
    if (!sp->L2_dram_queue_empty()) {
      auto packet = sp->L2_dram_queue_top();
      if (!packet->get_is_write() && lower_packets.insert(packet->get_request_uid()).second) {
        if (packet->ro_lower_packet()) ++ro_packets;
        const bool matches = packet->get_wid() == wid && packet->get_sid() == sid &&
            packet->get_tpc() == tpc && packet->get_streamID() == stream;
        if (!matches) ++origin_mismatches;
        printf("RO_DETAILED_LOWER|uid=%u|ro=%u|wid=%u|sid=%u|tpc=%u|stream=%llu|origin_matches=%u\n",
            packet->get_request_uid(), unsigned(packet->ro_lower_packet()),
            packet->get_wid(), packet->get_sid(), packet->get_tpc(),
            packet->get_streamID(), unsigned(matches));
        // Preserve this observation even if the OLD detailed stats path crashes.
        fflush(stdout);
      }
    }
    // Actual dram_t::push -> memory_stats_t::memlatstat_dram_access, not simple DRAM.
    part.dram_cycle();
    for (unsigned b = 0; b < 4; ++b) {
      const unsigned grants = sp->ft_bank_grants_for_test(b) - before[b];
      max_bank_grants = std::max(max_bank_grants, grants);
      assert(grants <= 1);
    }
    ++gpu.gpu_sim_cycle;
    if (cycle > 2000) {
      while (auto response = sp->pop()) {
        assert(expected.count(response->get_request_uid()));
        assert(replies.insert(response->get_request_uid()).second);
        assert(response->get_wid() == wid && response->get_sid() == sid &&
            response->get_tpc() == tpc && response->get_streamID() == stream);
        assert(response->has_ft_wb_data());
        delete response;
      }
    } else if (sp->ep_l2_descriptor_count_used() && sp->top()) {
      backpressure = true;
    }
    if (cycle > 2000 && !part.busy()) break;
  }
  assert(origin_mismatches == 0);
  assert(!lower_packets.empty());
  const bool ro_on = std::stoul(argv[5]) || std::stoul(argv[6]);
  assert(ro_on ? ro_packets > 0 : ro_packets == 0);
  assert(replies == expected && backpressure && !part.busy());
  assert(sp->l2_char_no_resource_leak());
  sp->ro_end_epoch();
  printf("RO detailed DRAM origin T%s M%s exact-once=%zu lower=%zu ro=%u origin_mismatches=%u max_bank_grants=%u PASS\n",
      argv[5], argv[6], replies.size(), lower_packets.size(), ro_packets,
      origin_mismatches, max_bank_grants);
}
