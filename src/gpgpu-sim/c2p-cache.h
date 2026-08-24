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
  C2P_PROBE_POLICY_DISTANCE_BINS = C2P_PROBE_POLICY_MAX_ORDINAL + 1,
  // Waiting C2P fallbacks are bucketed as 0, 1, 2--3, and 4 or more.
  C2P_PROBE_POLICY_FALLBACK_PRESSURE_BINS = 4,
  C2P_ADDR_OBSERVE_REGION_BUCKETS = 32
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
  // candidate; after a miss, one feature x candidate-bin package decision
  // selects either lower fallback or confirmation through ordinal four.
  // Disabled by default so canonical C2P is unchanged.
  bool adaptive_probe_policy;
  unsigned adaptive_probe_score_threshold;
  unsigned adaptive_probe_explore_period;
  // Reset score for every entry in the one 64 x 4 confirmation table. The
  // legal three-bit score range is 0..7.
  unsigned adaptive_probe_initial_score;
  // Experimental guardrail: after the first miss, scan a Snapshot candidate
  // list of at most four entries to completion. Disabled by default.
  bool adaptive_probe_force_full_small_candidates;
  // Select the bounded package after every compulsory first miss.  The four
  // fixed bins are 1--2, 3--4, 5--8, and 9+ initial candidates.
  bool adaptive_probe_package_policy;
  // Select an address-region x requester-cluster hash in place of the PC
  // hash for package decisions.  Both choices use exactly 64 x 4 3-bit
  // entries; this is an experiment-time selector, not extra predictor state.
  bool adaptive_probe_addr_topology_policy;
  // Read-only simulator observation of every post-miss decision point. It
  // never changes candidate issue, fallback, or resource arbitration.
  bool adaptive_probe_observe_tail;
  // Further split the first post-miss observation by a line-address region
  // hash and requester topology.  This is an offline feature study only.
  bool adaptive_probe_observe_addr_topology;
  // A separate locality partition for C2P experiments.  It is deliberately
  // independent from comparator_cluster_size: changing it must not alter the
  // ATA/CCD comparator scope or canonical C2P candidate ordering.
  bool locality_observe;
  unsigned locality_group_size;
  // Optional two-tier far-L1 sensitivity model.  Canonical C2P leaves these
  // disabled/zero and retains its historical ordering and timing.
  bool locality_aware_candidate_order;
  unsigned locality_outer_probe_extra_latency;
  unsigned locality_outer_return_extra_latency;
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
  // The CCN comparator sends new misses to the baseline L2 path when its
  // finite request network is congested.  Keep strict stalling selectable so
  // older experiments remain reproducible, but do not conflate it with CCN.
  bool ring_queue_fallback;
  // The legacy comparator serializes all request injection through one global
  // source.  The optional model instead reserves the directed links crossed
  // by a request, preserving two cycles per hop while allowing disjoint ring
  // segments to carry independent requests concurrently.
  bool ring_link_pipeline;
  // CCN-RT [Dublish et al.] periodically samples each source SM's observed
  // remote-hit rate.  A source below the configured threshold bypasses CCN
  // until its next sampling epoch; its shadow tags remain visible to peers.
  bool ring_request_throttle;
  unsigned ring_throttle_sample_instructions;
  unsigned ring_throttle_period_instructions;
  unsigned ring_throttle_min_hit_percent;
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
  // Default-off 4-SM locality observation.  Each exact/snapshot class is
  // one of none, local-only, outer-only, or both; probe classes are local
  // versus outer at the time the target is actually selected.
  unsigned long long locality_observed_queries;
  unsigned long long locality_snapshot_class[4];
  unsigned long long locality_exact_accept_class[4];
  unsigned long long locality_exact_query_class[4];
  unsigned long long locality_candidates_local;
  unsigned long long locality_candidates_outer;
  unsigned long long locality_probes_local;
  unsigned long long locality_probes_outer;
  unsigned long long locality_probe_hits_local;
  unsigned long long locality_probe_hits_outer;
  unsigned long long locality_probe_misses_local;
  unsigned long long locality_probe_misses_outer;
  // RING-only path accounting.  A traversal is recorded once at copied-tag
  // issue, before the later data-array access or lower-memory fallback.
  unsigned long long ring_traversals;
  unsigned long long ring_no_match_traversals;
  unsigned long long ring_traversal_hops;
  unsigned long long ring_network_wait_cycles;
  unsigned long long ring_queue_bypasses;
  unsigned long long ring_throttle_bypasses;
  unsigned long long ring_throttle_samples;
  unsigned long long ring_throttle_sample_requests;
  unsigned long long ring_throttle_sample_hits;
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
  // Candidate-bin x ordinal separates short-list remote hits from long tails.
  unsigned long long probe_candidate_bin_ordinal_hits
      [C2P_PROBE_POLICY_CANDIDATE_BINS][C2P_PROBE_POLICY_ORDINAL_BINS];
  unsigned long long probe_candidate_bin_ordinal_misses
      [C2P_PROBE_POLICY_CANDIDATE_BINS][C2P_PROBE_POLICY_ORDINAL_BINS];
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
  unsigned long long adaptive_continue_forced;
  unsigned long long adaptive_stop_predictor;
  unsigned long long adaptive_stop_hard_cap;
  unsigned long long adaptive_stop_later_peer;
  unsigned long long adaptive_stop_no_later_peer;
  unsigned long long adaptive_stop_remaining_candidates;
  unsigned long long adaptive_stop_next_peer_distance_total;
  // Read-only exact-peer histogram at learned early-stop points. The fixed
  // four-probe cap is intentionally excluded from this diagnostic.
  unsigned long long adaptive_early_stop_opportunities
      [C2P_PROBE_POLICY_CANDIDATE_BINS];
  unsigned long long adaptive_early_stop_later_peer
      [C2P_PROBE_POLICY_CANDIDATE_BINS];
  unsigned long long adaptive_early_stop_no_later_peer
      [C2P_PROBE_POLICY_CANDIDATE_BINS];
  unsigned long long adaptive_early_stop_distance
      [C2P_PROBE_POLICY_CANDIDATE_BINS][C2P_PROBE_POLICY_DISTANCE_BINS];
  // These account only the stop-to-lower-send delay. Pressure is the number
  // of already waiting C2P fallbacks, bucketed as 0, 1, 2--3, and 4+.
  unsigned long long adaptive_early_stop_lower_samples
      [C2P_PROBE_POLICY_FALLBACK_PRESSURE_BINS];
  unsigned long long adaptive_early_stop_lower_cycles
      [C2P_PROBE_POLICY_FALLBACK_PRESSURE_BINS];
  unsigned long long adaptive_early_stop_lower_waited
      [C2P_PROBE_POLICY_FALLBACK_PRESSURE_BINS];
  unsigned long long adaptive_first_probe_hits;
  unsigned long long adaptive_first_probe_misses;
  unsigned long long adaptive_first_probe_timeouts;
  unsigned long long adaptive_predictor_probe_hits;
  unsigned long long adaptive_predictor_probe_misses;
  unsigned long long adaptive_predictor_probe_timeouts;
  unsigned long long adaptive_exploration_probe_hits;
  unsigned long long adaptive_exploration_probe_misses;
  unsigned long long adaptive_exploration_probe_timeouts;
  unsigned long long adaptive_forced_probe_hits;
  unsigned long long adaptive_forced_probe_misses;
  unsigned long long adaptive_forced_probe_timeouts;
  unsigned long long adaptive_score_hist[8];
  unsigned long long adaptive_package_opportunities;
  unsigned long long adaptive_package_start_predictor;
  unsigned long long adaptive_package_start_exploration;
  unsigned long long adaptive_package_start_forced;
  unsigned long long adaptive_package_stop_predictor;
  unsigned long long adaptive_package_hit;
  unsigned long long adaptive_package_no_hit;
  unsigned long long adaptive_package_timeout;
  unsigned long long adaptive_package_score_hist[8];
  // A package can exhaust its four probes with candidates still available.
  // Keep that residual exact-peer opportunity distinct from ordinary package
  // misses, so policy traffic and its lost sharing opportunity are auditable.
  unsigned long long adaptive_package_residual_opportunities;
  unsigned long long adaptive_package_residual_later_peer;
  unsigned long long adaptive_package_residual_no_later_peer;
  unsigned long long adaptive_package_residual_remaining_candidates;
  unsigned long long adaptive_package_residual_next_peer_distance_total;
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
    PROBE_EXPLORATION,
    PROBE_FORCED
  };

  enum package_outcome { PACKAGE_HIT, PACKAGE_NO_HIT, PACKAGE_TIMEOUT };

  enum locality_class {
    LOCALITY_NONE = 0,
    LOCALITY_LOCAL_ONLY = 1,
    LOCALITY_OUTER_ONLY = 2,
    LOCALITY_BOTH = 3
  };

  struct addr_topology_observation {
    addr_topology_observation()
        : opportunities(0), later_peer(0), within_4(0), lower_ready(0),
          target_credit(0) {}
    unsigned long long opportunities;
    unsigned long long later_peer;
    unsigned long long within_4;
    unsigned long long lower_ready;
    unsigned long long target_credit;
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
    bool adaptive_package_active;
    bool adaptive_package_outcome_recorded;
    bool adaptive_early_stop_pending;
    unsigned adaptive_early_stop_pressure;
    unsigned long long adaptive_early_stop_cycle;
    unsigned probe_sid;
    unsigned peer_accesses;
    bool oracle_peer_hit;
    bool sharing_attempt;
    bool ring_started;
    bool ring_throttle_sampled;
    unsigned long long ring_throttle_epoch;
    unsigned probe_latency;
    unsigned return_latency;
  };

  struct ring_throttle_state {
    ring_throttle_state()
        : epoch((unsigned long long)-1), sampling(true), inject_enabled(true),
          sample_requests(0), sample_hits(0) {}
    unsigned long long epoch;
    bool sampling;
    bool inject_enabled;
    unsigned long long sample_requests;
    unsigned long long sample_hits;
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
  bool adaptive_should_start_package(transaction &txn);
  void adaptive_record_probe_result(transaction &txn, bool hit);
  void adaptive_record_probe_timeout(transaction &txn);
  void adaptive_record_package_outcome(transaction &txn,
                                       package_outcome outcome);
  void adaptive_record_package_residual(const transaction &txn);
  void adaptive_observe_tail(transaction &txn);
  void adaptive_observe_addr_topology(const transaction &txn, bool later_peer,
                                      unsigned distance, bool lower_ready,
                                      bool target_credit);
  void adaptive_record_stop(transaction &txn, bool hard_cap,
                            unsigned long long now);
  unsigned adaptive_candidate_bin(const transaction &txn) const;
  unsigned adaptive_package_score_index(const transaction &txn) const;
  unsigned adaptive_addr_topology_bucket(const transaction &txn) const;
  void record_peer_accesses(bool hit, unsigned accesses);
  void record_locality_accept(l1_cache *requester, mem_fetch *mf);
  void record_locality_query(const transaction &txn);
  void record_locality_probe_issue(const transaction &txn, unsigned target_sid);
  void record_locality_probe_result(const transaction &txn, bool hit);
  locality_class exact_locality_class(l1_cache *requester,
                                      mem_fetch *mf) const;
  bool same_locality_group(unsigned from_sid, unsigned to_sid) const;
  unsigned locality_group_id(unsigned sid) const;
  unsigned locality_probe_latency(const transaction &txn,
                                  unsigned target_sid) const;
  unsigned locality_return_latency(const transaction &txn) const;
  bool has_exact_peer(l1_cache *requester, mem_fetch *mf) const;
  std::vector<unsigned> exact_candidates(const transaction &txn,
                                         bool cluster_only) const;
  std::vector<unsigned> ordered_candidates(const transaction &txn) const;
  unsigned cluster_distance(unsigned from_sid, unsigned to_sid) const;
  unsigned cluster_size() const;
  unsigned cluster_id(unsigned sid) const;
  unsigned ring_distance(unsigned from_sid, unsigned to_sid) const;
  bool ring_throttle_allows(unsigned sid);
  void ring_throttle_record_hit(const transaction &txn);

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
  std::vector<unsigned long long> m_ring_link_next_issue_cycle;
  std::vector<ring_throttle_state> m_ring_throttle;
  std::vector<unsigned long long> m_peer_access_hit_hist;
  std::vector<unsigned long long> m_peer_access_miss_hist;
  // The only adaptive predictor state: one 3-bit score for each selected
  // feature-hash x candidate-count-bin package decision. PC-hash and
  // address/topology are separate configurations with identical 64 x 4
  // capacity (768 logical bits); AddrTopo never depends on an instruction PC.
  std::vector<unsigned char> m_adaptive_package_scores;
  unsigned long long m_adaptive_explore_counter;
  unsigned m_addr_observe_cluster_count;
  std::vector<addr_topology_observation> m_addr_observe_region;
  std::vector<addr_topology_observation> m_addr_observe_cluster;
  std::vector<addr_topology_observation> m_addr_observe_region_cluster;
  c2p_cache_stats m_stats;
};

#endif
