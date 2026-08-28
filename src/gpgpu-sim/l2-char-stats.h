// Timing-neutral L2 workload-characterization collector (schema L2CHARV1).
#ifndef L2_CHAR_STATS_INCLUDED
#define L2_CHAR_STATS_INCLUDED

#include <map>
#include <string>
#include <vector>

class mem_fetch;

struct l2_char_occ_stats {
  l2_char_occ_stats();
  void init(unsigned capacity);
  void sample(unsigned value);
  unsigned percentile(unsigned numerator, unsigned denominator) const;
  double average() const;
  double utilization() const;
  double full_ratio() const;

  unsigned capacity;
  bool unbounded;
  unsigned long long samples;
  unsigned long long sum;
  unsigned long long maximum;
  unsigned long long nonzero_cycles;
  unsigned long long full_cycles;
  std::vector<unsigned long long> hist;
};

struct l2_char_block_stats {
  l2_char_block_stats();
  unsigned long long eligible_cycles;
  unsigned long long blocked_cycles;
  unsigned long long blocked_requests;
  unsigned long long blocking_episodes;
};

struct l2_char_storage_snapshot {
  l2_char_storage_snapshot()
      : reserved(0), dirty(0), valid(0), max_reserved_set(0),
        all_reserved_sets(0) {}
  unsigned reserved;
  unsigned dirty;
  unsigned valid;
  unsigned max_reserved_set;
  unsigned all_reserved_sets;
  std::vector<unsigned> reserved_by_set;
};

struct l2_char_mshr_state {
  l2_char_mshr_state() : address(0), targets(0), ready(false) {}
  l2_char_mshr_state(unsigned long long a, unsigned t, bool r)
      : address(a), targets(t), ready(r) {}
  unsigned long long address;
  unsigned targets;
  bool ready;
};

struct l2_char_cycle_sample {
  l2_char_cycle_sample();
  l2_char_storage_snapshot storage;
  unsigned mshr_entries, mshr_ready_entries, mshr_targets, mshr_ready_targets;
  unsigned merge_limit_entries, max_merge_depth;
  unsigned missq, missq_demand, missq_wb, missq_other_write;
  unsigned l2dramq, draml2q, l2icntq, icntl2q, rop;
  bool data_port_busy, fill_port_busy;
  std::vector<l2_char_mshr_state> mshr_states;
};

class l2_char_collector {
 public:
  l2_char_collector(unsigned slice_id, unsigned sets, unsigned ways,
                    unsigned mshr_entries, unsigned merge_limit,
                    unsigned missq_capacity, unsigned l2dramq_capacity,
                    unsigned draml2q_capacity, unsigned l2icntq_capacity,
                    unsigned icntl2q_capacity, unsigned window_cycles,
                    bool set_detail, bool emit_windows);

  void sample_cycle(unsigned long long cycle, const l2_char_cycle_sample &s);
  void record_frontend(mem_fetch *mf, unsigned long long cycle,
                       unsigned eligible_mask, unsigned blocked_mask);
  void clear_frontend_request(mem_fetch *mf);
  void record_fill(bool eligible, bool blocked);
  void record_rop(bool eligible, bool blocked);
  void record_mshr_response(bool eligible, bool blocked);
  void record_lower_drain(bool eligible, bool blocked);
  void record_dram_return(bool eligible, bool blocked);
  void record_dram_issue(bool is_read, bool is_wb, bool return_block,
                         bool credit_block, bool scheduler_block);
  void record_data_port_accept(unsigned source);
  void record_wb_generated(unsigned bytes);
  void record_l2dram_push_class(unsigned klass);
  void record_l2dram_pop_class(unsigned klass);
  void observe_queue_classes(unsigned missq_total, unsigned missq_demand,
                             unsigned missq_wb, unsigned missq_other,
                             unsigned l2dram_total);
  void print(FILE *fp) const;
  bool invariants_hold(std::string *why) const;

 private:
  enum { kFrontendReasons = 7, kQueueClasses = 4 };
  struct request_state {
    request_state() : prev_blocked_mask(0) {}
    unsigned prev_blocked_mask;
  };
  struct lifetime_state {
    lifetime_state() : alloc_cycle(0), ready_cycle(0), allocated(false), ready_seen(false) {}
    unsigned long long alloc_cycle, ready_cycle;
    bool allocated, ready_seen;
  };

  void sample_occ(l2_char_occ_stats &all, l2_char_occ_stats &window,
                  unsigned value);
  void record_block(l2_char_block_stats &stats, bool eligible, bool blocked);
  void observe_mshr_lifetimes(unsigned long long cycle,
                              const std::vector<l2_char_mshr_state> &states);
  void close_window(unsigned long long end_cycle);
  std::string occ_fields(const char *prefix, const l2_char_occ_stats &occ) const;
  std::string block_fields(const char *prefix,
                           const l2_char_block_stats &block) const;

  unsigned m_slice_id, m_sets, m_ways, m_window_cycles;
  bool m_set_detail, m_emit_windows;
  unsigned long long m_cycles, m_window_start, m_window_samples;
  l2_char_occ_stats m_reserved, m_dirty, m_valid;
  l2_char_occ_stats m_mshr_entries, m_mshr_ready, m_mshr_targets;
  l2_char_occ_stats m_merge_limit_entries, m_merge_depth;
  l2_char_occ_stats m_missq, m_missq_demand, m_missq_wb, m_missq_other;
  l2_char_occ_stats m_l2dramq, m_draml2q, m_l2icntq, m_icntl2q, m_rop;
  l2_char_occ_stats m_set_reserved_distribution;
  l2_char_occ_stats m_all_reserved_sets;
  unsigned long long m_max_reserved_ways_any_set;
  unsigned long long m_cycles_any_set_all_reserved;
  l2_char_occ_stats m_window_reserved, m_window_mshr_entries;
  l2_char_occ_stats m_window_mshr_targets, m_window_missq, m_window_missq_wb;
  l2_char_occ_stats m_window_l2dramq, m_window_draml2q;
  unsigned long long m_data_busy, m_fill_busy;
  unsigned long long m_window_data_busy, m_window_fill_busy;
  l2_char_block_stats m_frontend[kFrontendReasons];
  l2_char_block_stats m_fill, m_rop_block, m_mshr_response, m_lower_drain;
  l2_char_block_stats m_dram_return;
  unsigned long long m_dram_issue_eligible, m_dram_issue_returnq;
  unsigned long long m_dram_issue_credit, m_dram_issue_scheduler;
  unsigned long long m_dram_read_returnq, m_dram_read_credit, m_dram_read_scheduler;
  unsigned long long m_dram_wb_credit, m_dram_wb_scheduler;
  unsigned long long m_data_accept_hit, m_data_accept_dirty, m_data_accept_other;
  unsigned long long m_wb_requests, m_wb_bytes;
  unsigned long long m_window_wb_requests, m_window_wb_bytes;
  unsigned m_l2dram_class[kQueueClasses];
  bool m_l2dram_class_error;
  std::map<mem_fetch *, request_state> m_frontend_requests;
  std::map<unsigned long long, lifetime_state> m_lifetimes;
  l2_char_occ_stats m_lifetime_pending, m_lifetime_drain, m_lifetime_total;
  std::vector<std::string> m_windows;
};

#endif
