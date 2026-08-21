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
  unsigned snapshot_latency;
  unsigned remote_tag_latency;
  unsigned remote_return_latency;
  unsigned query_queue_size;
  unsigned update_queue_size;
  unsigned update_transport_bytes_per_cycle;
  unsigned snapshot_rebuild_interval;
  unsigned probe_timeout;
  unsigned target_probe_queue_size;
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
  unsigned long long remote_hits;
  unsigned long long fallback_no_candidate;
  unsigned long long fallback_candidates_exhausted;
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
  unsigned long long snapshot_updates;
  unsigned long long snapshot_rebuilds;
  unsigned long long snapshot_rebuild_transport_tags;

  c2p_cache_stats();
  void clear();
};

class c2p_cache {
 public:
  c2p_cache(const c2p_cache_config &config, gpgpu_sim *gpu);

  bool enabled() const { return m_config.enabled; }
  void reset();
  void register_l1(l1_cache *cache);
  bool accept_miss(l1_cache *requester, mem_fetch *mf,
                   unsigned long long now);
  void on_l1_fill(l1_cache *cache, mem_fetch *mf);
  void on_l1_flush(l1_cache *cache);
  void cycle(unsigned long long now);
  void print_stats(FILE *fout) const;
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

  struct transaction {
    transaction(l1_cache *requester, mem_fetch *mf, unsigned requester_sid,
                uint64_t line_tag, unsigned long long now);

    l1_cache *requester;
    mem_fetch *mf;
    unsigned requester_sid;
    uint64_t line_tag;
    unsigned long long enqueue_cycle;
    unsigned long long ready_cycle;
    unsigned long long probe_wait_start;
    transaction_state state;
    std::vector<unsigned> rows;
    std::vector<bool> row_done;
    std::vector<unsigned> candidates;
    unsigned candidate_next;
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
  std::vector<unsigned> m_ccd_counters;
  unsigned long long m_ata_issue_cycle;
  std::vector<unsigned> m_ata_issues;
  unsigned long long m_ring_next_issue_cycle;
  std::vector<unsigned long long> m_peer_access_hit_hist;
  std::vector<unsigned long long> m_peer_access_miss_hist;
  c2p_cache_stats m_stats;
};

#endif
