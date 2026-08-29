// C4 production-path WAD regression.
#include <assert.h>
#include <stdio.h>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_fetch.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

static mem_fetch *write(partition_mf_allocator &a, new_addr_type addr,
                        unsigned long long cycle) {
  active_mask_t active; active.set(0);
  mem_access_byte_mask_t bytes;
  for (unsigned i = 0; i < SECTOR_SIZE; ++i) bytes.set(i);
  mem_access_sector_mask_t sectors; sectors.set((addr >> 5) & 3);
  return a.alloc(addr, GLOBAL_ACC_W, active, bytes, sectors, SECTOR_SIZE,
                 true, cycle, 0, 0, 0, NULL, 0);
}
static mem_fetch *read(partition_mf_allocator &a, new_addr_type addr,
                       unsigned long long cycle) {
  active_mask_t active; active.set(0);
  mem_access_byte_mask_t bytes;
  for (unsigned i = 0; i < SECTOR_SIZE; ++i) bytes.set(i);
  mem_access_sector_mask_t sectors; sectors.set((addr >> 5) & 3);
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
  memory_stats_t stats(cfg.num_shader(), gpu.getShaderCoreConfig(), mc, &gpu);
  memory_partition_unit part(0, mc, &stats, &gpu);
  memory_sub_partition *sp = part.get_sub_partition(0);
  partition_mf_allocator alloc(mc);

  // One dirty resident plus 128 destructive dirty evictions. DRAM issue is
  // held, so no writeback can reach real set_done() during this phase.
  for (unsigned n = 0; n < 130; ++n)
    sp->push(write(alloc, n * 128, 0), 0);
  for (unsigned cycle = 0; cycle < 1000 && sp->ep_l2_wad_occupancy() < 128;
       ++cycle) {
    part.cache_cycle(cycle); part.simple_dram_model_cycle(); ++gpu.gpu_sim_cycle;
    while (mem_fetch *reply = sp->pop()) delete reply;
  }
  assert(sp->ep_l2_wad_occupancy() == 128);
  const unsigned mshr_before = sp->ep_l2_line_mshr_entries();
  // Request 130's mandatory dirty eviction is refused before tag allocation.
  part.cache_cycle(gpu.gpu_sim_cycle); part.simple_dram_model_cycle();
  ++gpu.gpu_sim_cycle;
  assert(sp->ep_l2_wad_occupancy() == 128);
  assert(sp->ep_l2_line_mshr_entries() == mshr_before);
  assert(sp->ep_l2_icntl2_occupancy() > 0);

  // A lower read of an address with a pending/inflight writeback must wait.
  sp->push(read(alloc, 0, gpu.gpu_sim_cycle), gpu.gpu_sim_cycle);
  part.cache_cycle(gpu.gpu_sim_cycle); part.simple_dram_model_cycle();
  ++gpu.gpu_sim_cycle;
  assert(sp->ep_l2_wad_occupancy() == 128);

  part.l2_char_release_dram_issue_hold();
  for (unsigned cycle = 0; cycle < 100000 && part.busy(); ++cycle) {
    part.cache_cycle(gpu.gpu_sim_cycle); part.simple_dram_model_cycle();
    while (mem_fetch *reply = sp->pop()) delete reply;
    ++gpu.gpu_sim_cycle;
  }
  while (mem_fetch *reply = sp->pop()) delete reply;
  assert(!part.busy());
  assert(sp->ep_l2_wad_occupancy() == 0);
  assert(sp->l2_char_no_resource_leak());
  puts("EP-L2 C4 WAD production regression: PASS");
  return 0;
}
