#include "shader.h"

// Real ordinary baseline-cache pending/drain implementation, with a controlled
// fill-owner fixture. No GPU, kernel, trace replay, or data-value claim.
class ordinary_fixture
    : public dtc_l1::invalidation_observed_cache<read_only_cache> {
 public:
  using invalidation_observed_cache::invalidation_observed_cache;
  void hold_fill_owner() { m_extra_mf_fields.emplace(nullptr, extra_mf_fields()); }
  void drain_fill_owner() { assert(m_extra_mf_fields.erase(nullptr) == 1); }
};

int main() {
  // Exercise actual shader_core_config registration through production parser.
  for (int value : {-1, 0, 1}) {
    shader_core_config cfg(nullptr);
    auto parser = option_parser_create();
    cfg.reg_options(parser);
    assert(!cfg.dtc_l1_invalidate_on_source_event);
    if (value >= 0) {
      const char *argv[] = {"cpu_test",
          "-gpgpu_dtc_l1_invalidate_on_source_event", value ? "1" : "0"};
      option_parser_cmdline(parser, 3, argv);
    }
    assert(cfg.dtc_l1_invalidate_on_source_event == (value == 1));
    option_parser_destroy(parser);
  }
  char geometry[] = "N:1:128:1,L:R:m:N:L,A:2:2,2:0,32";
  cache_config cfg;
  cfg.init(geometry, FuncCachePreferNone);
  ordinary_fixture ordinary("cpu_ordinary", cfg, 0, 0, nullptr,
                            IN_L1C_MISS_QUEUE, L1_GPU_CACHE, nullptr);
  dtc_l1::config model;
  model.selected_mode = dtc_l1::mode::PAPER_OO;
  model.logical_sets = model.logical_ways = 1;
  model.physical_lines = 2;
  dtc_l1::oo_frontend oo(model);
  assert(oo.admit(1));
  const auto old = oo.access(1, 1, 0).physical;
  dtc_l1::source_invalidation_observer events;
  events.configure(7, 32);
  ordinary.hold_fill_owner();
  const auto delta = events.request(2, dtc_l1::invalidation_source::IDLE_CLUSTER,
                                    oo, ordinary);
  assert(delta.changed == 1 && delta.retained == 1);
  assert(ordinary.invalidation_pending() && events.ordinary_commits == 0);
  events.request(2, dtc_l1::invalidation_source::IDLE_CLUSTER, oo, ordinary);
  events.request(3, dtc_l1::invalidation_source::MEMBAR, oo, ordinary);
  assert(events.requests == 3 && events.changed == 1 && events.no_op_requests == 2);
  events.cycle(3, ordinary);
  assert(events.ordinary_commits == 0 && !oo.tag_visible(old));
  ordinary.drain_fill_owner();
  events.cycle(4, ordinary);
  assert(events.ordinary_commits == 1 && events.last_ordinary_commit_cycle == 4);
  assert(!ordinary.invalidation_pending());
  assert(events.first_request_cycle == 2 && events.last_request_cycle == 3);
  assert(events.order == 4 && !events.pending_first_request);
  assert(oo.admit(2));
  const auto next = oo.access(4, 2, 0).physical;
  assert(next.id != old.id);
  events.request(4, dtc_l1::invalidation_source::CACHE_CONFIG_CHANGE, oo, ordinary);
  assert(events.requests == 4 && events.ordinary_commits == 2);
  assert(events.changed == 2 && events.retained == 2 && events.no_op_requests == 2);
  assert(!oo.tag_visible(next) && oo.fill_identity_matches(old));
  assert(events.order == 6 && events.source_counts[1] == 2 &&
         events.source_counts[2] == 1 && events.source_counts[3] == 1);
  // Ordinary drain is independent: old DTC response/Ref is still outstanding.
  assert(oo.fill_identity_matches(old) && oo.ref_count(old) == 1);
  oo.complete(old);
  assert(oo.retire_ready_uid(5, 1) && !oo.is_allocated(old));
  oo.complete(next);
  assert(oo.retire_ready_uid(6, 2) && !oo.is_allocated(next));
  events.print(stdout);
  std::puts("{\"case\":\"K1-production-parser-real-ordinary-deferred-commit\","
            "\"local_cpu_check\":\"PASS\",\"value_correctness\":\"UNPROVEN\"}");
}
