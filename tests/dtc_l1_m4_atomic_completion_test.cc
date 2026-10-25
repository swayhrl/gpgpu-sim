// CPU component regression: real LDST global mem_fetch WB, real bounded
// frontends and real core/cluster/GPU drain. No kernel execution or GPU cycle.
#include "gpgpu-sim/gpu-sim.h"
#include "gpgpu-sim/shader.h"
#include "gpgpu-sim/icnt_wrapper.h"
#include "gpgpu-sim/scoreboard.h"
#include "cuda-sim/ptx_ir.h"
#include "gpgpu_context.h"
#include "gpgpusim_entrypoint.h"
#include "option_parser.h"
#include <csignal>
#include <functional>
#include <ctime>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
static void rejects(const std::function<void()> &f) {
  auto pid = fork(); assert(pid >= 0);
  if (!pid) { std::signal(SIGABRT, SIG_DFL); f(); _exit(0); }
  int status; assert(waitpid(pid, &status, 0) == pid);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}
int main(int argc, const char **argv) {
  assert(argc == 4); rlimit limit{0, 0}; setrlimit(RLIMIT_CORE, &limit);
  const unsigned mode = std::atoi(argv[2]);
  const bool io = mode == 2, oo = mode == 3 || mode == 4;
  gpgpu_context ctx; gpgpu_sim_config cfg(&ctx);
  auto parser = option_parser_create(); cfg.reg_options(parser); icnt_reg_options(parser);
  ctx.ptx_reg_options(parser); ctx.func_sim->ptx_opcocde_latency_options(parser);
  option_parser_cfgfile(parser, argv[1]);
  const char *opts[] = {"M4_CPU", "-gpgpu_n_clusters", "1", "-gpgpu_n_cores_per_cluster", "1",
    "-gpgpu_n_mem", "1", "-gpgpu_n_sub_partition_per_mchannel", "1",
    "-gpgpu_shader_core_pipeline", "64:32", "-gpgpu_shader_cta", "2",
    "-gpgpu_smem_latency", "2", "-network_mode", "2", "-gpgpu_dtc_l1_mode", argv[2],
    "-gpgpu_dtc_l1_finite_service", mode == 2 || mode == 3 ? "1" : "0",
    "-gpgpu_dtc_l1_tag_hash_bankrow", argv[3],
    "-gpgpu_dtc_l1_progressive_wakeup", "0", "-gpgpu_dtc_l1_invalidate_on_source_event", "0",
    "-gpgpu_l1_cache_write_ratio", "0", "-gpgpu_gmem_skip_L1D", "0",
    "-enable_ptx_file_line_stats", "0", "-gpgpu_dtc_l1_debug_event_limit", "0",
    "-power_simulation_enabled", "0", "-visualizer_enabled", "0",
    "-gpgpu_clock_domains", "1410.0:1410.0:1410.0:850.0"};
  option_parser_cmdline(parser, sizeof(opts) / sizeof(opts[0]), opts); cfg.init();
  ctx.the_gpgpusim->g_simulation_starttime = std::time(nullptr);
  auto *gpu = new exec_gpgpu_sim(cfg, &ctx); gpu->init();
  auto *cluster = gpu->get_simt_core_clusters()[0];
  auto *core = cluster->get_shader_cores()[0]; auto *ld = core->m_ldst_unit;
  auto *rf = dynamic_cast<opndcoll_rfu_t *>(ld->m_operand_collector); assert(rf);
  const auto *sc = gpu->getShaderCoreConfig();
  active_mask_t lanes; lanes.set(0); lanes.set(2); lanes.set(5);
  auto inst = [&](unsigned uid, bool returning = false, active_mask_t mask = active_mask_t(1)) {
    warp_inst_t i(sc); std::fill(std::begin(i.out), std::end(i.out), 0U);
    i.issue(mask, 0, 1, 0, 0, 0); i.m_uid = uid; i.pc = 0;
    i.op = LOAD_OP; i.op_pipe = MEM__OP; i.space = memory_space_t(global_space);
    i.cache_op = CACHE_GLOBAL; i.m_isatomic = true; i.data_size = 4;
    if (returning) { i.out[0] = 1; i.arch_reg.dst[0] = 1; }
    return i;
  };
  auto admit = [&](warp_inst_t &i) {
    return ld->dtc_l1_m4_observe_admit(i, ldst_unit::dtc_l1_m4_operation::ATOMIC);
  };
  auto step = [&] { ++gpu->gpu_sim_cycle; rf->process_banks(); ld->cycle(); };
  auto drain = [&] {
    for (unsigned n = 0; ld->dtc_l1_pending_work(); ++n) { assert(n < 1024); step(); }
  };
  auto wb = [&](warp_inst_t &i, active_mask_t mask) {
    mem_access_byte_mask_t bytes; bytes.set(0); mem_access_sector_mask_t sectors; sectors.set(0);
    mem_access_t a(GLOBAL_ACC_R, 0, 32, false, mask, bytes, sectors, &ctx);
    auto *mf = new mem_fetch(a, &i, 0, 8, 0, 0, 0, &cfg.m_memory_config, gpu->global_cycle());
    mf->set_reply(); ld->m_next_global.push_back(mf);
    ++gpu->gpu_sim_cycle; rf->process_banks(); ld->writeback();
    assert(ld->m_next_global.empty());
  };
  if (!io && !oo) {
    auto i = inst(1); assert(admit(i)); assert(!ld->dtc_l1_pending_work());
    ld->dtc_l1_m4_atomic_response(i, active_mask_t(1));
    std::puts("PASS LEGACY/PAPER_BASE excluded from M4 sidecar response tracking"); return 0;
  }
  // RED and no-destination ATOMG share exactly this source-decoded shape.
  for (bool returning : {false, true}) {
    auto i = inst(returning ? 101 : 100, returning, lanes); assert(admit(i)); assert(admit(i));
    rejects([&] { ld->dtc_l1_m4_observe_complete(i); }); // no dispatch-time closure
    core->m_warp[0]->m_n_atomic = 3;
    if (returning) { ld->m_pending_writes[0][1] = 2; ld->m_scoreboard->reserveRegisters(&i); }
    const auto completions = ld->m_dtc_l1_m4_source_completions;
    active_mask_t first; first.set(0); first.set(5); active_mask_t last; last.set(2);
    wb(i, first); assert(core->m_warp[0]->get_n_atomic() == 1);
    assert(ld->m_dtc_l1_m4_source_completions == completions);
    assert(admit(i)); // retrying dispatch must not reset already returned lanes
    rejects([&] { ld->dtc_l1_m4_atomic_response(i, first); });
    rejects([&] { ld->dtc_l1_m4_atomic_response(i, active_mask_t()); });
    rejects([&] { ld->dtc_l1_m4_atomic_response(i, active_mask_t(1ULL << 9)); });
    if (returning) assert(ld->m_pending_writes[0][1] == 1);
    wb(i, last); assert(core->m_warp[0]->get_n_atomic() == 0);
    assert(ld->m_dtc_l1_m4_source_completions == completions + 1);
    rejects([&] { ld->dtc_l1_m4_atomic_response(i, last); });
    if (returning) assert(ld->m_pending_writes[0].count(1) == 0);
    drain(); rejects([&] { ld->dtc_l1_m4_atomic_response(i, last); });
    assert(ld->m_dtc_l1_m4_source_completions == ld->m_dtc_l1_m4_observation_retires);
  }
  active_mask_t ma; ma.set(0); ma.set(2); active_mask_t mb; mb.set(1); mb.set(5);
  auto ia = inst(102, false, ma), ib = inst(103, false, mb); assert(admit(ia) && admit(ib));
  core->m_warp[0]->m_n_atomic = 4; const auto interleaved = ld->m_dtc_l1_m4_source_completions;
  wb(ia, active_mask_t(1)); assert(ld->m_dtc_l1_m4_source_completions == interleaved);
  wb(ib, mb); assert(ld->m_dtc_l1_m4_source_completions == interleaved + 1);
  wb(ia, active_mask_t(1ULL << 2)); assert(ld->m_dtc_l1_m4_source_completions == interleaved + 2);
  assert(core->m_warp[0]->get_n_atomic() == 0); drain();
  auto inactive = inst(200, false, active_mask_t()); assert(admit(inactive));
  auto shared = inst(201); shared.space = memory_space_t(shared_space); assert(admit(shared));
  ld->dtc_l1_m4_atomic_response(shared, active_mask_t(1)); assert(!ld->dtc_l1_pending_work());
  // Real finite PIB pressure and one-per-cycle retirement; preserve each
  // mode's configured capacity (IO=256, OO=128).
  const unsigned capacity = io ? cfg.m_shader_config.dtc_l1_io_pib_entries : cfg.m_shader_config.dtc_l1_oo_pib_entries;
  assert(capacity == (io ? 256U : 128U));
  for (unsigned n = 0; n < capacity; ++n) { auto i = inst(1000 + n); assert(admit(i)); }
  auto extra = inst(1000 + capacity); assert(!admit(extra));
  auto first = inst(1000); ld->dtc_l1_m4_atomic_response(first, active_mask_t(1)); step();
  assert(admit(extra));
  for (unsigned n = 1; n <= capacity; ++n) {
    auto i = inst(1000 + n); ld->dtc_l1_m4_atomic_response(i, active_mask_t(1));
  }
  const auto before_retire = ld->m_dtc_l1_m4_observation_retires; step();
  assert(ld->m_dtc_l1_m4_observation_retires == before_retire + 1);
  drain(); assert(ld->m_dtc_l1_m4_source_completions == ld->m_dtc_l1_m4_observation_retires);
  // Two finished architectural kernels: A's metadata may be published after
  // its own tail drains even while unrelated B has a live sidecar.
  function_info entry(1, &ctx);
  kernel_info_t a(dim3(1, 1, 1), dim3(32, 1, 1), &entry, 1);
  kernel_info_t b(dim3(1, 1, 1), dim3(32, 1, 1), &entry, 2);
  assert(a.get_uid() && b.get_uid());
  gpu->m_running_kernels[0] = &a; gpu->m_running_kernels[1] = &b;
  gpu->gpu_kernel_time[1][a.get_uid()] = {}; gpu->gpu_kernel_time[2][b.get_uid()] = {};
  auto fa = inst(3000); fa.m_isatomic = false; fa.m_is_proxy_fence = true;
  auto fb = inst(3001); fb.m_isatomic = false; fb.m_is_proxy_fence = true;
  core->m_kernel = &a;
  assert(ld->dtc_l1_m4_observe_admit(fa, ldst_unit::dtc_l1_m4_operation::PROXY_FENCE));
  core->m_kernel = &b;
  assert(ld->dtc_l1_m4_observe_admit(fb, ldst_unit::dtc_l1_m4_operation::PROXY_FENCE));
  core->m_kernel = nullptr; assert(!core->isactive() && core->get_not_completed() == 0);
  gpu->set_kernel_done(&a); gpu->set_kernel_done(&a); gpu->set_kernel_done(&b);
  assert(gpu->finished_kernel() == 0 && gpu->m_dtc_draining_kernels.size() == 2);
  assert(gpu->active() && core->dtc_l1_pending_work() && cluster->dtc_l1_pending_work());
  ld->dtc_l1_m4_observe_complete(fa);
  ++gpu->gpu_sim_cycle; rf->process_banks(); core->cycle(); // real early-return gate must let tail run
  assert(!ld->dtc_l1_pending_work(a.get_uid()) && ld->dtc_l1_pending_work(b.get_uid()));
  assert(gpu->finished_kernel() == a.get_uid()); assert(gpu->finished_kernel() == 0);
  ld->dtc_l1_m4_observe_complete(fb);
  ++gpu->gpu_sim_cycle; rf->process_banks(); core->cycle();
  assert(gpu->finished_kernel() == b.get_uid()); assert(gpu->finished_kernel() == 0);
  assert(!ld->dtc_l1_pending_work() && gpu->m_dtc_draining_kernels.empty());
  auto *stats = tmpfile(); assert(stats); gpu->shader_print_dtc_l1_stats(stats); fclose(stats);
  if (mode == 3) {
    auto &f = *ld->m_dtc_l1_oo_frontend;
    const auto c = gpu->global_cycle(); assert(f.admit(9000)); auto id = f.access(c, 9000, 0).physical;
    f.complete(id); f.advance_service(c + 1); assert(f.retire_ready_uid(c + 1, 9000));
    assert(f.allocated_lines() == 1 && f.active_refs() == 0);
    assert(!ld->dtc_l1_pending_work()); // retained valid data are not pending work
  }
  std::printf("PASS real_LDST mode=%u AE=%s zero_dest_RED_ATOMG returning_atomic out_of_order_disjoint_multi_response once_duplicate_reject zeroactive_shared_exclusion finitePIB=%u retirewidth1 threadsended_pendingdrain kernel_specific_done retained_Ref0_notbusy kernels_executed=0 gpu_cycle_calls=0\n", mode, argv[3], capacity);
}
