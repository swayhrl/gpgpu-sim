#ifndef EP_L2_IDEAL_TEST_FIXTURE_H
#define EP_L2_IDEAL_TEST_FIXTURE_H
#include <cassert>
#include <cstdio>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

struct Fixture {
  gpgpu_context ctx;
  gpgpu_sim_config cfg;
  exec_gpgpu_sim *gpu;
  memory_stats_t *stats;
  memory_partition_unit *part;
  memory_sub_partition *sp;
  partition_mf_allocator *alloc;
  Fixture(const char *base, const char *overlay) : cfg(&ctx) {
    auto opt = option_parser_create();
    ctx.ptx_reg_options(opt); ctx.func_sim->ptx_opcocde_latency_options(opt);
    icnt_reg_options(opt); cfg.reg_options(opt);
    option_parser_cfgfile(opt, base); option_parser_cfgfile(opt, overlay);
    cfg.init(); option_parser_destroy(opt);
    gpu = new exec_gpgpu_sim(cfg, &ctx);
    const auto mc = gpu->getMemoryConfig();
    stats = new memory_stats_t(cfg.num_shader(), gpu->getShaderCoreConfig(), mc, gpu);
    part = new memory_partition_unit(0, mc, stats, gpu);
    sp = part->get_sub_partition(0);
    alloc = new partition_mf_allocator(mc);
  }
  ~Fixture() { delete alloc; delete part; delete stats; delete gpu; }
  mem_fetch *make(uint64_t address, bool write = false, unsigned bytes_count = 32) {
    active_mask_t active; active.set(0);
    mem_access_byte_mask_t bytes;
    for (unsigned b = address % 128; b < address % 128 + bytes_count; ++b) bytes.set(b);
    mem_access_sector_mask_t sectors; sectors.set((address / 32) % 4);
    return alloc->alloc(address, write ? GLOBAL_ACC_W : GLOBAL_ACC_R,
        active, bytes, sectors, 32, write, gpu->gpu_sim_cycle, 0, 0, 0, nullptr, 0);
  }
  void push(mem_fetch *mf) { sp->push(mf, gpu->gpu_sim_cycle); }
  void cycle() {
    part->cache_cycle(gpu->gpu_sim_cycle);
    part->simple_dram_model_cycle(); ++gpu->gpu_sim_cycle;
  }
  void until_reply() {
    for (unsigned i = 0; i < 10000 && !sp->ep_l2_l2icnt_occupancy(); ++i) cycle();
    assert(sp->ep_l2_l2icnt_occupancy() == 1);
  }
  std::vector<ep_l2_ideal::Byte> read_response(mem_fetch *expected) {
    until_reply(); assert(sp->top() == expected);
    auto value = sp->ep_l2_ideal_response(expected);
    assert(sp->pop() == expected); delete expected; return value;
  }
  void drain() {
    for (unsigned i = 0; i < 10000 && part->busy(); ++i) {
      cycle(); while (auto mf = sp->pop()) delete mf;
    }
    assert(!part->busy()); assert(sp->l2_char_no_resource_leak());
  }
};


#endif
