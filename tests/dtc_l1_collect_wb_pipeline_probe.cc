// CPU component fixture: real LDST issue/admission helpers, cycle(), WB
// arbitration, RFU register-bank grants, completion ledger and normal printer.
// Setup may call admission helpers between edges to align fully-ready inputs.
// Lower responses are controlled CPU packets, not an ICNT/L2/DRAM simulation.
// No kernel, GPU cycle(), trace replay, native GPU or simulation thread.
#include "gpgpu-sim/gpu-sim.h"
#include "gpgpu-sim/shader.h"
#include "gpgpu-sim/icnt_wrapper.h"
#include "gpgpu_context.h"
#include "gpgpusim_entrypoint.h"
#include "option_parser.h"
#include <cstring>
#include <ctime>
#include <dlfcn.h>
#include <fstream>
#include <numeric>
#include <string>
#include <sys/resource.h>
#include <unistd.h>

struct cpu_link : mem_fetch_interface {
  std::deque<mem_fetch *> roots;
  bool full(unsigned, bool) const override { return false; }
  void push(mem_fetch *mf) override { roots.push_back(mf); }
};

struct fixture {
  gpgpu_context ctx;
  gpgpu_sim_config cfg{&ctx};
  exec_gpgpu_sim *gpu;
  shader_core_ctx *core;
  ldst_unit *unit;
  cpu_link link;
  bool on = false;
  uint64_t edge_start = 0;
  uint64_t expected_sectors = 0;
  unsigned loads = 0;
  unsigned snapshots = 0;

  fixture(const char *config, const char *flag, const char *mode,
          const char *finite, const char *k1, const char *sms) {
    auto parser = option_parser_create();
    cfg.reg_options(parser);
    icnt_reg_options(parser);
    ctx.ptx_reg_options(parser);
    ctx.func_sim->ptx_opcocde_latency_options(parser);
    option_parser_cfgfile(parser, config);
    const char *args[] = {"AC_CPU", "-gpgpu_n_clusters", sms,
        "-gpgpu_n_cores_per_cluster", "1", "-gpgpu_n_mem", "1",
        "-gpgpu_n_sub_partition_per_mchannel", "1",
        "-gpgpu_shader_core_pipeline", "128:32", "-gpgpu_shader_cta", "2",
        "-gpgpu_smem_latency", "2", "-network_mode", "2",
        "-gpgpu_dtc_l1_mode", mode, "-gpgpu_dtc_l1_finite_service", finite,
        "-gpgpu_l1_cache_write_ratio", "0", "-gpgpu_gmem_skip_L1D", "0",
        "-enable_ptx_file_line_stats", "0", "-gpgpu_flush_l1_cache", "1",
        "-gpgpu_runtime_stat", "500000", "-gpgpu_dtc_l1_debug_event_limit", "0",
        "-gpgpu_dtc_l1_invalidate_on_source_event", k1,
        "-power_simulation_enabled", "0", "-visualizer_enabled", "0",
        "-gpgpu_clock_domains", "1410.0:1410.0:1410.0:850.0"};
    option_parser_cmdline(parser, sizeof(args) / sizeof(args[0]), args);
#ifndef DTC_COLLECT_WB_PROBE_BASELINE
    assert(!cfg.m_shader_config.dtc_l1_collect_wb_pipeline);
    if (std::strcmp(flag, "omitted")) {
      const char *opt[] = {"AC_CPU", "-gpgpu_dtc_l1_collect_wb_pipeline", flag};
      option_parser_cmdline(parser, 3, opt);
    }
    on = cfg.m_shader_config.dtc_l1_collect_wb_pipeline;
#else
    assert(!std::strcmp(flag, "omitted") || !std::strcmp(flag, "0"));
#endif
    cfg.init();
    assert(cfg.gpu_runtime_stat_flag == 0 && cfg.gpu_stat_sample_freq == 500000);
    ctx.the_gpgpusim->g_simulation_starttime = std::time(nullptr);
    gpu = new exec_gpgpu_sim(cfg, &ctx);
    gpu->init();
    core = gpu->get_simt_core_clusters()[0]->get_shader_cores()[0];
    unit = core->m_ldst_unit;
    unit->m_icnt = &link;
    assert(dynamic_cast<opndcoll_rfu_t *>(unit->m_operand_collector));
  }
  uint64_t now() const { return gpu->global_cycle(); }
  dtc_l1::oo_frontend &front() { return *unit->m_dtc_l1_oo_frontend; }
  void setup_advance() {
    ++gpu->gpu_sim_cycle;
    front().advance_service(now());
  }
  warp_inst_t instruction(unsigned warp = 0, unsigned reg = 1) {
    warp_inst_t inst(&cfg.m_shader_config);
    inst.op = LOAD_OP;
    inst.memory_op = memory_load;
    inst.op_pipe = MEM__OP;
    inst.space.set_type(global_space);
    inst.cache_op = CACHE_ALL;
    inst.data_size = 4;
    inst.pc = 0x100 + loads * 4;
    inst.initiation_interval = inst.latency = 1;
    std::fill(std::begin(inst.out), std::end(inst.out), 0);
    std::fill(std::begin(inst.in), std::end(inst.in), 0);
    for (auto &dst : inst.arch_reg.dst) dst = -1;
    for (auto &src : inst.arch_reg.src) src = -1;
    inst.out[0] = reg;
    inst.arch_reg.dst[0] = reg;
    active_mask_t active;
    active.set();
    inst.issue(active, warp, now(), warp, 0, 0);
    return inst;
  }
  void prime(unsigned lines, uint64_t base) {
    setup_advance();
    const uint64_t seed = 1000000 + now();
    assert(front().admit(seed));
    std::vector<dtc_l1::physical_identity> identities;
    for (unsigned i = 0; i < lines; ++i) {
      setup_advance();
      const auto result = front().access(now(), seed, base + i * 128);
      assert(result.kind == dtc_l1::io_access_kind::NEW_MISS);
      identities.push_back(result.physical);
      front().complete(result.physical);
    }
    while (!front().entry_ready_for(seed)) setup_advance();
    setup_advance();
    assert(front().retire_ready_uid(now(), seed));
  }
  // Production issue() creates the ledger/pending writes. Its production
  // admission helper creates the real PIB and full/partial physical ownership.
  unsigned enqueue(unsigned lines, uint64_t base, unsigned warp = 0,
                   int admit = -1, bool keep_dispatch = false,
                   bool start_advance = true) {
    if (start_advance) setup_advance();
    warp_inst_t inst = instruction(warp, loads + 1);
    for (unsigned i = 0; i < lines; ++i) {
      mem_access_byte_mask_t bytes;
      mem_access_sector_mask_t sectors;
      bytes.set(); sectors.set();
      inst.m_accessq.push_back(mem_access_t(GLOBAL_ACC_R, base + i * 128, 128,
          false, inst.get_active_mask(), bytes, sectors, &ctx));
    }
    const unsigned uid = inst.get_uid();
    register_set issue(1, "AC_CPU_ISSUE");
    **issue.get_free() = inst;
    core->m_warp[warp]->inc_inst_in_pipeline();
    unit->m_scoreboard->reserveRegisters(&inst);
    unit->issue(issue);
    ++loads;
    expected_sectors += lines * 4;
    const unsigned target = admit < 0 ? lines : static_cast<unsigned>(admit);
    unsigned attempts = 0;
    do {
      mem_stage_stall_type reason = NO_RC_FAIL;
      mem_stage_access_type type = G_MEM_LD;
      unit->dtc_l1_oo_memory_cycle(*unit->m_dispatch_reg, reason, type);
      assert(++attempts < 5000);
      if (unit->dtc_l1_oo_find_entry(uid)->next_reference >= target) break;
      setup_advance();
    } while (true);
    if (!keep_dispatch) {
      // Controlled setup seam substitutes for the identical dispatch-drain
      // edge at the tail of LDST cycle(); no completion/Ref release here.
      assert(target == lines);
      core->dec_inst_in_pipeline(warp);
      unit->m_dispatch_reg->clear();
    }
    return uid;
  }
  warp_inst_t sidecar(ldst_unit::dtc_l1_m4_operation operation, unsigned warp) {
    setup_advance();
    auto inst = instruction(warp, 0);
    assert(unit->dtc_l1_m4_observe_admit(inst, operation));
    return inst;
  }
  void respond() {
    while (!link.roots.empty()) {
      mem_fetch *root = link.roots.front(); link.roots.pop_front();
      for (unsigned sector : {2U, 0U, 3U, 1U}) {
        mem_access_byte_mask_t bytes;
        mem_access_sector_mask_t mask;
        for (unsigned i = sector * 32; i < (sector + 1) * 32; ++i) bytes.set(i);
        mask.set(sector);
        mem_access_t access(GLOBAL_ACC_R, root->get_addr() + sector * 32, 32,
            false, root->get_inst().get_active_mask(), bytes, mask, &ctx);
        auto *child = new mem_fetch(access, &root->get_inst(), 0, READ_PACKET_SIZE,
            root->get_wid(), 0, 0, &cfg.m_memory_config, now(), root);
        child->set_reply();
        unit->fill(child);
      }
    }
  }
  void block_bank() {
    assert(!unit->m_dtc_l1_oo_pib.empty());
    auto inst = unit->m_dtc_l1_oo_pib.front().inst;
    // Actual RFU request reserves the same bank; the production DTC attempt
    // must now fail in opndcoll_rfu_t::writeback(), without a mock grant path.
    assert(unit->m_operand_collector->writeback(inst));
  }
  void edge(bool deny = false, bool returns = true, unsigned microsteps = 1) {
    ++gpu->gpu_sim_cycle;
    if (!edge_start) edge_start = now();
    unit->m_operand_collector->step();
    if (returns) respond();
    if (deny && !unit->m_dtc_l1_oo_pib.empty()) block_bank();
    const uint64_t before_retire = front().retires();
    const uint64_t before_sectors = unit->m_dtc_service_collected_sectors;
    for (unsigned micro = 0; micro < microsteps; ++micro) unit->cycle();
    assert(front().retires() - before_retire <= 1);
    assert(unit->m_dtc_service_collected_sectors - before_sectors <= 4);
    front().assert_shadow_refs();
    uint64_t shadow = 0;
    for (const auto &entry : front().m_entries) shadow += entry.references.size();
    assert(shadow == front().active_refs());
    assert(gpu->dtc_l1_lower_requests_acquired() ==
           gpu->dtc_l1_lower_requests_released() + gpu->dtc_l1_lower_outstanding());
    std::printf("AC_EDGE cycle=%llu pib=%zu refs=%llu allocated=%zu "
                "sectors=%llu retires=%llu closed=%llu inflight=%zu fifo=%zu "
                "create_used=%u inject_used=%u refill_used=%u retire_used=%u collect_used=%u\n",
        now(), unit->m_dtc_l1_oo_pib.size(), (unsigned long long)front().active_refs(),
        front().allocated_lines(),
        (unsigned long long)(unit->m_dtc_service_collected_sectors - before_sectors),
        (unsigned long long)(front().retires() - before_retire),
        (unsigned long long)unit->m_dtc_l1_oo_completion_dependencies_closed,
        unit->m_dtc_l1_oo_inflight.size(), unit->m_response_fifo.size(),
        used(unit->m_dtc_service_create), used(unit->m_dtc_service_inject),
        used(unit->m_dtc_service_refill), used(unit->m_dtc_service_retire),
        used(unit->m_dtc_service_collect));
    for (const auto &entry : unit->m_dtc_l1_oo_pib)
      std::printf("AC_ENTRY cycle=%llu uid=%u admitted=%zu total=%zu "
          "ready=%llu collect_start=%llu collect_done=%llu left=%u\n", now(),
          entry.inst.get_uid(), entry.next_reference, entry.references.size(),
          (unsigned long long)entry.service.ready_cycle,
          (unsigned long long)entry.service.collect_start_cycle,
          (unsigned long long)entry.service.collect_done_cycle, entry.service.sectors_left);
#ifndef DTC_COLLECT_WB_PROBE_BASELINE
    if (on) {
      const auto &p = unit->m_dtc_collect_wb;
      assert(p.sectors == unit->m_dtc_service_collected_sectors);
      assert(p.cacheable_retires == unit->m_dtc_service_retired);
      assert(p.retires == p.cacheable_retires + p.sidecar_retires);
      assert(p.result_sectors + p.collecting_sectors <= p.bank_capacity_sectors * 2);
      assert(p.cycle_sectors <= 4 && p.cycle_retires <= 1);
      assert(std::accumulate(p.sector_hist.begin(), p.sector_hist.end(), uint64_t(0)) == p.samples);
      assert(p.retire_hist[0] + p.retire_hist[1] == p.samples);
      assert(p.retire_hist[1] == p.retires);
    }
#endif
  }
  unsigned used(const dtc_l1::service_quota &q) {
    return q.m_cycle == now() ? q.m_used : 0;
  }
  void drain(unsigned microsteps = 1) {
    for (unsigned count = 0; !unit->m_dtc_l1_oo_pib.empty(); ++count) {
      assert(count < 2000);
      edge(false, true, microsteps);
    }
    assert(front().active_refs() == 0 && front().occupancy() == 0);
    assert(unit->m_dtc_l1_oo_completion_dependencies == unit->m_dtc_l1_oo_completion_dependencies_closed);
    assert(unit->m_dtc_l1_oo_lower_created == unit->m_dtc_l1_oo_lower_issued &&
           unit->m_dtc_l1_oo_lower_created == unit->m_dtc_l1_oo_lower_responses);
    assert(unit->m_dtc_service_collected_sectors == expected_sectors);
    assert(unit->m_dtc_service_retired == loads);
    assert(unit->m_dtc_service_ready_to_complete_sum ==
           unit->m_dtc_service_ready_to_collect_sum +
           unit->m_dtc_service_collection_cycles + unit->m_dtc_service_wb_wait);
    assert(unit->m_response_fifo.empty() && link.roots.empty());
    assert(gpu->dtc_l1_lower_outstanding() == 0);
  }
  void normal() {
    std::printf("AC_NORMAL_BEGIN id=%u\n", snapshots);
    gpu->gpu_print_stat(0);
    std::printf("AC_NORMAL_END id=%u\n", snapshots++);
  }
};

int main(int argc, const char **argv) {
  if (argc != 9) return 64; // config flag scenario sectors mode finite K1 SMs
  rlimit core_limit{0, 0}; setrlimit(RLIMIT_CORE, &core_limit);
  Dl_info binding{};
  assert(dladdr(dlsym(RTLD_DEFAULT, "_ZN9gpgpu_sim14gpu_print_statEy"), &binding));
  std::printf("AC_LIBRARY %s\n", binding.dli_fname);
  char self[4096]{};
  assert(readlink("/proc/self/exe", self, sizeof(self) - 1) > 0);
  std::printf("AC_ELF %s\n", self);
  std::ifstream maps("/proc/self/maps");
  std::string line;
  while (std::getline(maps, line))
    if (line.find(binding.dli_fname) != std::string::npos)
      std::printf("AC_LIBRARY_MAP %s\n", line.c_str());
  fixture f(argv[1], argv[2], argv[5], argv[6], argv[7], argv[8]);
  const std::string scenario = argv[3];
  const unsigned sectors = std::stoul(argv[4]);
  assert(sectors && sectors % 4 == 0);
  if (scenario == "mode_only") return 0;
  f.normal(); // true no-event/no-sample baseline through the normal entrypoint
  if (scenario == "empty") {
    for (unsigned i = 0; i < 6; ++i) f.edge();
  } else if (scenario == "sidecars") {
    const auto before = f.gpu->gpu_sim_insn;
    using op = ldst_unit::dtc_l1_m4_operation;
    for (auto kind : {op::STORE, op::ATOMIC, op::BYPASS_LOAD, op::PROXY_FENCE}) {
      auto inst = f.sidecar(kind, 0);
      f.unit->dtc_l1_m4_observe_complete(inst);
    }
    f.drain();
    assert(f.gpu->gpu_sim_insn == before); // no duplicated architecture effects
    assert(f.unit->m_dtc_l1_m4_source_completions == 4 &&
           f.unit->m_dtc_l1_m4_observation_retires == 4);
  } else if (scenario == "late_waiter" || scenario == "generation") {
    const unsigned count = scenario == "late_waiter" ? 12 : 1;
    for (unsigned i = 0; i < count; ++i) f.enqueue(1, 0);
    if (scenario == "generation") {
      const auto old = f.front().m_entries.front().references.front();
      f.unit->invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER);
      assert(!f.front().tag_visible(old) && f.front().ref_count(old) == 1);
      f.enqueue(1, 0, 1);
      const auto replacement = f.front().m_entries.back().references.front();
      assert(old.id != replacement.id && f.front().fill_identity_matches(old));
      f.unit->invalidate(dtc_l1::invalidation_source::CACHE_CONFIG_CHANGE);
    }
    bool late_added = false;
    for (unsigned step = 0; !f.unit->m_dtc_l1_oo_pib.empty(); ++step) {
      assert(step < 300);
      f.edge(false, true, 3);
      if (scenario == "late_waiter" && !late_added && f.front().wakeups() == 4) {
        const auto id = f.front().m_entries.front().references.front();
        assert(!f.front().m_phys[id.id].ready);
        f.enqueue(1, 0, 1, -1, false, false);
        late_added = true;
      }
    }
    assert(scenario != "late_waiter" || (late_added && f.front().wakeups() == 13));
    f.drain();
  } else if (scenario == "fence") {
    f.prime(1, 0); f.prime(1, 128);
    auto fence = f.sidecar(ldst_unit::dtc_l1_m4_operation::PROXY_FENCE, 0);
    const unsigned blocked = f.enqueue(1, 0, 0);
    const unsigned independent = f.enqueue(1, 128, 1);
    for (unsigned i = 0; i < 5; ++i) {
      f.edge();
      auto *entry = f.unit->dtc_l1_oo_find_entry(blocked);
      assert(entry && entry->service.collect_start_cycle == UINT64_MAX);
    }
    assert(!f.unit->dtc_l1_oo_find_entry(independent));
    f.unit->dtc_l1_m4_observe_complete(fence);
    f.drain();
  } else {
    const unsigned count = scenario == "single" || scenario == "partial" ? 1 :
                           scenario == "denied" ? 3 : 2;
    for (unsigned i = 0; i < count; ++i) f.prime(sectors / 4, i * 4096);
    for (unsigned i = 0; i < count; ++i)
      f.enqueue(sectors / 4, i * 4096, i % 2,
                scenario == "partial" ? 1 : -1, scenario == "partial");
    if (scenario == "partial") {
      assert(sectors >= 8);
      f.edge();
      assert(f.unit->m_dtc_service_collected_sectors == 0);
    }
    if (scenario == "k1_refs") {
      const uint64_t refs = f.front().active_refs();
      f.unit->invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER);
      for (unsigned i = 0; i < sectors / 4 + 4; ++i) {
        f.edge(true);
        if (i == 2) f.unit->invalidate(dtc_l1::invalidation_source::MEMBAR);
        assert(f.front().active_refs() == refs);
      }
    }
    if (scenario == "denied")
      for (unsigned i = 0; i < 8; ++i) f.edge(true, true, 3);
    f.drain(3);
    if (scenario == "k1_refs") assert(f.front().allocated_lines() == 0);
  }
  f.normal();
  f.edge(); // distinct last full cumulative snapshot; never add the snapshots
  f.normal();
  std::printf("AC_RESULT status=PASS_CPU_LDST_COMPONENT scenario=%s on=%u "
              "sectors=%llu loads=%u lower=%llu kernel_launches=0 gpu_cycles=0 "
              "simulation_threads=0 value_correctness=UNPROVEN\n",
      scenario.c_str(), f.on, (unsigned long long)f.expected_sectors, f.loads,
      (unsigned long long)f.unit->m_dtc_l1_oo_lower_created);
}
