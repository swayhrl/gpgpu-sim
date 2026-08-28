// Deterministic P5A fixture for the production memory_partition path.
//
// It injects R0 -> W0 -> R1 through L2interface, never accesses a private
// FIFO, and lets memory_partition_unit::simple_dram_model_cycle() perform the
// real FIFO-head arbitration.  ReturnQ consumption is held only after R0 has
// actually returned from the production DRAM-latency path.

#include <assert.h>
#include <stdio.h>

#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_fetch.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

namespace {

mem_fetch *make_request(partition_mf_allocator &allocator,
                        const memory_config *config, new_addr_type addr,
                        mem_access_type type, bool write,
                        unsigned long long cycle) {
  active_mask_t active_mask;
  active_mask.set(0);
  mem_access_byte_mask_t byte_mask;
  for (unsigned i = 0; i < SECTOR_SIZE; ++i) byte_mask.set(i);
  mem_access_sector_mask_t sector_mask;
  sector_mask.set(0);
  mem_fetch *mf = allocator.alloc(addr, type, active_mask, byte_mask,
                                  sector_mask, SECTOR_SIZE, write, cycle,
                                  0, 0, 0, NULL, 0);
  // The fixture config has exactly one subpartition.  Keep this assertion
  // next to construction so no request can silently target another queue.
  assert(mf->get_sub_partition_id() == 0);
  (void)config;
  return mf;
}

void cycle_partition(memory_partition_unit &partition, gpgpu_sim &gpu,
                     unsigned cycle, bool run_cache) {
  if (run_cache) partition.cache_cycle(cycle);
  partition.simple_dram_model_cycle();
  ++gpu.gpu_sim_cycle;
}

void drain_reply(memory_sub_partition *sp) {
  mem_fetch *reply = sp->pop();
  if (reply) delete reply;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s <base-gpgpusim.config> <p5-overlay.config>\n",
            argv[0]);
    return 2;
  }

  gpgpu_context ctx;
  option_parser_t opp = option_parser_create();
  ctx.ptx_reg_options(opp);
  ctx.func_sim->ptx_opcocde_latency_options(opp);
  icnt_reg_options(opp);
  gpgpu_sim_config config(&ctx);
  config.reg_options(opp);
  option_parser_cfgfile(opp, argv[1]);
  option_parser_cfgfile(opp, argv[2]);
  config.init();
  option_parser_destroy(opp);

  // Do not call gpgpu_sim::init(): the fixture owns one real partition and
  // deliberately exercises it in isolation.
  exec_gpgpu_sim gpu(config, &ctx);
  const memory_config *mem_config = gpu.getMemoryConfig();
  assert(mem_config->m_n_mem == 1);
  assert(mem_config->m_n_sub_partition_per_memory_channel == 1);
  memory_stats_t stats(config.num_shader(), gpu.getShaderCoreConfig(),
                       mem_config, &gpu);
  memory_partition_unit partition(0, mem_config, &stats, &gpu);
  memory_sub_partition *sp = partition.get_sub_partition(0);
  L2interface lower_if(sp);
  partition_mf_allocator allocator(mem_config);

  mem_fetch *r0 = make_request(allocator, mem_config, 0x0000, GLOBAL_ACC_R,
                               false, 0);
  lower_if.push(r0);

  // A. R0 must leave L2->DRAM and then really fill the one-entry ReturnQ.
  bool returnq_full = false;
  for (unsigned cycle = 0; cycle < 10000; ++cycle) {
    cycle_partition(partition, gpu, cycle, false);
    if (sp->dram_L2_queue_full()) {
      returnq_full = true;
      break;
    }
  }
  assert(returnq_full);
  assert(sp->L2_dram_queue_empty());
  sp->l2_char_hold_returnq(1000);

  // B. Only after the real response has filled ReturnQ, append W0 then R1.
  mem_fetch *w0 = make_request(allocator, mem_config, 0x1000, L2_WRBK_ACC,
                               true, gpu.gpu_sim_cycle);
  mem_fetch *r1 = make_request(allocator, mem_config, 0x2000, GLOBAL_ACC_R,
                               false, gpu.gpu_sim_cycle);
  lower_if.push(w0);
  lower_if.push(r1);
  assert(sp->L2_dram_queue_top() == w0);

  // C. With ReturnQ full, production arbitration must issue the FIFO-head WB
  // (no return destination required) and expose R1 as the next head.
  cycle_partition(partition, gpu, 10001, false);
  assert(sp->dram_L2_queue_full());
  assert(sp->L2_dram_queue_top() == r1);
  assert(partition.l2_char_wb_head_while_returnq_full() > 0);
  assert(partition.l2_char_wb_issued_while_returnq_full() > 0);

  // D. The same full ReturnQ blocks the now FIFO-head return-bearing read.
  cycle_partition(partition, gpu, 10002, false);
  assert(sp->dram_L2_queue_full());
  assert(sp->L2_dram_queue_top() == r1);
  assert(partition.l2_char_read_head_while_returnq_full() > 0);
  assert(partition.l2_char_read_issue_blocked_while_returnq_full() > 0);

  // Release only the consumption hold, then drain all real responses through
  // the normal cache/ICNT side.  No FIFO reordering or synthetic completion.
  sp->l2_char_hold_returnq(0);
  for (unsigned cycle = 10003;
       cycle < 30000 &&
       (partition.busy() || !sp->L2_dram_queue_empty() ||
        !sp->dram_L2_queue_empty());
       ++cycle) {
    cycle_partition(partition, gpu, cycle, true);
    drain_reply(sp);
  }
  drain_reply(sp);

  assert(!partition.busy());
  assert(partition.l2_char_no_credit_leak());
  if (!sp->l2_char_no_resource_leak()) {
    fprintf(stderr, "P5A residual resource state:\n");
    sp->print(stderr);
    return 1;
  }
  assert(sp->l2_char_no_resource_leak());

  // Print the actual collector records so this same production-path fixture
  // proves DRAM ReturnQ causality for L2CHARV1 as well as the legacy P5A
  // invariants above.
  sp->print_l2_char_stats(stdout);

  printf("P5A PASS returnq_full=1 wb_head=%llu wb_issued=%llu "
         "read_head=%llu read_blocked=%llu credit_leak_free=1 "
         "resource_leak_free=1 fifo_reorder=0\n",
         partition.l2_char_wb_head_while_returnq_full(),
         partition.l2_char_wb_issued_while_returnq_full(),
         partition.l2_char_read_head_while_returnq_full(),
         partition.l2_char_read_issue_blocked_while_returnq_full());
  return 0;
}
