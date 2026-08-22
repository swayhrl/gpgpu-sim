// C2P-Cache: private-L1 candidate pruning model.
//
// This model deliberately sits beside the normal L1 miss path.  It never
// replaces an L1 tag array or an MSHR: an accepted miss is either completed by
// the normal L1 fill path after an exact peer hit, or the original mem_fetch is
// forwarded unchanged to the baseline lower-memory path.

#ifndef C2P_CACHE_H
#define C2P_CACHE_H

#include <stdint.h>
#include <stdio.h>

#include <deque>
#include <list>
#include <utility>
#include <vector>

class OptionParser;
class gpgpu_sim;
class l1_cache;
class mem_fetch;

// These bins are observation-only.  They cover the first confirmations that
// the adaptive C2P+ design may later choose between, while keeping a single
// overflow bin for longer candidate chains.
enum {
  C2P_PROBE_POLICY_MAX_ORDINAL = 4,
  C2P_PROBE_POLICY_ORDINAL_BINS = C2P_PROBE_POLICY_MAX_ORDINAL + 1,
  C2P_PROBE_POLICY_PC_BUCKETS = 64,
  C2P_PROBE_POLICY_CANDIDATE_BINS = 4,
  C2P_PROBE_POLICY_DISTANCE_BINS = C2P_PROBE_POLICY_MAX_ORDINAL + 1
};

class c2p_cache_config {
 public:
  enum sharing_scheme {
    C2P_SCHEME = 0,
    ATA_SCHEME = 1,
    CCD_SCHEME = 2,
    RING_SCHEME = 3
  };

  c2p_cache_config();
  void reg_options(OptionParser *opp);

  bool enabled;
  bool oracle_only;
  bool ideal_peer_lookup;
  bool collect_oracle;
  unsigned bf_engines;
  unsigned bf_latency;
  // The default 64 BF rows plus 16 tag-mask rows in each of 64 banks is the
  // paper's 5,120-row logical Snapshot Matrix.  These knobs support the
  // Figure-13 m/k sweep while preserving that exact default encoding.
  unsigned snapshot_bf_rows_per_bank;
  unsigned bf_hashes;
  unsigned snapshot_latency;
  unsigned remote_tag_latency;
  unsigned remote_return_latency;
  unsigned query_queue_size;
  unsigned update_queue_size;
  unsigned update_transport_bytes_per_cycle;
  unsigned snapshot_rebuild_interval;
  unsigned probe_timeout;
  unsigned target_probe_queue_size;
  // C2P+ only: zero preserves the default exhaustive candidate scan.
  unsigned max_candidate_probes;
  // C2P+ adaptive confirmation policy.  A request always probes its first
  // candidate; later candidates are selected by a small PC-hash/ordinal
  // utility table.  Disabled by default so canonical C2P is unchanged.
  bool adaptive_probe_policy;
  unsigned adaptive_probe_score_threshold;
  unsigned adaptive_probe_explore_period;
  // When enabled, retain a distinct adaptive score for each initial Snapshot
  // candidate-count bin: 1--2, 3--4, 5--8, and 9+ candidates.
  bool adaptive_probe_candidate_count_bins;
  // Read-only simulator observation of every post-miss decision point. It
  // never changes candidate issue, fallback, or resource arbitration.
  bool adaptive_probe_observe_tail;
  // C2P+ counterfactual: a one-request-per-cycle remote tag pipeline that
  // does not reserve the target L1 data port.
  bool separate_target_tag_port;
  // Diagnostic-only control: bypass just the target-L1 data-port contention
  // for C2P probes.  It is off for every architectural experiment.
  bool diagnostic_target_port_bypass;
  unsigned snapshot_copies;
  unsigned scheme;
  unsigned comparator_cluster_size;
  unsigned ata_cluster_issue_width;
  unsigned ata_tag_latency;
  unsigned ccd_predictor_latency;
  unsigned ccd_broadcast_latency;
  unsigned ring_hop_latency;
  unsigned peer_line_latency;
};

struct c2p_cache_stats {
  unsigned long long l1_misses;
  unsigned long long oracle_peer_hits;
  unsigned long long queries_accepted;
  unsigned long long queries_queue_bypass;
  unsigned long long updates_queue_bypass;
  unsigned long long candidate_total;
  unsigned long long candidate_queries;
  unsigned long long peer_probes;
  unsigned long long peer_probe_hits;
  unsigned long long peer_probe_misses;
  unsigned long long peer_l1_accesses;
  // These are observation counters only.  They separate target-port
  // contention, FIFO residence, and requester-fill backpressure without
  // changing C2P transaction timing.
  unsigned long long target_probe_port_busy_cycles;
  unsigned long long target_tag_port_busy_cycles;
  unsigned long long target_probe_queue_wait_cycles;
  unsigned long long target_probe_queue_full_cycles;
  unsigned long long requester_fill_wait_cycles;
  // Per-transaction state residence is observation-only.  It attributes an
  // accepted miss's lifetime without changing state transitions or latency.
  unsigned long long residence_encode_cycles;
  unsigned long long residence_rows_cycles;
  unsigned long long residence_match_cycles;
  unsigned long long residence_ready_cycles;
  unsigned long long residence_target_probe_cycles;
  unsigned long long residence_probe_cycles;
  unsigned long long residence_return_cycles;
  unsigned long long residence_fallback_cycles;
  // Probe ordinal distinguishes long false-candidate tails from a target-side
  // stall after a useful candidate was already selected.
  unsigned long long remote_hit_probe_ordinal_total;
  unsigned long long remote_hit_probe_ordinal_samples;
  unsigned long long fallback_probe_ordinal_total;
  unsigned long long fallback_probe_ordinal_samples;
  // Observation-only evidence for a later adaptive confirmation policy.  The
  // first four bins correspond to exact probe ordinals; bin four is overflow.
  // The PC table is an offline hash study, not a predictor consulted by C2P.
  unsigned long long probe_ordinal_hits[C2P_PROBE_POLICY_ORDINAL_BINS];
  unsigned long long probe_ordinal_misses[C2P_PROBE_POLICY_ORDINAL_BINS];
  unsigned long long probe_pc_ordinal_hits[C2P_PROBE_POLICY_PC_BUCKETS]
                                          [C2P_PROBE_POLICY_ORDINAL_BINS];
  unsigned long long probe_pc_ordinal_misses[C2P_PROBE_POLICY_PC_BUCKETS]
                                            [C2P_PROBE_POLICY_ORDINAL_BINS];
  // A continuation decision occurs after one or more failed probes while a
  // next Snapshot candidate exists.  lower_ready and target_credit are
  // sampled for diagnosis only and never influence this transaction.
  unsigned long long continuation_decisions
      [C2P_PROBE_POLICY_ORDINAL_BINS][2][2];
  // Adaptive C2P+ diagnostics.  They distinguish a policy-induced lower
  // fallback that saved a false candidate scan from one that discarded a
  // remaining exact peer, and retain why each probe was issued.
  unsigned long long adaptive_continuation_opportunities;
  unsigned long long adaptive_continue_predictor;
  unsigned long long adaptive_continue_exploration;
  unsigned long long adaptive_stop_predictor;
  unsigned long long adaptive_stop_hard_cap;
  unsigned long long adaptive_stop_later_peer;
  unsigned long long adaptive_stop_no_later_peer;
  unsigned long long adaptive_stop_remaining_candidates;
  unsigned long long adaptive_stop_next_peer_distance_total;
  unsigned long long adaptive_first_probe_hits;
  unsigned long long adaptive_first_probe_misses;
  unsigned long long adaptive_first_probe_timeouts;
  unsigned long long adaptive_predictor_probe_hits;
  unsigned long long adaptive_predictor_probe_misses;
  unsigned long long adaptive_predictor_probe_timeouts;
  unsigned long long adaptive_exploration_probe_hits;
  unsigned long long adaptive_exploration_probe_misses;
  unsigned long long adaptive_exploration_probe_timeouts;
  unsigned long long adaptive_score_hist[8];
  // At every point where a failed probe leaves another candidate, classify the
  // initial candidate-set size and the first later exact peer, if any. These
  // are counterfactual observations: no lookup result is fed into the policy.
  unsigned long long adaptive_tail_observe_opportunities
      [C2P_PROBE_POLICY_CANDIDATE_BINS];
  unsigned long long adaptive_tail_observe_later_peer
      [C2P_PROBE_POLICY_CANDIDATE_BINS];
  unsigned long long adaptive_tail_observe_no_later_peer
      [C2P_PROBE_POLICY_CANDIDATE_BINS];
  unsigned long long adaptive_tail_observe_distance
      [C2P_PROBE_POLICY_CANDIDATE_BINS][C2P_PROBE_POLICY_DISTANCE_BINS];
  // A timeout can occur after a target FIFO admission or while the candidate
  // cannot enter a full target FIFO.  Keep these causes distinct.
  unsigned long long fallback_target_wait_timeout;
  unsigned long long fallback_target_admission_timeout;
  // The accept-time oracle and query-time exact peer set can differ because
  // normal L1 fills/evictions continue while a transaction waits.
  unsigned long long peer_lost_before_query;
  unsigned long long peer_gained_before_query;
  unsigned long long remote_hits;
  unsigned long long fallback_no_candidate;
  unsigned long long fallback_candidates_exhausted;
  unsigned long long fallback_candidate_budget;
  unsigned long long fallback_probe_timeout;
  unsigned long long fallback_queue;
  unsigned long long snapshot_false_positive;
  unsigned long long snapshot_false_negative;
  unsigned long long snapshot_true_positive;
  unsigned long long snapshot_true_negative;
  unsigned long long snapshot_query_false_positive;
  unsigned long long snapshot_query_false_negative;
  unsigned long long snapshot_query_true_positive;
  unsigned long long snapshot_query_true_negative;
  // CCD's two-bit predictor is classified against its exact in-cluster
  // tag-time candidate snapshot; these counters feed the Fig. 12 comparison.
  unsigned long long ccd_false_positive;
  unsigned long long ccd_false_negative;
  unsigned long long ccd_true_positive;
  unsigned long long ccd_true_negative;
  unsigned long long snapshot_updates;
  unsigned long long snapshot_rebuilds;
  unsigned long long snapshot_rebuild_transport_tags;

  c2p_cache_stats();
  void clear();
};

class c2p_cache {
 public:
  // A sharing controller can either consume a miss, leave it to the normal
  // lower-cache path, or deliberately hold the L1 miss queue while a finite
  // discovery structure drains.  The latter is required for RING: silently
  // sending a full-ring request to L2 would erase the traversal bottleneck
  // that defines the comparator.
  enum miss_action { MISS_TO_LOWER, MISS_ACCEPTED, MISS_STALL };

  c2p_cache(const c2p_cache_config &config, gpgpu_sim *gpu);

  bool enabled() const { return m_config.enabled; }
  void reset();
  void register_l1(l1_cache *cache);
  miss_action accept_miss(l1_cache *requester, mem_fetch *mf,
                          unsigned long long now);
  void on_l1_fill(l1_cache *cache, mem_fetch *mf);
  void on_l1_flush(l1_cache *cache);
  void cycle(unsigned long long now);
  void print_stats(FILE *fout) const;
  // Diagnostic only: called after the global simulator has already declared
  // deadlock, so it cannot perturb normal C2P timing or accounting.
  void display_state(FILE *fout) const;
  const c2p_cache_stats &stats() const { return m_stats; }

 private:
  enum transaction_state {
    WAIT_ENCODE,
    WAIT_ROWS,
    WAIT_MATCH,
    READY_TO_PROBE,
    WAIT_TARGET_PROBE,
    WAIT_PROBE,
    WAIT_RETURN,
    WAIT_FALLBACK
  };

  enum probe_issue_reason {
    PROBE_FIRST,
    PROBE_PREDICTOR,
    PROBE_EXPLORATION
  };

  struct transaction {
    transaction(l1_cache *requester, mem_fetch *mf, unsigned requester_sid,
                uint64_t line_tag, unsigned long long now);

    l1_cache *requester;
    mem_fetch *mf;
    unsigned requester_sid;
    uint64_t line_tag;
    unsigned long long enqueue_cycle;
    unsigned long long state_enter_cycle;
    unsigned long long ready_cycle;
    unsigned long long probe_wait_start;
    transaction_state state;
    std::vector<unsigned> rows;
    std::vector<bool> row_done;
    std::vector<unsigned> candidates;
    unsigned candidate_next;
    unsigned probe_pc_bucket;
    probe_issue_reason probe_reason;
    // A failed peer probe may leave its next target FIFO full for several
    // cycles. Decide the adaptive continuation once, then retain that choice
    // until the candidate is actually issued or the transaction falls back.
    bool adaptive_continue_decided;
    bool adaptive_tail_observed;
    unsigned probe_sid;
    unsigned peer_accesses;
    bool oracle_peer_hit;
    bool sharing_attempt;
    bool ring_started;
    unsigned probe_latency;
    unsigned return_latency;
  };

  struct update_entry {
    update_entry(unsigned sid_, uint64_t line_tag_, bool rebuild_)
        : sid(sid_), line_tag(line_tag_), rebuild(rebuild_) {}
    unsigned sid;
    uint64_t line_tag;
    bool rebuild;
  };

  struct pending_update {
    pending_update(const update_entry &entry_, unsigned long long ready_cycle_)
        : entry(entry_), ready_cycle(ready_cycle_) {}
    update_entry entry;
    unsigned long long ready_cycle;
  };

  std::vector<unsigned> query_rows(uint64_t line_tag) const;
  void set_snapshot_bits(unsigned sid, uint64_t line_tag);
  void clear_snapshot_column(unsigned sid);
  bool snapshot_bit(unsigned row, unsigned sid) const;
  void begin_next_rebuild();
  void issue_update(unsigned long long now, unsigned &engines_left);
  void issue_query_encodes(unsigned long long now, unsigned &engines_left);
  void schedule_rows(unsigned long long now);
  void complete_matches(unsigned long long now);
  void service_target_probe_queues(unsigned long long now);
  void advance_probes(unsigned long long now);
  void transition(transaction &txn, transaction_state state,
                  unsigned long long now);
  void retire(transaction &txn, unsigned long long now);
  void begin_fallback(transaction &txn, unsigned long long now);
  bool adaptive_should_continue(transaction &txn);
  void adaptive_record_probe_result(transaction &txn, bool hit);
  void adaptive_record_probe_timeout(transaction &txn);
  void adaptive_observe_tail(transaction &txn);
  void adaptive_record_stop(const transaction &txn, bool hard_cap);
  unsigned adaptive_candidate_bin(const transaction &txn) const;
  unsigned adaptive_score_index(const transaction &txn, unsigned ordinal) const;
  void record_peer_accesses(bool hit, unsigned accesses);
  bool has_exact_peer(l1_cache *requester, mem_fetch *mf) const;
  std::vector<unsigned> exact_candidates(const transaction &txn,
                                         bool cluster_only) const;
  std::vector<unsigned> ordered_candidates(const transaction &txn) const;
  unsigned cluster_distance(unsigned from_sid, unsigned to_sid) const;
  unsigned cluster_size() const;
  unsigned cluster_id(unsigned sid) const;
  unsigned ring_distance(unsigned from_sid, unsigned to_sid) const;

  const c2p_cache_config &m_config;
  gpgpu_sim *m_gpu;
  unsigned m_num_sms;
  unsigned m_words;
  std::vector<l1_cache *> m_l1s;
  std::vector<std::vector<uint64_t> > m_snapshot;
  std::list<transaction> m_transactions;
  std::deque<update_entry> m_update_queue;
  std::deque<pending_update> m_update_pipeline;
  // m_rebuild_sid is the next L1 selected for a rebuild.  Keep the active
  // target separately so a completed rebuild advances instead of repeatedly
  // rebuilding SM 0.
  unsigned m_rebuild_sid;
  unsigned m_rebuild_target_sid;
  bool m_rebuild_active;
  unsigned long long m_next_rebuild_cycle;
  std::vector<uint64_t> m_rebuild_tags;
  unsigned m_rebuild_enqueue_next_tag;
  unsigned m_rebuild_pending_tags;
  std::vector<std::vector<bool> > m_bank_copy_used;
  std::vector<std::deque<transaction *> > m_target_probe_queues;
  std::vector<unsigned long long> m_target_tag_next_issue_cycle;
  std::vector<unsigned> m_ccd_counters;
  unsigned long long m_ata_issue_cycle;
  std::vector<unsigned> m_ata_issues;
  unsigned long long m_ring_next_issue_cycle;
  std::vector<unsigned long long> m_peer_access_hit_hist;
  std::vector<unsigned long long> m_peer_access_miss_hist;
  // Four confirmation ordinals x 64 PC buckets x four candidate-count bins,
  // each represented by a 3-bit saturating utility score. The count-bin
  // dimension is only indexed when configured; otherwise bin zero preserves
  // the original PC-hash x ordinal policy. `unsigned char` makes the intended
  // 1 KiB maximum storage cost explicit even in this software model.
  std::vector<unsigned char> m_adaptive_probe_scores;
  unsigned long long m_adaptive_explore_counter;
  c2p_cache_stats m_stats;
};

#endif
