// C9: a production ROP-ready request waits only because ICNT->L2 is full.
#include <assert.h>
#include <stdio.h>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_fetch.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

static mem_fetch *make_read(partition_mf_allocator &a, new_addr_type addr,
                            unsigned long long cycle) {
  active_mask_t active; active.set(0);
  mem_access_byte_mask_t bytes;
  for (unsigned i = 0; i < SECTOR_SIZE; ++i) bytes.set(i);
  mem_access_sector_mask_t sectors; sectors.set(0);
  return a.alloc(addr, GLOBAL_ACC_R, active, bytes, sectors, SECTOR_SIZE,
                 false, cycle, 0, 0, 0, NULL, 0);
}

int main(int argc, char **argv) {
  assert(argc == 3);
  gpgpu_context ctx; option_parser_t opp = option_parser_create();
  ctx.ptx_reg_options(opp); ctx.func_sim->ptx_opcocde_latency_options(opp);
  icnt_reg_options(opp); gpgpu_sim_config cfg(&ctx); cfg.reg_options(opp);
  option_parser_cfgfile(opp, argv[1]); option_parser_cfgfile(opp, argv[2]);
  cfg.init(); option_parser_destroy(opp);
  exec_gpgpu_sim gpu(cfg, &ctx); const memory_config *mc = gpu.getMemoryConfig();
  assert(mc->m_n_mem == 1 && mc->m_n_sub_partition_per_memory_channel == 1);
  memory_stats_t stats(cfg.num_shader(), gpu.getShaderCoreConfig(), mc, &gpu);
  memory_partition_unit part(0, mc, &stats, &gpu);
  memory_sub_partition *sp = part.get_sub_partition(0);
  partition_mf_allocator alloc(mc);
  // A/B reserve both ways. C remains the ICNT->L2 head on LINE_ALLOC_FAIL,
  // so D is ROP-ready but cannot enter the one-entry production input FIFO.
  sp->push(make_read(alloc, 0x0000, 0), 0);
  sp->push(make_read(alloc, 0x0080, 0), 0);
  sp->push(make_read(alloc, 0x0100, 0), 0);
  sp->push(make_read(alloc, 0x0180, 0), 0);
  for (unsigned cycle = 0; cycle < 220; ++cycle) {
    part.cache_cycle(cycle); part.simple_dram_model_cycle(); ++gpu.gpu_sim_cycle;
  }
  for (unsigned cycle = 220; cycle < 900; ++cycle) {
    part.cache_cycle(cycle); part.simple_dram_model_cycle();
    while (mem_fetch *reply = sp->pop()) delete reply;
    ++gpu.gpu_sim_cycle;
  }
  while (mem_fetch *reply = sp->pop()) delete reply;
  assert(!part.busy()); assert(sp->l2_char_no_resource_leak());
  sp->print_l2_char_stats(stdout);
  puts("C9 production ROP-to-input fixture completed");
  return 0;
}
