#ifndef DTC_L1_INVALIDATION_H
#define DTC_L1_INVALIDATION_H

#include <cstdio>
#include <cstdlib>
#include "dtc-l1-common.h"
#include "../option_parser.h"

namespace dtc_l1 {

enum class invalidation_source : unsigned {
  UNSPECIFIED, IDLE_CLUSTER, MEMBAR, CACHE_CONFIG_CHANGE
};

inline const char *invalidation_source_name(invalidation_source source) {
  switch (source) {
    case invalidation_source::IDLE_CLUSTER: return "IDLE_CLUSTER";
    case invalidation_source::MEMBAR: return "MEMBAR";
    case invalidation_source::CACHE_CONFIG_CHANGE: return "CACHE_CONFIG_CHANGE";
    default: return "UNSPECIFIED";
  }
}

inline void register_source_invalidation_option(option_parser_t parser,
                                                bool *enabled) {
  option_parser_register(parser, "-gpgpu_dtc_l1_invalidate_on_source_event",
                         OPT_BOOL, enabled,
                         "Opt-in OO lookup invalidation at existing LDST source "
                         "requests; preserves physical owners (IO unsupported)",
                         "0");
}

inline void check_source_invalidation_mode(bool enabled, unsigned selected) {
  if (!enabled || selected == static_cast<unsigned>(mode::PAPER_OO)) return;
  const char *reason = selected == static_cast<unsigned>(mode::PAPER_IO)
                           ? "IO_K1_UNIMPLEMENTED_STOP"
                           : "OO_K1_ONLY_MODERN_AND_BASE_OUT_OF_SCOPE_STOP";
  std::fprintf(stderr, "%s: -gpgpu_dtc_l1_invalidate_on_source_event=1\n", reason);
  std::abort();
}

// Read-only adapter. Ordinary cache code, its invalidation/drain predicate,
// and cycle implementation are inherited unchanged. Instantiated only for K1.
template <class Cache>
class invalidation_observed_cache : public Cache {
 public:
  using Cache::Cache;
  bool invalidation_pending() const { return this->m_invalidate_pending; }
};

// No model state or service port lives here. These are observation counters;
// each actual ordinary call is made exactly once in its original order.
class source_invalidation_observer {
 public:
  void configure(unsigned sm, unsigned event_limit) {
    m_sm = sm;
    m_events_left = event_limit;
  }

  template <class Cache>
  lookup_invalidation_result request(uint64_t cycle, invalidation_source source,
                                     oo_frontend &frontend, Cache &ordinary) {
    ++requests;
    ++source_counts[static_cast<unsigned>(source)];
    if (first_request_cycle == UINT64_MAX) first_request_cycle = cycle;
    last_request_cycle = cycle;
    if (!pending_first_request) pending_first_request = requests;
    pending_last_request = requests;
    const auto delta = frontend.invalidate_lookup(cycle);
    changed += delta.changed;
    retained += delta.retained;
    reclaimed += delta.reclaimed;
    no_op_requests += delta.changed == 0;
    ++order;
    if (event_slot())
      std::fprintf(stderr,
          "DTC_INVALIDATE_V1 sm=%u order=%llu phase=LDST_SOURCE_REQUEST "
          "cycle=%llu request=%llu source=%s changed=%llu retained=%llu "
          "reclaimed=%llu\n", m_sm, ull(order), ull(cycle), ull(requests),
          invalidation_source_name(source), ull(delta.changed),
          ull(delta.retained), ull(delta.reclaimed));
    ordinary.invalidate();
    // invalidate() always sets pending before its retirement attempt. A false
    // pending bit on return therefore witnesses one actual commit attempt,
    // including the tag_array unused/no-op case (not a claim of Tag changes).
    if (!ordinary.invalidation_pending())
      commit(cycle, "ORDINARY_REQUEST_RETURN");
    return delta;
  }

  template <class Cache>
  void cycle(uint64_t cycle, Cache &ordinary) {
    const bool pending = ordinary.invalidation_pending();
    ordinary.cycle();
    if (pending && !ordinary.invalidation_pending())
      commit(cycle, "ORDINARY_CACHE_CYCLE_RETURN");
  }

  void print(FILE *fp) const {
    std::fprintf(fp,
        "DTC_INVALIDATE_V1_SUMMARY sm=%u requests=%llu idle=%llu membar=%llu "
        "config=%llu unspecified=%llu ordinary_commits=%llu changed=%llu "
        "retained_at_event=%llu reclaimed_at_event=%llu no_op_requests=%llu "
        "first_request_cycle=%llu last_request_cycle=%llu "
        "first_ordinary_commit_cycle=%llu last_ordinary_commit_cycle=%llu "
        "pending_first_request=%llu pending_last_request=%llu "
        "event_records_omitted=%llu\n", m_sm, ull(requests),
        ull(source_counts[1]), ull(source_counts[2]), ull(source_counts[3]),
        ull(source_counts[0]), ull(ordinary_commits), ull(changed), ull(retained),
        ull(reclaimed), ull(no_op_requests), ull(first_request_cycle),
        ull(last_request_cycle), ull(first_ordinary_commit_cycle),
        ull(last_ordinary_commit_cycle), ull(pending_first_request),
        ull(pending_last_request), ull(events_omitted));
  }

  uint64_t requests = 0, ordinary_commits = 0, order = 0;
  std::array<uint64_t, 4> source_counts{};
  uint64_t changed = 0, retained = 0, reclaimed = 0, no_op_requests = 0;
  uint64_t first_request_cycle = UINT64_MAX, last_request_cycle = UINT64_MAX;
  uint64_t first_ordinary_commit_cycle = UINT64_MAX;
  uint64_t last_ordinary_commit_cycle = UINT64_MAX;
  uint64_t pending_first_request = 0, pending_last_request = 0;
  uint64_t events_omitted = 0;

 private:
  static unsigned long long ull(uint64_t value) { return value; }
  bool event_slot() {
    if (m_events_left) { --m_events_left; return true; }
    ++events_omitted;
    return false;
  }
  void commit(uint64_t cycle, const char *phase) {
    assert(pending_first_request && pending_last_request);
    ++ordinary_commits;
    ++order;
    if (first_ordinary_commit_cycle == UINT64_MAX)
      first_ordinary_commit_cycle = cycle;
    last_ordinary_commit_cycle = cycle;
    if (event_slot())
      std::fprintf(stderr,
          "DTC_INVALIDATE_V1 sm=%u order=%llu phase=%s cycle=%llu "
          "ordinary_commit=%llu first_request=%llu last_request=%llu\n",
          m_sm, ull(order), phase, ull(cycle), ull(ordinary_commits),
          ull(pending_first_request), ull(pending_last_request));
    pending_first_request = pending_last_request = 0;
  }
  unsigned m_sm = 0, m_events_left = 0;
};
}  // namespace dtc_l1
#endif
