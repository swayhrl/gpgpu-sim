// C3b: production-path EP-L2 descriptor and line-MSHR regressions.
#include <assert.h>
#include <stdio.h>

#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_fetch.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

static mem_fetch *make_read(partition_mf_allocator &allocator,
                            new_addr_type address,
                            unsigned long long cycle) {
  active_mask_t active;
  active.set(0);
  mem_access_byte_mask_t bytes;
  for (unsigned i = 0; i < SECTOR_SIZE; ++i) bytes.set(i);
  mem_access_sector_mask_t sectors;
  sectors.set((address >> 5) & 3);
  return allocator.alloc(address, GLOBAL_ACC_R, active, bytes, sectors,
                         SECTOR_SIZE, false, cycle, 0, 0, 0, NULL, 0);
}

static mem_fetch *make_write(partition_mf_allocator &allocator,
                             new_addr_type address,
                             unsigned long long cycle) {
  active_mask_t active;
  active.set(0);
  mem_access_byte_mask_t bytes;
  for (unsigned i = 0; i < SECTOR_SIZE; ++i) bytes.set(i);
  mem_access_sector_mask_t sectors;
  sectors.set((address >> 5) & 3);
  return allocator.alloc(address, GLOBAL_ACC_W, active, bytes, sectors,
                         SECTOR_SIZE, true, cycle, 0, 0, 0, NULL, 0);
}

struct scenario {
  gpgpu_context context;
  option_parser_t options;
  gpgpu_sim_config config;
  exec_gpgpu_sim *gpu;
  const memory_config *memory;
  memory_stats_t *stats;
  memory_partition_unit *partition;
  memory_sub_partition *subpartition;
  partition_mf_allocator *allocator;
  unsigned replies;

  scenario(const char *base_config, const char *fixture_config)
      : options(option_parser_create()),
        config(&context),
        gpu(NULL),
        memory(NULL),
        stats(NULL),
        partition(NULL),
        subpartition(NULL),
        allocator(NULL),
        replies(0) {
    context.ptx_reg_options(options);
    context.func_sim->ptx_opcocde_latency_options(options);
    icnt_reg_options(options);
    config.reg_options(options);
    option_parser_cfgfile(options, base_config);
    option_parser_cfgfile(options, fixture_config);
    config.init();
    option_parser_destroy(options);
    options = NULL;

    gpu = new exec_gpgpu_sim(config, &context);
    memory = gpu->getMemoryConfig();
    stats = new memory_stats_t(config.num_shader(), gpu->getShaderCoreConfig(),
                               memory, gpu);
    partition = new memory_partition_unit(0, memory, stats, gpu);
    subpartition = partition->get_sub_partition(0);
    allocator = new partition_mf_allocator(memory);
  }

  ~scenario() {
    while (mem_fetch *reply = subpartition->pop()) delete reply;
    delete allocator;
    delete partition;
    delete stats;
    delete gpu;
  }

  void push(new_addr_type address) {
    subpartition->push(make_read(*allocator, address, gpu->gpu_sim_cycle),
                       gpu->gpu_sim_cycle);
  }

  void push_write(new_addr_type address) {
    subpartition->push(make_write(*allocator, address, gpu->gpu_sim_cycle),
                       gpu->gpu_sim_cycle);
  }

  void cycle(bool drain_replies) {
    partition->cache_cycle(gpu->gpu_sim_cycle);
    partition->simple_dram_model_cycle();
    if (drain_replies) {
      while (mem_fetch *reply = subpartition->pop()) {
        ++replies;
        delete reply;
      }
    }
    ++gpu->gpu_sim_cycle;
  }

  void drain(unsigned limit) {
    for (unsigned i = 0; i < limit && partition->busy(); ++i) cycle(true);
    while (mem_fetch *reply = subpartition->pop()) {
      ++replies;
      delete reply;
    }
    assert(!partition->busy());
    assert(subpartition->l2_char_no_resource_leak());
  }
};

static void lifecycle(const char *base, const char *fixture) {
  scenario s(base, fixture);

  // First response occupies L2->ICNT. It establishes the deliberate
  // backpressure needed to prove the second descriptor cannot retire early.
  s.push(0x1000);
  for (unsigned i = 0; i < 2000 && s.subpartition->ep_l2_l2icnt_occupancy() == 0;
       ++i)
    s.cycle(false);
  assert(s.subpartition->ep_l2_l2icnt_occupancy() == 1);
  assert(s.subpartition->ep_l2_descriptor_count_used() == 0);

  s.push(0x2000);
  for (unsigned i = 0; i < 200; ++i) {
    s.cycle(false);
    if (s.subpartition->ep_l2_descriptor_count_used() == 1) break;
  }
  assert(s.subpartition->ep_l2_descriptor_count_used() == 1);
  assert(s.subpartition->ep_l2_line_mshr_entries() == 1);
  assert(s.subpartition->ep_l2_missq_occupancy() == 1);
  // A cache cycle drains the short MissQ into the real L2->DRAM FIFO but
  // cannot release the descriptor.
  s.partition->cache_cycle(s.gpu->gpu_sim_cycle);
  assert(s.subpartition->ep_l2_missq_occupancy() == 0);
  assert(s.subpartition->ep_l2_l2dram_occupancy() == 1);
  assert(s.subpartition->ep_l2_descriptor_count_used() == 1);
  s.partition->simple_dram_model_cycle();
  ++s.gpu->gpu_sim_cycle;

  // Preserve a real DRAM return in ReturnQ, then let fill make the descriptor
  // ready while the deliberately full L2->ICNT FIFO still blocks retirement.
  for (unsigned i = 0; i < 4000 && s.subpartition->ep_l2_draml2_occupancy() == 0;
       ++i)
    s.cycle(false);
  assert(s.subpartition->ep_l2_draml2_occupancy() == 1);
  assert(s.subpartition->ep_l2_descriptor_count_used() == 1);
  s.subpartition->l2_char_hold_returnq(1);
  s.cycle(false);
  s.cycle(false);
  assert(s.subpartition->ep_l2_ready_requesters() == 1);
  assert(s.subpartition->ep_l2_descriptor_count_used() == 1);
  assert(s.subpartition->ep_l2_l2icnt_occupancy() == 1);

  // Freeing the real queue slot is the only event that permits descriptor and
  // MSHR retirement. The following cache cycle performs the real enqueue.
  mem_fetch *first = s.subpartition->pop();
  assert(first != NULL);
  ++s.replies;
  delete first;
  s.cycle(false);
  assert(s.subpartition->ep_l2_l2icnt_occupancy() == 1);
  assert(s.subpartition->ep_l2_descriptor_count_used() == 0);
  assert(s.subpartition->ep_l2_line_mshr_entries() == 0);
  s.drain(1000);
}

static void different_sector(const char *base, const char *fixture) {
  scenario s(base, fixture);
  s.push(0x4000);
  s.push(0x4020);
  for (unsigned i = 0; i < 200; ++i) {
    s.cycle(true);
    if (s.subpartition->ep_l2_descriptor_count_used() == 2 &&
        s.subpartition->ep_l2_lower_read_issue_count() == 2)
      break;
  }
  assert(s.subpartition->ep_l2_line_mshr_entries() == 1);
  assert(s.subpartition->ep_l2_descriptor_count_used() == 2);
  assert(s.subpartition->ep_l2_lower_read_issue_count() == 2);
  // T3 production path: both sectors share one static resident payload while
  // their lower responses are independently pending.
  assert(s.subpartition->ep_l2_resident_payload_pending() == 1);
  s.drain(5000);
  assert(s.replies == 2);
}

static void same_sector(const char *base, const char *fixture) {
  scenario s(base, fixture);
  s.push(0x5000);
  s.push(0x5000);
  for (unsigned i = 0; i < 200; ++i) {
    s.cycle(true);
    if (s.subpartition->ep_l2_descriptor_count_used() == 2 &&
        s.subpartition->ep_l2_lower_read_issue_count() == 1)
      break;
  }
  assert(s.subpartition->ep_l2_line_mshr_entries() == 1);
  assert(s.subpartition->ep_l2_descriptor_count_used() == 2);
  assert(s.subpartition->ep_l2_lower_read_issue_count() == 1);
  // T6 production path: two requesters to one sector create exactly one
  // lower fill / pending bit and both complete from that response.
  assert(s.subpartition->ep_l2_resident_payload_pending() == 1);
  s.drain(5000);
  assert(s.replies == 2);
}

// C5 production path: the static resident landing is owned before the lower
// request leaves L2, and the exact identity is carried by that transaction.
static void payload_landing(const char *base, const char *fixture) {
  scenario s(base, fixture);
  s.push(0x6800);
  for (unsigned i = 0; i < 200; ++i) {
    s.cycle(true);
    if (s.subpartition->ep_l2_payload_identity_lower_issue_count() == 1) break;
  }
  fprintf(stderr, "payload lower=%llu pending=%u valid=%u\n",
          s.subpartition->ep_l2_payload_identity_lower_issue_count(),
          s.subpartition->ep_l2_resident_payload_pending(),
          s.subpartition->ep_l2_resident_payload_valid());
  assert(s.subpartition->ep_l2_payload_identity_lower_issue_count() == 1);
  assert(s.subpartition->ep_l2_resident_payload_pending() == 1);
  s.drain(5000);
  assert(s.subpartition->ep_l2_resident_payload_pending() == 0);
  assert(s.subpartition->ep_l2_resident_payload_valid() == 1);
}

static void valid_or_dirty_sector_miss(const char *base, const char *fixture) {
  // T4: once sector 0 has filled, a sector-1 miss must retain the same
  // resident landing as valid while its new lower fill is pending.
  scenario s(base, fixture);
  s.push(0x6900);
  s.drain(5000);
  const unsigned long long reads_after_first =
      s.subpartition->ep_l2_payload_identity_lower_issue_count();
  assert(reads_after_first == 1);
  assert(s.subpartition->ep_l2_resident_payload_valid() == 1);
  s.push(0x6920);
  for (unsigned i = 0; i < 200; ++i) {
    s.cycle(true);
    if (s.subpartition->ep_l2_payload_identity_lower_issue_count() ==
        reads_after_first + 1) break;
  }
  assert(s.subpartition->ep_l2_payload_identity_lower_issue_count() ==
         reads_after_first + 1);
  assert(s.subpartition->ep_l2_resident_payload_valid() == 1);
  assert(s.subpartition->ep_l2_resident_payload_pending() == 1);
  s.drain(5000);
  assert(s.replies == 2);
  assert(s.subpartition->ep_l2_resident_payload_pending() == 0);

  // T2/T5: lazy-fetch write allocation is locally complete with no lower
  // pending sector; a later missing-sector read retains its dirty ownership
  // while that read is in flight.
  scenario dirty(base, fixture);
  dirty.push_write(0x6a00);
  dirty.drain(5000);
  assert(dirty.subpartition->ep_l2_payload_identity_lower_issue_count() == 0);
  assert(dirty.subpartition->ep_l2_resident_payload_pending() == 0);
  assert(dirty.subpartition->ep_l2_resident_payload_valid() == 1);
  dirty.push(0x6a20);
  for (unsigned i = 0; i < 200; ++i) {
    dirty.cycle(true);
    if (dirty.subpartition->ep_l2_payload_identity_lower_issue_count() == 1)
      break;
  }
  assert(dirty.subpartition->ep_l2_payload_identity_lower_issue_count() == 1);
  assert(dirty.subpartition->ep_l2_resident_payload_valid() == 1);
  assert(dirty.subpartition->ep_l2_resident_payload_pending() == 1);
  dirty.drain(5000);
  assert(dirty.replies == 2);
  assert(dirty.subpartition->ep_l2_resident_payload_pending() == 0);
}

static void descriptor_exhaustion(const char *base, const char *fixture) {
  scenario s(base, fixture);
  for (unsigned line = 0; line < 64; ++line)
    for (unsigned request = 0; request < 4; ++request) s.push(0x8000 + line * 128);
  s.push(0xc000);  // request 257, pending behind the 256-descriptor pool.

  for (unsigned i = 0; i < 2000 &&
                       s.subpartition->ep_l2_descriptor_count_used() != 256;
       ++i)
    s.cycle(false);
  assert(s.subpartition->ep_l2_descriptor_count_used() == 256);
  assert(s.subpartition->ep_l2_line_mshr_entries() == 64);
  assert(s.subpartition->ep_l2_lower_read_issue_count() == 64);
  s.cycle(false);
  assert(s.subpartition->ep_l2_last_preview_block_reason() ==
         mshr_table::EP_L2_BLOCK_DESCRIPTOR_POOL_FULL);

  s.partition->l2_char_release_dram_issue_hold();
  s.drain(100000);
  assert(s.replies == 257);
}

// C7 instrumentation must be observational. The exact same production
// request/drain sequence therefore has identical simulated cycles with the
// EPL2B0V1 collector enabled and disabled.
static void timing_neutral(const char *base, const char *on, const char *off) {
  scenario enabled(base, on), disabled(base, off);
  enabled.push(0x7000); disabled.push(0x7000);
  enabled.drain(5000); disabled.drain(5000);
  assert(enabled.replies == disabled.replies);
  assert(enabled.gpu->gpu_sim_cycle == disabled.gpu->gpu_sim_cycle);
}

static void banked_production_no_loss(const char *base, const char *fixture) {
  scenario s(base, fixture);
  // Five ways of one set include payload IDs 0 and 4, which target bank 0.
  // The production retry path must drain all requests without duplication.
  for (unsigned i = 0; i < 5; ++i) s.push(0x10000 + i * 8192);
  s.drain(20000);
  assert(s.replies == 5);
}

int main(int argc, char **argv) {
  assert(argc == 6);
  lifecycle(argv[1], argv[2]);
  different_sector(argv[1], argv[2]);
  same_sector(argv[1], argv[2]);
  payload_landing(argv[1], argv[2]);
  valid_or_dirty_sector_miss(argv[1], argv[2]);
  descriptor_exhaustion(argv[1], argv[3]);
  timing_neutral(argv[1], argv[2], argv[4]);
  banked_production_no_loss(argv[1], argv[5]);
  payload_landing(argv[1], argv[5]);
  valid_or_dirty_sector_miss(argv[1], argv[5]);
  puts("EP-L2 C3b production descriptor/MSHR regressions: PASS");
  return 0;
}
