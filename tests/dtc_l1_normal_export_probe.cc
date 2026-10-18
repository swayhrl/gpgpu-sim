// CPU component fixture. Calls the actual production gpu_print_stat(), with
// real GPU/cluster/core/LDST objects but NO kernel, trace replay, GPU cycle(),
// runtime simulation thread, or simulator executable. -fno-access-control is
// only a test setup/readback seam, never a production printing substitute.
#include "gpgpu-sim/gpu-sim.h"
#include "gpgpu-sim/shader.h"
#include "gpgpu-sim/icnt_wrapper.h"
#include "gpgpu_context.h"
#include "gpgpusim_entrypoint.h"
#include "option_parser.h"
#include <array>
#include <cstring>
#include <dlfcn.h>
#include <sys/resource.h>
#include <ctime>
#include <fstream>
#include <string>
#include <unistd.h>

static std::array<ldst_unit *, 64> units(exec_gpgpu_sim *gpu) {
  std::array<ldst_unit *, 64> result{};
  unsigned n = 0;
  const auto *cfg = gpu->getShaderCoreConfig();
  for (unsigned cluster = 0; cluster < cfg->n_simt_clusters; ++cluster)
    for (unsigned local = 0; local < cfg->n_simt_cores_per_cluster; ++local)
      result.at(n++) = gpu->get_simt_core_clusters()[cluster]
                            ->get_shader_cores()[local]->m_ldst_unit;
  assert(n == result.size());
  return result;
}

static void normal_boundary(exec_gpgpu_sim *gpu, unsigned number) {
  const auto all = units(gpu);
  std::array<std::array<unsigned char,
      sizeof(dtc_l1::source_invalidation_observer)>, 64> before{};
  std::array<std::array<uint64_t, 4>, 64> state{};
  for (unsigned sm = 0; sm < 64; ++sm) {
    std::memcpy(before[sm].data(), &all[sm]->m_dtc_invalidation, before[sm].size());
    state[sm] = {all[sm]->m_dtc_service_sample_cycle,
                 all[sm]->m_dtc_service_occupancy_sample_cycle,
                 all[sm]->m_dtc_l1_oo_frontend->allocated_lines(),
                 all[sm]->m_dtc_l1_oo_frontend->active_refs()};
  }
  const uint64_t clock = gpu->global_cycle();
  std::printf("X2_CPU_NORMAL_BOUNDARY_BEGIN id=%u\n", number);
  gpu->gpu_print_stat(0); // The normal entrypoint, not a private printer.
  std::printf("X2_CPU_NORMAL_BOUNDARY_END id=%u\n", number);
  assert(gpu->global_cycle() == clock);
  for (unsigned sm = 0; sm < 64; ++sm) {
    assert(std::memcmp(before[sm].data(), &all[sm]->m_dtc_invalidation,
                       before[sm].size()) == 0);
    assert((state[sm] == std::array<uint64_t, 4>{
        all[sm]->m_dtc_service_sample_cycle,
        all[sm]->m_dtc_service_occupancy_sample_cycle,
        all[sm]->m_dtc_l1_oo_frontend->allocated_lines(),
        all[sm]->m_dtc_l1_oo_frontend->active_refs()}));
  }
}

int main(int argc, const char **argv) {
  if (argc != 5) return 64; // config, omitted|0|1, no_event|events, cores/cluster
  rlimit limit{0, 0};
  setrlimit(RLIMIT_CORE, &limit);
  Dl_info binding{};
  void *entry = dlsym(RTLD_DEFAULT, "_ZN9gpgpu_sim14gpu_print_statEy");
  assert(entry && dladdr(entry, &binding));
  std::printf("X2_CPU_LOADED_LIBRARY path=%s\n", binding.dli_fname);
  char executable[4096]{};
  const ssize_t length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
  assert(length > 0);
  std::printf("X2_CPU_LOADED_ELF path=%s\n", executable);
  std::ifstream maps("/proc/self/maps");
  std::string mapping;
  bool mapped = false;
  while (std::getline(maps, mapping)) {
    if (mapping.find(binding.dli_fname) != std::string::npos) {
      mapped = true;
      std::printf("X2_CPU_LIBRARY_MAP %s\n", mapping.c_str());
    }
  }
  assert(mapped);
  gpgpu_context ctx;
  gpgpu_sim_config cfg(&ctx);
  auto parser = option_parser_create();
  cfg.reg_options(parser);
  icnt_reg_options(parser);
  ctx.ptx_reg_options(parser);
  ctx.func_sim->ptx_opcocde_latency_options(parser);
  option_parser_cfgfile(parser, argv[1]);
  const bool multi_core = std::strcmp(argv[4], "8") == 0;
  assert(multi_core || std::strcmp(argv[4], "1") == 0);
  const char *options[] = {"X2_CPU",
      "-gpgpu_n_clusters", multi_core ? "8" : "64",
      "-gpgpu_n_cores_per_cluster", multi_core ? "8" : "1",
      "-gpgpu_n_mem", "1", "-gpgpu_n_sub_partition_per_mchannel", "1",
      "-gpgpu_shader_core_pipeline", "64:32", "-gpgpu_shader_cta", "2",
      "-gpgpu_smem_latency", "2", "-network_mode", "2",
      "-gpgpu_dtc_l1_mode", "3", "-gpgpu_dtc_l1_finite_service", "1",
      "-gpgpu_l1_cache_write_ratio", "0", "-gpgpu_gmem_skip_L1D", "0",
      "-enable_ptx_file_line_stats", "0", "-gpgpu_flush_l1_cache", "1",
      "-gpgpu_runtime_stat", "500000", "-gpgpu_dtc_l1_debug_event_limit", "0",
      "-power_simulation_enabled", "0", "-visualizer_enabled", "0",
      "-gpgpu_clock_domains", "1410.0:1410.0:1410.0:850.0"};
  option_parser_cmdline(parser, sizeof(options) / sizeof(options[0]), options);
  if (std::strcmp(argv[2], "omitted") != 0) {
    const char *flag[] = {"X2_CPU", "-gpgpu_dtc_l1_invalidate_on_source_event", argv[2]};
    option_parser_cmdline(parser, 3, flag);
  }
  assert(cfg.m_shader_config.dtc_l1_debug_event_limit == 0);
  assert(std::strcmp(cfg.gpgpu_runtime_stat, "500000") == 0);
  cfg.init();
  assert(cfg.gpu_runtime_stat_flag == 0 && cfg.gpu_stat_sample_freq == 500000);
  ctx.the_gpgpusim->g_simulation_starttime = std::time(nullptr);
  auto *gpu = new exec_gpgpu_sim(cfg, &ctx);
  gpu->init();
  const auto all = units(gpu);
  normal_boundary(gpu, 0);
  const bool events = std::strcmp(argv[3], "events") == 0;
  if (events) {
    // Prepare real frontend API state: ready Ref0 retained and ready live Ref1.
    // These are CPU metadata inputs, not simulated instructions or payloads.
    auto &front = *all[0]->m_dtc_l1_oo_frontend;
    front.advance_service(1);
    assert(front.admit(100));
    const auto retained = front.access(1, 100, 0).physical;
    front.complete(retained);
    front.advance_service(2);
    assert(front.retire_ready_uid(2, 100));
    front.advance_service(3);
    assert(front.admit(101));
    const auto live = front.access(3, 101, 128).physical;
    front.complete(live);
    front.advance_service(4);
    // Controlled ordinary owner holds the unchanged production drain gate.
    all[0]->m_L1D->m_extra_mf_fields.emplace(nullptr, baseline_cache::extra_mf_fields());
    gpu->gpu_sim_cycle = 10;
    auto *first = gpu->get_simt_core_clusters()[0]->get_shader_cores()[0];
    first->cache_invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER);
    first->cache_invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER);
    first->m_warp[0]->set_membar();
    assert(!first->warp_waiting_at_mem_barrier(0));
    assert(all[0]->m_L1D->m_invalidate_pending);
    front.advance_service(11);
    assert(front.retire_ready_uid(11, 101));
    gpu->gpu_sim_cycle = 12;
    auto *last = gpu->get_simt_core_clusters()[cfg.m_shader_config.n_simt_clusters - 1]
                     ->get_shader_cores()[cfg.m_shader_config.n_simt_cores_per_cluster - 1];
    last->cache_invalidate(dtc_l1::invalidation_source::CACHE_CONFIG_CHANGE);
  }
  gpu->gpu_sim_cycle = 13;
  normal_boundary(gpu, 1);
  if (events) {
    assert(all[0]->m_L1D->m_extra_mf_fields.erase(nullptr) == 1);
    gpu->gpu_sim_cycle = 21;
    all[0]->cycle(); // CPU LDST component only: original ordinary commit now drains.
    gpu->gpu_sim_cycle = 22;
    gpu->get_simt_core_clusters()[0]->get_shader_cores()[0]->cache_invalidate(
        dtc_l1::invalidation_source::IDLE_CLUSTER);
  }
  gpu->gpu_sim_cycle = 22;
  normal_boundary(gpu, 2);
  std::puts("X2_CPU_COMPONENT_RESULT boundaries=3 sms=64 kernels=0 "
            "gpu_cycle_calls=0 simulation_threads_started=0 print_state_unchanged=1");
  return 0;
}
