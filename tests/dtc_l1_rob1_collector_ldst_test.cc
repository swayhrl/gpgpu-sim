// PREPARED_NOT_TESTED. Adapted from accepted dtc_l1_progressive_ldst_test.cc.
// Linked CPU fixture: real parser/LDST ctor/return/collection/RF-WB/retirement.
// -fno-access-control is a fixture setup/readback seam. No GPU cycle(), kernel,
// trace replay, payload-value oracle, CUDA launch, or runtime simulation thread.
// Must link a newly qualified diagnostic library matching this exact Core tree.
#include "gpgpu-sim/gpu-sim.h"
#include "gpgpu-sim/shader.h"
#include "gpgpu-sim/icnt_wrapper.h"
#include "gpgpu-sim/scoreboard.h"
#include "gpgpu_context.h"
#include "gpgpusim_entrypoint.h"
#include "option_parser.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <sys/resource.h>

int main(int argc, const char **argv) {
  // base config; omitted|0|1|invalid; mode; finite. Negative modes must abort
  // in the production ctor before fixture asserts for the supported case.
  assert(argc == 5);
  rlimit limit{0,0}; setrlimit(RLIMIT_CORE, &limit);
  gpgpu_context ctx; gpgpu_sim_config cfg(&ctx);
  auto parser = option_parser_create();
  cfg.reg_options(parser); icnt_reg_options(parser); ctx.ptx_reg_options(parser);
  ctx.func_sim->ptx_opcocde_latency_options(parser);
#ifndef ROB1_ORIGINAL_CORE
  assert(cfg.m_shader_config.dtc_l1_collection_quota == 0);
#endif
  option_parser_cfgfile(parser, argv[1]);
  const char *opts[] = {"ROB1_CPU", "-gpgpu_n_clusters","1",
    "-gpgpu_n_cores_per_cluster","1", "-gpgpu_n_mem","1",
    "-gpgpu_n_sub_partition_per_mchannel","1",
    "-gpgpu_memory_partition_indexing","0",
    "-gpgpu_shader_core_pipeline","64:32", "-gpgpu_shader_cta","2",
    "-gpgpu_smem_latency","2", "-network_mode","2",
    "-gpgpu_dtc_l1_mode",argv[3], "-gpgpu_dtc_l1_finite_service",argv[4],
    "-gpgpu_dtc_l1_progressive_wakeup","0",
    "-gpgpu_dtc_l1_tag_hash_bankrow","0",
    "-gpgpu_dtc_l1_logical_sets","64", "-gpgpu_dtc_l1_logical_ways","8",
    "-gpgpu_dtc_l1_oo_pib_entries","128",
    "-gpgpu_dtc_l1_invalidate_on_source_event","0",
    "-gpgpu_l1_cache_write_ratio","0", "-gpgpu_gmem_skip_L1D","0",
    "-enable_ptx_file_line_stats","0", "-gpgpu_dtc_l1_debug_event_limit","0",
    "-power_simulation_enabled","0", "-visualizer_enabled","0",
    "-gpgpu_clock_domains","1410.0:1410.0:1410.0:850.0"};
  option_parser_cmdline(parser, sizeof(opts)/sizeof(opts[0]), opts);
#ifndef ROB1_ORIGINAL_CORE
  if (std::strcmp(argv[2], "omitted")) {
    const char *quota[] = {"ROB1_CPU", "-gpgpu_dtc_l1_collection_quota", argv[2]};
    option_parser_cmdline(parser, 3, quota);
  }
#else
  assert(!std::strcmp(argv[2], "omitted"));
#endif
  cfg.init(); ctx.the_gpgpusim->g_simulation_starttime = std::time(nullptr);
  auto *gpu = new exec_gpgpu_sim(cfg, &ctx); gpu->init();
  assert(!std::strcmp(argv[3], "3") && !std::strcmp(argv[4], "1"));
#ifndef ROB1_ORIGINAL_CORE
  const unsigned width = cfg.m_shader_config.dtc_l1_collection_quota ? 1 : 4;
  assert(cfg.m_shader_config.dtc_l1_collection_quota == (width == 1 ? 1U : 0U));
#else
  constexpr unsigned width = 4;
#endif
  auto *core = gpu->get_simt_core_clusters()[0]->get_shader_cores()[0];
  auto *ld = core->m_ldst_unit; auto &front = *ld->m_dtc_l1_oo_frontend;
  auto *rf = dynamic_cast<opndcoll_rfu_t *>(ld->m_operand_collector); assert(rf);
  auto inst = [&](unsigned uid) {
    warp_inst_t i(gpu->getShaderCoreConfig());
    std::fill(std::begin(i.out), std::end(i.out), 0U);
    active_mask_t active; active.set(0); i.issue(active,0,1,0,0,0);
    i.m_uid=uid; i.pc=0; i.op=LOAD_OP; i.op_pipe=MEM__OP;
    i.space=memory_space_t(global_space); i.cache_op=CACHE_ALL;
    i.out[0]=1; i.arch_reg.dst[0]=1; return i;
  };
  gpu->gpu_sim_cycle=10; front.advance_service(10);
  auto load=inst(100); assert(front.admit(100));
  ld->m_pending_writes[0][1]=1; ld->m_scoreboard->reserveRegisters(&load);
  const std::vector<dtc_l1::line_reference> refs{{0,7}}; // three requested sectors
  ld->dtc_l1_r4c0_record_issue(load,refs,1);
  ++ld->m_dtc_l1_oo_completion_dependencies;
  ld->m_dtc_l1_oo_pib.push_back({load,refs,1});
  ld->dtc_l1_r4c0_record_pib(load,refs,"OO");
  const auto id=front.access(10,100,0).physical;
  mem_access_t access(GLOBAL_ACC_R,0,128,false,&ctx);
  auto *root=new mem_fetch(access,(const warp_inst_t *)nullptr,0,8,0,0,0,
      &cfg.m_memory_config,gpu->global_cycle());
  assert(gpu->dtc_l1_try_acquire_lower_request());
  ++ld->m_dtc_l1_oo_lower_created; ++ld->m_dtc_l1_oo_lower_issued;
  assert(ld->m_dtc_l1_oo_inflight.emplace(root->get_request_uid(),
      ldst_unit::dtc_l1_oo_inflight{id,0,100,15,0}).second);
  for (unsigned sector : {0U,2U,1U,3U}) {
    mem_access_t part(GLOBAL_ACC_R,sector*32,32,false,&ctx);
    auto *reply=new mem_fetch(part,(const warp_inst_t *)nullptr,0,8,0,0,0,
        &cfg.m_memory_config,gpu->global_cycle(),root);
    reply->set_reply(); assert(ld->dtc_l1_oo_consume_response(reply));
  }
  assert(!front.entry_ready_for(100)); // no same-cycle metadata/notifications
  gpu->gpu_sim_cycle=11; rf->process_banks(); ld->cycle();
  auto *entry=ld->dtc_l1_oo_find_entry(100); assert(entry);
  assert(front.entry_ready_for(100) && entry->service.ready_cycle==11);
  assert(entry->service.sectors_left==3 && front.ref_count(id)==1);
  assert(entry->service.collect_done_cycle==UINT64_MAX);
  const unsigned done=width==1 ? 14 : 12;
  unsigned moved=0;
  for (unsigned cycle=12; cycle<=done; ++cycle) {
    gpu->gpu_sim_cycle=cycle; rf->process_banks();
    const auto before=entry->service.sectors_left;
    ld->cycle(); ld->cycle(); // repeated absolute-cycle calls cannot refill quota
    const auto delta=before-entry->service.sectors_left;
    assert(delta<=width); moved+=delta;
    assert(moved+entry->service.sectors_left==3);
    assert(front.ref_count(id)==1 && ld->m_dtc_service_retired==0);
  }
  assert(entry->service.collect_start_cycle==12 && entry->service.collect_done_cycle==done);
  assert(moved==3 && ld->m_dtc_service_collected_sectors==3);
  gpu->gpu_sim_cycle=done+1; rf->process_banks();
  auto competing=inst(999); assert(rf->writeback(competing)); // real RF reservation
  ld->cycle();
  assert(ld->m_dtc_l1_oo_ready_but_wb_blocked_cycles==1);
  assert(front.ref_count(id)==1 && ld->m_pending_writes[0][1]==1);
  assert(entry->service.collect_done_cycle==done && entry->service.sectors_left==0);
  assert(ld->m_dtc_l1_r4c0_completion_trace.count(100)==1);
  gpu->gpu_sim_cycle=done+2; rf->process_banks(); ld->cycle();
  assert(!ld->dtc_l1_oo_find_entry(100) && front.active_refs()==0);
  assert(ld->m_pending_writes[0].count(1)==0);
  assert(ld->m_dtc_l1_r4c1_retired_history.at(100).closed_dependencies==1);
  assert(ld->m_dtc_service_retired==1 && ld->m_dtc_service_collected_sectors==3);
  assert(gpu->dtc_l1_lower_outstanding()==0 && ld->m_dtc_l1_oo_inflight.empty());
  front.assert_shadow_refs();
  std::printf("ROB1_COLLECTION_TRACE width=%u ready=11 start=12 done=%u retire=%u "
      "sectors=3 wb_denied=1 ref_final=0 ledger_closed=1 kernels=0 gpu_cycle_calls=0 threads_started=0\n",
      width,done,done+2);
}
