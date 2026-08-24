#include "c2p-cache.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>

#include "../option_parser.h"
#include "gpu-cache.h"
#include "gpu-sim.h"

namespace {
const unsigned kSnapshotBanks = 64;
const unsigned kTagMaskRowsPerBank = 16;

bool is_power_of_two(unsigned value) {
  return value && !(value & (value - 1));
}

unsigned histogram_percentile(const std::vector<unsigned long long> &hist,
                              unsigned percentile) {
  unsigned long long samples = 0;
  for (unsigned i = 0; i < hist.size(); ++i) samples += hist[i];
  if (!samples) return 0;
  const unsigned long long rank =
      (samples * percentile + 99) / 100;  // nearest-rank percentile
  unsigned long long cumulative = 0;
  for (unsigned i = 0; i < hist.size(); ++i) {
    cumulative += hist[i];
    if (cumulative >= rank) return i;
  }
  assert(0);
  return 0;
}

unsigned histogram_max(const std::vector<unsigned long long> &hist) {
  for (unsigned i = hist.size(); i != 0; --i)
    if (hist[i - 1]) return i - 1;
  return 0;
}

uint32_t fold_hash(uint64_t value, uint32_t salt) {
  uint64_t x = value ^ (uint64_t)salt * 0x9e3779b97f4a7c15ULL;
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return (uint32_t)(x ^ (x >> 32));
}

unsigned probe_policy_ordinal_bin(unsigned ordinal) {
  assert(ordinal != 0);
  return ordinal <= C2P_PROBE_POLICY_MAX_ORDINAL ? ordinal - 1
                                                  : C2P_PROBE_POLICY_MAX_ORDINAL;
}
}  // namespace

c2p_cache_config::c2p_cache_config()
    : enabled(false),
      oracle_only(false),
      ideal_peer_lookup(false),
      collect_oracle(true),
      bf_engines(128),
      bf_latency(2),
      snapshot_bf_rows_per_bank(64),
      bf_hashes(3),
      snapshot_latency(2),
      remote_tag_latency(7),
      remote_return_latency(2),
      query_queue_size(256),
      update_queue_size(1024),
      update_transport_bytes_per_cycle(128),
      snapshot_rebuild_interval(0),
      probe_timeout(32),
      target_probe_queue_size(32),
      max_candidate_probes(0),
      adaptive_probe_policy(false),
      adaptive_probe_score_threshold(4),
      adaptive_probe_explore_period(64),
      adaptive_probe_initial_score(4),
      adaptive_probe_force_full_small_candidates(false),
      adaptive_probe_package_policy(false),
      adaptive_probe_addr_topology_policy(false),
      adaptive_probe_observe_tail(false),
      adaptive_probe_observe_addr_topology(false),
      separate_target_tag_port(false),
      diagnostic_target_port_bypass(false),
      snapshot_copies(4),
      scheme(C2P_SCHEME),
      comparator_cluster_size(8),
      ata_cluster_issue_width(4),
      ata_tag_latency(7),
      ccd_predictor_latency(1),
      ccd_broadcast_latency(3),
      ring_hop_latency(2),
      ring_queue_fallback(false),
      ring_link_pipeline(false),
      ring_request_throttle(false),
      ring_throttle_sample_instructions(1000000),
      ring_throttle_period_instructions(10000000),
      ring_throttle_min_hit_percent(5),
      peer_line_latency(14) {}

void c2p_cache_config::reg_options(OptionParser *opp) {
  option_parser_register(opp, "-c2p_cache_enable", OPT_BOOL, &enabled,
                         "enable C2P private-L1 candidate pruning", "0");
  option_parser_register(
      opp, "-c2p_cache_oracle_only", OPT_BOOL, &oracle_only,
      "collect C2P oracle statistics without changing the baseline path", "0");
  option_parser_register(opp, "-c2p_cache_ideal_peer", OPT_BOOL,
                         &ideal_peer_lookup,
                         "use exact peer discovery instead of Snapshot Matrix",
                         "0");
  option_parser_register(opp, "-c2p_cache_collect_oracle", OPT_BOOL,
                         &collect_oracle,
                         "collect exact redundant-L2 oracle statistics", "1");
  option_parser_register(opp, "-c2p_cache_bf_engines", OPT_UINT32,
                         &bf_engines, "C2P BF/tag-mask engines", "128");
  option_parser_register(opp, "-c2p_cache_bf_latency", OPT_UINT32,
                         &bf_latency, "C2P BF/tag-mask latency", "2");
  option_parser_register(opp, "-c2p_cache_snapshot_bf_rows_per_bank",
                         OPT_UINT32, &snapshot_bf_rows_per_bank,
                         "C2P Bloom-filter rows per Snapshot Matrix bank", "64");
  option_parser_register(opp, "-c2p_cache_bf_hashes", OPT_UINT32, &bf_hashes,
                         "C2P Bloom-filter hashes in addition to tag mask", "3");
  option_parser_register(opp, "-c2p_cache_snapshot_latency", OPT_UINT32,
                         &snapshot_latency, "C2P Snapshot Matrix latency",
                         "2");
  option_parser_register(opp, "-c2p_cache_remote_tag_latency", OPT_UINT32,
                         &remote_tag_latency, "C2P remote L1 tag latency",
                         "7");
  option_parser_register(opp, "-c2p_cache_remote_return_latency", OPT_UINT32,
                         &remote_return_latency,
                         "C2P remote cache-line return latency", "2");
  option_parser_register(opp, "-c2p_cache_query_queue_size", OPT_UINT32,
                         &query_queue_size, "C2P outstanding query limit",
                         "256");
  option_parser_register(opp, "-c2p_cache_update_queue_size", OPT_UINT32,
                         &update_queue_size, "C2P snapshot update queue limit",
                         "1024");
  option_parser_register(opp, "-c2p_cache_update_transport_bytes_per_cycle",
                         OPT_UINT32, &update_transport_bytes_per_cycle,
                         "C2P background update transport bandwidth", "128");
  option_parser_register(opp, "-c2p_cache_snapshot_rebuild_interval",
                         OPT_UINT32, &snapshot_rebuild_interval,
                         "idle cycles between C2P L1 snapshot rebuilds (0=continuous)",
                         "0");
  option_parser_register(opp, "-c2p_cache_probe_timeout", OPT_UINT32,
                         &probe_timeout,
                         "C2P full target-probe queue timeout before L2 fallback", "32");
  option_parser_register(opp, "-c2p_cache_target_probe_queue_size", OPT_UINT32,
                         &target_probe_queue_size,
                         "C2P remote probe FIFO entries per target L1", "32");
  option_parser_register(opp, "-c2p_cache_max_candidate_probes", OPT_UINT32,
                         &max_candidate_probes,
                         "C2P+ failed-candidate probe budget (0=exhaustive)",
                         "0");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_policy", OPT_BOOL,
      &adaptive_probe_policy,
      "C2P+ feature-hash confirmation package policy", "0");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_score_threshold", OPT_UINT32,
      &adaptive_probe_score_threshold,
      "C2P+ 3-bit adaptive utility threshold (0..7)", "4");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_explore_period", OPT_UINT32,
      &adaptive_probe_explore_period,
      "C2P+ forced package period (0 disables exploration)", "64");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_initial_score", OPT_UINT32,
      &adaptive_probe_initial_score,
      "C2P+ confirmation-table reset score (0..7)", "4");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_force_full_small_candidates", OPT_BOOL,
      &adaptive_probe_force_full_small_candidates,
      "C2P+ scan candidate lists of at most four to completion", "0");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_package_policy", OPT_BOOL,
      &adaptive_probe_package_policy,
      "C2P+ use one feature-hash/candidate-bin confirmation package", "0");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_addr_topology_policy", OPT_BOOL,
      &adaptive_probe_addr_topology_policy,
      "C2P+ use address-region x requester-cluster package hash", "0");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_observe_tail", OPT_BOOL,
      &adaptive_probe_observe_tail,
      "observe initial candidate bins and exact later-peer distance", "0");
  option_parser_register(
      opp, "-c2p_cache_adaptive_probe_observe_addr_topology", OPT_BOOL,
      &adaptive_probe_observe_addr_topology,
      "observe line-region and requester-topology confirmation features", "0");
  option_parser_register(
      opp, "-c2p_cache_separate_target_tag_port", OPT_BOOL,
      &separate_target_tag_port,
      "C2P+ model a pipelined remote tag port separate from target data port",
      "0");
  option_parser_register(
      opp, "-c2p_cache_diagnostic_target_port_bypass", OPT_BOOL,
      &diagnostic_target_port_bypass,
      "diagnostic only: remove C2P probe contention for target L1 data port",
      "0");
  option_parser_register(opp, "-c2p_cache_snapshot_copies", OPT_UINT32,
                         &snapshot_copies, "C2P Snapshot Matrix copies", "4");
  option_parser_register(
      opp, "-c2p_cache_scheme", OPT_UINT32, &scheme,
      "sharing scheme: 0=C2P, 1=ATA-like, 2=CCD-like, 3=RING-like", "0");
  option_parser_register(
      opp, "-c2p_cache_comparator_cluster_size", OPT_UINT32,
      &comparator_cluster_size,
      "ATA/CCD logical peer group size, independent of simulator endpoints",
      "8");
  option_parser_register(opp, "-c2p_cache_ata_cluster_issue_width", OPT_UINT32,
                         &ata_cluster_issue_width,
                         "ATA-like aggregate-tag requests accepted per cluster/cycle",
                         "4");
  option_parser_register(opp, "-c2p_cache_ata_tag_latency", OPT_UINT32,
                         &ata_tag_latency,
                         "ATA-like aggregate-tag lookup latency", "7");
  option_parser_register(opp, "-c2p_cache_ccd_predictor_latency", OPT_UINT32,
                         &ccd_predictor_latency,
                         "CCD-like 2-bit predictor lookup latency", "1");
  option_parser_register(opp, "-c2p_cache_ccd_broadcast_latency", OPT_UINT32,
                         &ccd_broadcast_latency,
                         "CCD-like cluster broadcast latency", "3");
  option_parser_register(opp, "-c2p_cache_ring_hop_latency", OPT_UINT32,
                         &ring_hop_latency, "RING-like per-hop latency", "2");
  option_parser_register(
      opp, "-c2p_cache_ring_queue_fallback", OPT_BOOL, &ring_queue_fallback,
      "RING CCN sends new misses to L2 while its request network is full", "0");
  option_parser_register(
      opp, "-c2p_cache_ring_link_pipeline", OPT_BOOL, &ring_link_pipeline,
      "RING-like directed-link pipeline instead of one global injector", "0");
  option_parser_register(
      opp, "-c2p_cache_ring_request_throttle", OPT_BOOL,
      &ring_request_throttle,
      "CCN-RT: bypass RING after a low-hit per-SM sample interval", "0");
  option_parser_register(
      opp, "-c2p_cache_ring_throttle_sample_instructions", OPT_UINT32,
      &ring_throttle_sample_instructions,
      "CCN-RT per-SM sampling interval in committed scalar instructions",
      "1000000");
  option_parser_register(
      opp, "-c2p_cache_ring_throttle_period_instructions", OPT_UINT32,
      &ring_throttle_period_instructions,
      "CCN-RT per-SM sampling epoch in committed scalar instructions",
      "10000000");
  option_parser_register(
      opp, "-c2p_cache_ring_throttle_min_hit_percent", OPT_UINT32,
      &ring_throttle_min_hit_percent,
      "CCN-RT minimum sampled remote-hit percentage", "5");
  option_parser_register(opp, "-c2p_cache_peer_line_latency", OPT_UINT32,
                         &peer_line_latency,
                         "ATA/CCD/RING-like remote cache-line access latency",
                         "14");
}

c2p_cache_stats::c2p_cache_stats() { clear(); }

void c2p_cache_stats::clear() {
  l1_misses = 0;
  oracle_peer_hits = 0;
  queries_accepted = 0;
  queries_queue_bypass = 0;
  updates_queue_bypass = 0;
  candidate_total = 0;
  candidate_queries = 0;
  peer_probes = 0;
  peer_probe_hits = 0;
  peer_probe_misses = 0;
  peer_l1_accesses = 0;
  ring_traversals = 0;
  ring_no_match_traversals = 0;
  ring_traversal_hops = 0;
  ring_network_wait_cycles = 0;
  ring_queue_bypasses = 0;
  ring_throttle_bypasses = 0;
  ring_throttle_samples = 0;
  ring_throttle_sample_requests = 0;
  ring_throttle_sample_hits = 0;
  target_probe_port_busy_cycles = 0;
  target_tag_port_busy_cycles = 0;
  target_probe_queue_wait_cycles = 0;
  target_probe_queue_full_cycles = 0;
  requester_fill_wait_cycles = 0;
  residence_encode_cycles = 0;
  residence_rows_cycles = 0;
  residence_match_cycles = 0;
  residence_ready_cycles = 0;
  residence_target_probe_cycles = 0;
  residence_probe_cycles = 0;
  residence_return_cycles = 0;
  residence_fallback_cycles = 0;
  remote_hit_probe_ordinal_total = 0;
  remote_hit_probe_ordinal_samples = 0;
  fallback_probe_ordinal_total = 0;
  fallback_probe_ordinal_samples = 0;
  for (unsigned ordinal = 0; ordinal < C2P_PROBE_POLICY_ORDINAL_BINS;
       ++ordinal) {
    probe_ordinal_hits[ordinal] = 0;
    probe_ordinal_misses[ordinal] = 0;
    for (unsigned lower_ready = 0; lower_ready != 2; ++lower_ready)
      for (unsigned target_credit = 0; target_credit != 2; ++target_credit)
        continuation_decisions[ordinal][lower_ready][target_credit] = 0;
  }
  for (unsigned bin = 0; bin < C2P_PROBE_POLICY_CANDIDATE_BINS; ++bin)
    for (unsigned ordinal = 0; ordinal < C2P_PROBE_POLICY_ORDINAL_BINS;
         ++ordinal) {
      probe_candidate_bin_ordinal_hits[bin][ordinal] = 0;
      probe_candidate_bin_ordinal_misses[bin][ordinal] = 0;
    }
  for (unsigned bucket = 0; bucket < C2P_PROBE_POLICY_PC_BUCKETS; ++bucket)
    for (unsigned ordinal = 0; ordinal < C2P_PROBE_POLICY_ORDINAL_BINS;
         ++ordinal) {
      probe_pc_ordinal_hits[bucket][ordinal] = 0;
      probe_pc_ordinal_misses[bucket][ordinal] = 0;
    }
  adaptive_continuation_opportunities = 0;
  adaptive_continue_predictor = 0;
  adaptive_continue_exploration = 0;
  adaptive_continue_forced = 0;
  adaptive_stop_predictor = 0;
  adaptive_stop_hard_cap = 0;
  adaptive_stop_later_peer = 0;
  adaptive_stop_no_later_peer = 0;
  adaptive_stop_remaining_candidates = 0;
  adaptive_stop_next_peer_distance_total = 0;
  for (unsigned bin = 0; bin < C2P_PROBE_POLICY_CANDIDATE_BINS; ++bin) {
    adaptive_early_stop_opportunities[bin] = 0;
    adaptive_early_stop_later_peer[bin] = 0;
    adaptive_early_stop_no_later_peer[bin] = 0;
    for (unsigned distance = 0;
         distance < C2P_PROBE_POLICY_DISTANCE_BINS; ++distance)
      adaptive_early_stop_distance[bin][distance] = 0;
  }
  for (unsigned pressure = 0;
       pressure != C2P_PROBE_POLICY_FALLBACK_PRESSURE_BINS; ++pressure) {
    adaptive_early_stop_lower_samples[pressure] = 0;
    adaptive_early_stop_lower_cycles[pressure] = 0;
    adaptive_early_stop_lower_waited[pressure] = 0;
  }
  adaptive_first_probe_hits = 0;
  adaptive_first_probe_misses = 0;
  adaptive_first_probe_timeouts = 0;
  adaptive_predictor_probe_hits = 0;
  adaptive_predictor_probe_misses = 0;
  adaptive_predictor_probe_timeouts = 0;
  adaptive_exploration_probe_hits = 0;
  adaptive_exploration_probe_misses = 0;
  adaptive_exploration_probe_timeouts = 0;
  adaptive_forced_probe_hits = 0;
  adaptive_forced_probe_misses = 0;
  adaptive_forced_probe_timeouts = 0;
  for (unsigned score = 0; score != 8; ++score)
    adaptive_score_hist[score] = 0;
  adaptive_package_opportunities = 0;
  adaptive_package_start_predictor = 0;
  adaptive_package_start_exploration = 0;
  adaptive_package_start_forced = 0;
  adaptive_package_stop_predictor = 0;
  adaptive_package_hit = 0;
  adaptive_package_no_hit = 0;
  adaptive_package_timeout = 0;
  for (unsigned score = 0; score != 8; ++score)
    adaptive_package_score_hist[score] = 0;
  adaptive_package_residual_opportunities = 0;
  adaptive_package_residual_later_peer = 0;
  adaptive_package_residual_no_later_peer = 0;
  adaptive_package_residual_remaining_candidates = 0;
  adaptive_package_residual_next_peer_distance_total = 0;
  for (unsigned bin = 0; bin != C2P_PROBE_POLICY_CANDIDATE_BINS; ++bin) {
    adaptive_tail_observe_opportunities[bin] = 0;
    adaptive_tail_observe_later_peer[bin] = 0;
    adaptive_tail_observe_no_later_peer[bin] = 0;
    for (unsigned distance = 0; distance != C2P_PROBE_POLICY_DISTANCE_BINS;
         ++distance)
      adaptive_tail_observe_distance[bin][distance] = 0;
  }
  fallback_target_wait_timeout = 0;
  fallback_target_admission_timeout = 0;
  peer_lost_before_query = 0;
  peer_gained_before_query = 0;
  remote_hits = 0;
  fallback_no_candidate = 0;
  fallback_candidates_exhausted = 0;
  fallback_candidate_budget = 0;
  fallback_probe_timeout = 0;
  fallback_queue = 0;
  snapshot_false_positive = 0;
  snapshot_false_negative = 0;
  snapshot_true_positive = 0;
  snapshot_true_negative = 0;
  snapshot_query_false_positive = 0;
  snapshot_query_false_negative = 0;
  snapshot_query_true_positive = 0;
  snapshot_query_true_negative = 0;
  ccd_false_positive = 0;
  ccd_false_negative = 0;
  ccd_true_positive = 0;
  ccd_true_negative = 0;
  snapshot_updates = 0;
  snapshot_rebuilds = 0;
  snapshot_rebuild_transport_tags = 0;
}

c2p_cache::transaction::transaction(l1_cache *requester_, mem_fetch *mf_,
                                     unsigned requester_sid_, uint64_t line_tag_,
                                     unsigned long long now)
    : requester(requester_),
      mf(mf_),
      requester_sid(requester_sid_),
      line_tag(line_tag_),
      enqueue_cycle(now),
      state_enter_cycle(now),
      ready_cycle(now),
      probe_wait_start(now),
      state(WAIT_ENCODE),
      candidate_next(0),
      probe_pc_bucket(fold_hash((uint64_t)mf_->get_pc(), 0xC2F0u) &
                      (C2P_PROBE_POLICY_PC_BUCKETS - 1)),
      probe_reason(PROBE_FIRST),
      adaptive_continue_decided(false),
      adaptive_tail_observed(false),
      adaptive_package_active(false),
      adaptive_package_outcome_recorded(false),
      adaptive_early_stop_pending(false),
      adaptive_early_stop_pressure(0),
      adaptive_early_stop_cycle(0),
      probe_sid((unsigned)-1),
      peer_accesses(0),
      oracle_peer_hit(false),
      sharing_attempt(false),
      ring_started(false),
      ring_throttle_sampled(false),
      ring_throttle_epoch(0),
      probe_latency(0),
      return_latency(0) {}

c2p_cache::c2p_cache(const c2p_cache_config &config, gpgpu_sim *gpu)
    : m_config(config),
      m_gpu(gpu),
      m_num_sms(gpu->get_config().num_shader()),
      m_words((m_num_sms + 63) / 64),
      m_l1s(m_num_sms, (l1_cache *)NULL),
      m_snapshot(kSnapshotBanks *
                     (kTagMaskRowsPerBank + config.snapshot_bf_rows_per_bank),
                 std::vector<uint64_t>(m_words, 0)),
      m_rebuild_sid(0),
      m_rebuild_target_sid((unsigned)-1),
      m_rebuild_active(false),
      m_next_rebuild_cycle(0),
      m_rebuild_enqueue_next_tag(0),
      m_rebuild_pending_tags(0),
      m_bank_copy_used(kSnapshotBanks,
                       std::vector<bool>(std::max(1U, config.snapshot_copies),
                                         false)),
      m_target_probe_queues(m_num_sms),
      m_target_tag_next_issue_cycle(m_num_sms, 0),
      m_ccd_counters(
          std::max(1U, (m_num_sms + std::max(1U, config.comparator_cluster_size) - 1) /
                           std::max(1U, config.comparator_cluster_size)),
          2),
      m_ata_issue_cycle(0),
      m_ata_issues(
          std::max(1U, (m_num_sms + std::max(1U, config.comparator_cluster_size) - 1) /
                           std::max(1U, config.comparator_cluster_size)),
          0),
      m_ring_next_issue_cycle(0),
      m_ring_link_next_issue_cycle(m_num_sms, 0),
      m_ring_throttle(m_num_sms),
      m_peer_access_hit_hist(m_num_sms + 1, 0),
      m_peer_access_miss_hist(m_num_sms + 1, 0),
      m_adaptive_package_scores(C2P_PROBE_POLICY_PC_BUCKETS *
                                    C2P_PROBE_POLICY_CANDIDATE_BINS,
                                config.adaptive_probe_initial_score),
      m_adaptive_explore_counter(0),
      m_addr_observe_cluster_count(
          std::max(1U, (m_num_sms + std::max(1U, config.comparator_cluster_size) - 1) /
                           std::max(1U, config.comparator_cluster_size))),
      m_addr_observe_region(C2P_ADDR_OBSERVE_REGION_BUCKETS *
                                C2P_PROBE_POLICY_CANDIDATE_BINS),
      m_addr_observe_cluster(m_addr_observe_cluster_count *
                                 C2P_PROBE_POLICY_CANDIDATE_BINS),
      m_addr_observe_region_cluster(
          C2P_ADDR_OBSERVE_REGION_BUCKETS * m_addr_observe_cluster_count *
          C2P_PROBE_POLICY_CANDIDATE_BINS) {
  assert(m_config.scheme <= c2p_cache_config::RING_SCHEME);
  assert(is_power_of_two(m_config.snapshot_bf_rows_per_bank));
  assert(m_config.bf_hashes > 0);
  assert(m_config.comparator_cluster_size > 0);
  assert(m_config.target_probe_queue_size > 0);
  assert(!m_config.ring_request_throttle ||
         (m_config.ring_throttle_sample_instructions > 0 &&
          m_config.ring_throttle_period_instructions >=
              m_config.ring_throttle_sample_instructions &&
          m_config.ring_throttle_min_hit_percent <= 100));
  assert(m_config.adaptive_probe_score_threshold <= 7);
  assert(m_config.adaptive_probe_initial_score <= 7);
  assert(!m_config.adaptive_probe_package_policy ||
         m_config.adaptive_probe_policy);
  assert(!m_config.adaptive_probe_addr_topology_policy ||
         m_config.adaptive_probe_package_policy);
  assert(!m_config.adaptive_probe_observe_addr_topology ||
         m_config.adaptive_probe_observe_tail);
  assert(!m_config.adaptive_probe_policy ||
         (m_config.scheme == c2p_cache_config::C2P_SCHEME &&
          m_config.separate_target_tag_port &&
          m_config.max_candidate_probes == 0 &&
          m_config.adaptive_probe_package_policy));
  assert(!(m_config.separate_target_tag_port &&
           m_config.diagnostic_target_port_bypass));
}

void c2p_cache::reset() {
  for (unsigned row = 0; row < m_snapshot.size(); ++row)
    std::fill(m_snapshot[row].begin(), m_snapshot[row].end(), 0);
  m_transactions.clear();
  m_update_queue.clear();
  m_update_pipeline.clear();
  for (unsigned sid = 0; sid < m_target_probe_queues.size(); ++sid)
    m_target_probe_queues[sid].clear();
  std::fill(m_target_tag_next_issue_cycle.begin(),
            m_target_tag_next_issue_cycle.end(), 0);
  m_rebuild_sid = 0;
  m_rebuild_target_sid = (unsigned)-1;
  m_rebuild_active = false;
  m_next_rebuild_cycle = 0;
  m_rebuild_tags.clear();
  m_rebuild_enqueue_next_tag = 0;
  m_rebuild_pending_tags = 0;
  std::fill(m_ccd_counters.begin(), m_ccd_counters.end(), 2);
  m_ata_issue_cycle = 0;
  std::fill(m_ata_issues.begin(), m_ata_issues.end(), 0);
  m_ring_next_issue_cycle = 0;
  std::fill(m_ring_link_next_issue_cycle.begin(),
            m_ring_link_next_issue_cycle.end(), 0);
  std::fill(m_ring_throttle.begin(), m_ring_throttle.end(),
            ring_throttle_state());
  std::fill(m_peer_access_hit_hist.begin(), m_peer_access_hit_hist.end(), 0);
  std::fill(m_peer_access_miss_hist.begin(), m_peer_access_miss_hist.end(), 0);
  std::fill(m_adaptive_package_scores.begin(),
            m_adaptive_package_scores.end(),
            m_config.adaptive_probe_initial_score);
  m_adaptive_explore_counter = 0;
  std::fill(m_addr_observe_region.begin(), m_addr_observe_region.end(),
            addr_topology_observation());
  std::fill(m_addr_observe_cluster.begin(), m_addr_observe_cluster.end(),
            addr_topology_observation());
  std::fill(m_addr_observe_region_cluster.begin(),
            m_addr_observe_region_cluster.end(), addr_topology_observation());
  m_stats.clear();
}

void c2p_cache::register_l1(l1_cache *cache) {
  const unsigned sid = cache->c2p_sid();
  assert(sid < m_l1s.size());
  assert(m_l1s[sid] == NULL || m_l1s[sid] == cache);
  m_l1s[sid] = cache;
}

std::vector<unsigned> c2p_cache::query_rows(uint64_t line_tag) const {
  std::vector<unsigned> rows;
  const unsigned bf_rows_per_bank = m_config.snapshot_bf_rows_per_bank;
  const unsigned rows_per_bank = kTagMaskRowsPerBank + bf_rows_per_bank;
  const unsigned bf_rows = kSnapshotBanks * bf_rows_per_bank;
  const unsigned lower10 = (unsigned)(line_tag & 0x3ffU);
  unsigned reversed = 0;
  for (unsigned i = 0; i < 10; ++i)
    reversed |= ((lower10 >> i) & 1U) << (9 - i);
  const unsigned tag_bank = reversed >> 4;
  const unsigned tag_offset = bf_rows_per_bank + (reversed & 0xfU);
  rows.push_back(tag_bank * rows_per_bank + tag_offset);

  const unsigned h1 = fold_hash(line_tag, 0x243f6a88U) & (bf_rows - 1);
  const unsigned h2 = fold_hash(line_tag, 0x85a308d3U) & (bf_rows - 1);
  for (unsigned multiple = 1; multiple <= m_config.bf_hashes; ++multiple) {
    const unsigned index = (multiple * h1 + h2) & (bf_rows - 1);
    const unsigned bank = index / bf_rows_per_bank;
    const unsigned offset = index % bf_rows_per_bank;
    rows.push_back(bank * rows_per_bank + offset);
  }
  return rows;
}

void c2p_cache::set_snapshot_bits(unsigned sid, uint64_t line_tag) {
  assert(sid < m_num_sms);
  const std::vector<unsigned> rows = query_rows(line_tag);
  for (unsigned i = 0; i < rows.size(); ++i)
    m_snapshot[rows[i]][sid / 64] |= 1ULL << (sid % 64);
}

void c2p_cache::clear_snapshot_column(unsigned sid) {
  assert(sid < m_num_sms);
  const uint64_t bit = 1ULL << (sid % 64);
  for (unsigned row = 0; row < m_snapshot.size(); ++row)
    m_snapshot[row][sid / 64] &= ~bit;
}

bool c2p_cache::snapshot_bit(unsigned row, unsigned sid) const {
  return (m_snapshot[row][sid / 64] >> (sid % 64)) & 1ULL;
}

bool c2p_cache::has_exact_peer(l1_cache *requester, mem_fetch *mf) const {
  for (unsigned sid = 0; sid < m_l1s.size(); ++sid) {
    if (m_l1s[sid] != NULL && m_l1s[sid] != requester &&
        m_l1s[sid]->c2p_probe(mf))
      return true;
  }
  return false;
}

unsigned c2p_cache::cluster_size() const {
  return std::min(m_num_sms, m_config.comparator_cluster_size);
}

unsigned c2p_cache::cluster_id(unsigned sid) const { return sid / cluster_size(); }

unsigned c2p_cache::ring_distance(unsigned from_sid, unsigned to_sid) const {
  assert(from_sid < m_num_sms && to_sid < m_num_sms && from_sid != to_sid);
  return (to_sid + m_num_sms - from_sid) % m_num_sms;
}

std::vector<unsigned> c2p_cache::exact_candidates(
    const transaction &txn, bool cluster_only) const {
  std::vector<unsigned> candidates;
  const unsigned requester_cluster = cluster_id(txn.requester_sid);
  for (unsigned sid = 0; sid < m_l1s.size(); ++sid) {
    if (sid == txn.requester_sid || m_l1s[sid] == NULL) continue;
    if (cluster_only && cluster_id(sid) != requester_cluster) continue;
    if (m_l1s[sid]->c2p_probe(txn.mf)) candidates.push_back(sid);
  }
  return candidates;
}

bool c2p_cache::ring_throttle_allows(unsigned sid) {
  if (!m_config.ring_request_throttle) return true;
  assert(sid < m_ring_throttle.size());
  const unsigned long long instructions = m_gpu->shader_core_instructions(sid);
  const unsigned long long epoch =
      instructions / m_config.ring_throttle_period_instructions;
  const unsigned long long offset =
      instructions % m_config.ring_throttle_period_instructions;
  ring_throttle_state &state = m_ring_throttle[sid];

  if (state.epoch != epoch) {
    state = ring_throttle_state();
    state.epoch = epoch;
  }
  if (state.sampling && offset >= m_config.ring_throttle_sample_instructions) {
    // CCN-RT evaluates each source independently after its sampling window.
    // No sample request means no evidence either way, so retain injection
    // rather than turning a silent core into a permanently bypassing core.
    state.sampling = false;
    if (state.sample_requests) {
      ++m_stats.ring_throttle_samples;
      m_stats.ring_throttle_sample_requests += state.sample_requests;
      m_stats.ring_throttle_sample_hits += state.sample_hits;
      state.inject_enabled =
          state.sample_hits * 100 >=
          state.sample_requests * m_config.ring_throttle_min_hit_percent;
    }
  }
  return state.sampling || state.inject_enabled;
}

void c2p_cache::ring_throttle_record_hit(const transaction &txn) {
  if (!m_config.ring_request_throttle || !txn.ring_throttle_sampled) return;
  assert(txn.requester_sid < m_ring_throttle.size());
  ring_throttle_state &state = m_ring_throttle[txn.requester_sid];
  // The paper samples observed hits during tS.  A response that arrives after
  // the sampling boundary must not retroactively alter that epoch's decision.
  if (state.epoch == txn.ring_throttle_epoch && state.sampling)
    ++state.sample_hits;
}

c2p_cache::miss_action c2p_cache::accept_miss(l1_cache *requester,
                                               mem_fetch *mf,
                                               unsigned long long now) {
  if ((!m_config.enabled && !m_config.oracle_only) || mf->get_is_write() ||
      mf->isatomic() ||
      mf->get_access_type() != GLOBAL_ACC_R)
    return MISS_TO_LOWER;

  const bool ring_request =
      m_config.enabled && !m_config.oracle_only &&
      m_config.scheme == c2p_cache_config::RING_SCHEME;
  if (ring_request && !ring_throttle_allows(requester->c2p_sid())) {
    ++m_stats.l1_misses;
    if (m_config.collect_oracle && has_exact_peer(requester, mf))
      ++m_stats.oracle_peer_hits;
    ++m_stats.ring_throttle_bypasses;
    return MISS_TO_LOWER;
  }

  // ``l1_cache::cycle()`` retries an unconsumed miss queue head each cycle.
  // A full RING must therefore stall before collecting miss/oracle counters;
  // otherwise one architectural miss would be counted once per wait cycle.
  const bool ring_full =
      m_config.enabled && !m_config.oracle_only &&
      m_config.scheme == c2p_cache_config::RING_SCHEME &&
      m_transactions.size() >= m_config.query_queue_size;
  if (ring_full && !m_config.ring_queue_fallback)
    return MISS_STALL;

  ++m_stats.l1_misses;
  const bool oracle = m_config.collect_oracle && has_exact_peer(requester, mf);
  if (oracle) ++m_stats.oracle_peer_hits;
  if (ring_full) {
    // CCN uses finite per-node request queues and buffers.  When they are
    // congested, new local misses bypass CCN to L2 until capacity returns;
    // accepted ring requests still retain their full traversal behavior.
    ++m_stats.ring_queue_bypasses;
    return MISS_TO_LOWER;
  }
  if (m_config.oracle_only || !m_config.enabled) return MISS_TO_LOWER;
  if (m_transactions.size() >= m_config.query_queue_size) {
    // C2P explicitly retains the baseline escape path under query pressure.
    ++m_stats.queries_queue_bypass;
    return MISS_TO_LOWER;
  }
  if (m_config.scheme == c2p_cache_config::ATA_SCHEME) {
    if (m_ata_issue_cycle != now) {
      m_ata_issue_cycle = now;
      std::fill(m_ata_issues.begin(), m_ata_issues.end(), 0);
    }
    const unsigned cluster = cluster_id(requester->c2p_sid());
    if (m_ata_issues[cluster] >= m_config.ata_cluster_issue_width)
      return MISS_TO_LOWER;
    ++m_ata_issues[cluster];
  }
  for (std::list<transaction>::const_iterator it = m_transactions.begin();
       it != m_transactions.end(); ++it)
    assert(it->mf != mf);

  transaction txn(requester, mf, requester->c2p_sid(),
                  requester->c2p_line_tag(mf->get_addr()), now);
  txn.oracle_peer_hit = oracle;
  txn.probe_latency = m_config.remote_tag_latency;
  txn.return_latency = m_config.remote_return_latency;
  if (m_config.scheme == c2p_cache_config::C2P_SCHEME) {
    txn.rows = query_rows(txn.line_tag);
    txn.row_done.assign(txn.rows.size(), false);
  } else {
    // The prior mechanisms read copied/aggregate tags when the request is
    // accepted, then expose that result after their modeled lookup delay.
    // Keep this tag-time candidate snapshot separate from c2p_probe() below:
    // a selected peer must still hold the data when its data-array access
    // actually occurs.
    txn.candidates = exact_candidates(
        txn, m_config.scheme != c2p_cache_config::RING_SCHEME);
    txn.state = WAIT_MATCH;
    if (m_config.scheme == c2p_cache_config::ATA_SCHEME) {
      txn.ready_cycle = now + m_config.ata_tag_latency;
      txn.sharing_attempt = true;
    } else if (m_config.scheme == c2p_cache_config::CCD_SCHEME) {
      const unsigned cluster = cluster_id(txn.requester_sid);
      txn.sharing_attempt = m_ccd_counters[cluster] >= 2;
      const bool cluster_peer_hit = !txn.candidates.empty();
      if (txn.sharing_attempt && cluster_peer_hit)
        ++m_stats.ccd_true_positive;
      else if (txn.sharing_attempt)
        ++m_stats.ccd_false_positive;
      else if (cluster_peer_hit)
        ++m_stats.ccd_false_negative;
      else
        ++m_stats.ccd_true_negative;
      txn.ready_cycle = now + m_config.ccd_predictor_latency +
                        (txn.sharing_attempt
                             ? m_config.ccd_broadcast_latency +
                                   m_config.remote_tag_latency
                             : 0);
    } else {
      assert(m_config.scheme == c2p_cache_config::RING_SCHEME);
      txn.ready_cycle = now;
      txn.sharing_attempt = true;
    }
    txn.probe_latency = m_config.peer_line_latency;
    txn.return_latency = 0;
  }
  if (ring_request && m_config.ring_request_throttle) {
    ring_throttle_state &state = m_ring_throttle[txn.requester_sid];
    txn.ring_throttle_sampled = state.sampling;
    txn.ring_throttle_epoch = state.epoch;
    if (txn.ring_throttle_sampled) ++state.sample_requests;
  }
  m_transactions.push_back(txn);
  ++m_stats.queries_accepted;
  return MISS_ACCEPTED;
}

void c2p_cache::on_l1_fill(l1_cache *cache, mem_fetch *mf) {
  if (!m_config.enabled || m_config.scheme != c2p_cache_config::C2P_SCHEME ||
      mf->get_is_write())
    return;
  if (m_update_queue.size() >= m_config.update_queue_size) {
    // This is safe for correctness: the exact peer probe remains authoritative
    // and the next rebuild repairs the missing metadata.  Keep it explicit so
    // an experiment cannot mistake update backpressure for Bloom-filter loss.
    ++m_stats.updates_queue_bypass;
    return;
  }
  m_update_queue.push_back(update_entry(
      cache->c2p_sid(), cache->c2p_line_tag(mf->get_addr()), false));
}

void c2p_cache::on_l1_flush(l1_cache *cache) {
  if (!m_config.enabled || m_config.scheme != c2p_cache_config::C2P_SCHEME)
    return;
  const unsigned sid = cache->c2p_sid();
  clear_snapshot_column(sid);
  // Fills queued before the flush no longer describe resident cache lines.
  for (std::deque<update_entry>::iterator it = m_update_queue.begin();
       it != m_update_queue.end();) {
    if (it->sid == sid)
      it = m_update_queue.erase(it);
    else
      ++it;
  }
  // A BF engine may already be encoding a fill/update for this column.  A
  // flush invalidates that work too; otherwise a delayed completion would
  // reintroduce stale snapshot bits after the column has been cleared.
  for (std::deque<pending_update>::iterator it = m_update_pipeline.begin();
       it != m_update_pipeline.end();) {
    if (it->entry.sid == sid) {
      if (it->entry.rebuild) {
        assert(m_rebuild_pending_tags > 0);
        --m_rebuild_pending_tags;
      }
      it = m_update_pipeline.erase(it);
    } else {
      ++it;
    }
  }
  if (m_rebuild_active && m_rebuild_target_sid == sid) {
    m_rebuild_tags.clear();
    m_rebuild_enqueue_next_tag = 0;
    m_rebuild_pending_tags = 0;
    m_rebuild_active = false;
  }
}

void c2p_cache::begin_next_rebuild() {
  if (m_l1s.empty()) return;
  for (unsigned tries = 0; tries < m_l1s.size(); ++tries) {
    const unsigned sid = (m_rebuild_sid + tries) % m_l1s.size();
    if (m_l1s[sid] == NULL) continue;
    m_rebuild_target_sid = sid;
    m_rebuild_sid = (sid + 1) % m_l1s.size();
    clear_snapshot_column(sid);
    m_l1s[sid]->c2p_valid_line_tags(m_rebuild_tags);
    m_rebuild_enqueue_next_tag = 0;
    m_rebuild_pending_tags = 0;
    m_rebuild_active = true;
    ++m_stats.snapshot_rebuilds;
    return;
  }
}

void c2p_cache::issue_update(unsigned long long now, unsigned &engines_left) {
  // BF/tag-mask encoding is a two-cycle shared engine operation for both
  // miss-side queries and background updates.  Commit updates only after
  // that latency; querying keeps priority because issue_query_encodes() runs
  // first and passes the remaining engine budget here.
  while (!m_update_pipeline.empty() &&
         m_update_pipeline.front().ready_cycle <= now) {
    const update_entry entry = m_update_pipeline.front().entry;
    m_update_pipeline.pop_front();
    set_snapshot_bits(entry.sid, entry.line_tag);
    ++m_stats.snapshot_updates;
    if (entry.rebuild) {
      assert(m_rebuild_pending_tags > 0);
      --m_rebuild_pending_tags;
    }
  }
  if (!m_rebuild_active && now >= m_next_rebuild_cycle) {
    begin_next_rebuild();
  }
  // A periodic rebuild first transports tags from the selected L1 into the
  // shared update queue.  At the paper's 128 B/cycle bandwidth this is 16
  // compact 64-bit tags/cycle.  Unlike normal fills, rebuild traffic waits
  // for queue space rather than being dropped, because it defines a complete
  // replacement Snapshot column.
  unsigned transport_tags =
      std::max(1U, m_config.update_transport_bytes_per_cycle /
                        (unsigned)sizeof(uint64_t));
  while (m_rebuild_active && transport_tags &&
         m_rebuild_enqueue_next_tag < m_rebuild_tags.size() &&
         m_update_queue.size() < m_config.update_queue_size) {
    m_update_queue.push_back(update_entry(
        m_rebuild_target_sid, m_rebuild_tags[m_rebuild_enqueue_next_tag++],
        true));
    ++m_rebuild_pending_tags;
    ++m_stats.snapshot_rebuild_transport_tags;
    --transport_tags;
  }
  while (engines_left && !m_update_queue.empty()) {
    const update_entry entry = m_update_queue.front();
    m_update_queue.pop_front();
    m_update_pipeline.push_back(
        pending_update(entry, now + m_config.bf_latency));
    --engines_left;
  }
  if (m_rebuild_active &&
      m_rebuild_enqueue_next_tag >= m_rebuild_tags.size() &&
      m_rebuild_pending_tags == 0) {
    m_rebuild_active = false;
    m_next_rebuild_cycle = now + m_config.snapshot_rebuild_interval;
  }
}

void c2p_cache::issue_query_encodes(unsigned long long now,
                                    unsigned &engines_left) {
  for (std::list<transaction>::iterator it = m_transactions.begin();
       it != m_transactions.end() && engines_left; ++it) {
    if (it->state != WAIT_ENCODE) continue;
    transition(*it, WAIT_ROWS, now);
    it->ready_cycle = now + m_config.bf_latency;
    --engines_left;
  }
}

void c2p_cache::schedule_rows(unsigned long long now) {
  for (unsigned bank = 0; bank < m_bank_copy_used.size(); ++bank)
    std::fill(m_bank_copy_used[bank].begin(), m_bank_copy_used[bank].end(),
              false);
  for (std::list<transaction>::iterator it = m_transactions.begin();
       it != m_transactions.end(); ++it) {
    if (it->state != WAIT_ROWS || now < it->ready_cycle) continue;
    for (unsigned row_i = 0; row_i < it->rows.size(); ++row_i) {
      if (it->row_done[row_i]) continue;
      const unsigned bank = it->rows[row_i] /
                            (kTagMaskRowsPerBank +
                             m_config.snapshot_bf_rows_per_bank);
      assert(bank < m_bank_copy_used.size());
      for (unsigned copy = 0; copy < m_bank_copy_used[bank].size(); ++copy) {
        if (!m_bank_copy_used[bank][copy]) {
          m_bank_copy_used[bank][copy] = true;
          it->row_done[row_i] = true;
          break;
        }
      }
    }
    bool all_rows = true;
    for (unsigned row_i = 0; row_i < it->row_done.size(); ++row_i)
      all_rows &= it->row_done[row_i];
    if (all_rows) {
      transition(*it, WAIT_MATCH, now);
      it->ready_cycle = now + m_config.snapshot_latency;
    }
  }
}

unsigned c2p_cache::cluster_distance(unsigned from_sid, unsigned to_sid) const {
  // The simulation may expose each SM as its own endpoint to avoid modeling
  // artifacts in a shared simt_core_cluster.  Candidate locality must still
  // follow the evaluated GPU's logical SM organization, not that endpoint
  // decomposition.  comparator_cluster_size is the paper's 8-SM group for
  // both ATA/CCD and C2P candidate ordering.
  const unsigned from_cluster = cluster_id(from_sid);
  const unsigned to_cluster = cluster_id(to_sid);
  if (from_cluster == to_cluster) return 0;
  return 1 + (from_cluster > to_cluster ? from_cluster - to_cluster
                                        : to_cluster - from_cluster);
}

std::vector<unsigned> c2p_cache::ordered_candidates(
    const transaction &txn) const {
  std::vector<unsigned> candidates;
  if (m_config.ideal_peer_lookup) {
    for (unsigned sid = 0; sid < m_l1s.size(); ++sid)
      if (sid != txn.requester_sid && m_l1s[sid] != NULL &&
          m_l1s[sid]->c2p_probe(txn.mf))
        candidates.push_back(sid);
  } else {
    for (unsigned sid = 0; sid < m_num_sms; ++sid) {
      if (sid == txn.requester_sid || m_l1s[sid] == NULL) continue;
      bool candidate = true;
      for (unsigned row_i = 0; row_i < txn.rows.size(); ++row_i)
        candidate &= snapshot_bit(txn.rows[row_i], sid);
      if (candidate) candidates.push_back(sid);
    }
  }
  std::stable_sort(candidates.begin(), candidates.end(),
                   [this, &txn](unsigned a, unsigned b) {
                     const unsigned da = cluster_distance(txn.requester_sid, a);
                     const unsigned db = cluster_distance(txn.requester_sid, b);
                     return da == db ? a < b : da < db;
                   });
  return candidates;
}

void c2p_cache::complete_matches(unsigned long long now) {
  for (std::list<transaction>::iterator it = m_transactions.begin();
       it != m_transactions.end(); ++it) {
    if (it->state != WAIT_MATCH || now < it->ready_cycle) continue;
    if (m_config.scheme == c2p_cache_config::C2P_SCHEME) {
      it->candidates = ordered_candidates(*it);
      if (!m_config.ideal_peer_lookup) {
        // The paper's system-level TP/TN/FP/FN classifies candidate
        // generation against peer residency when the L1 miss is accepted.
        // Keep that primary, paper-comparable classification.  Also record
        // a second query-time truth table below: peer fills and evictions
        // while a request waits in C2P are a timing diagnostic, not Bloom
        // metadata inaccuracy.
        if (!it->candidates.empty() && it->oracle_peer_hit)
          ++m_stats.snapshot_true_positive;
        else if (!it->candidates.empty())
          ++m_stats.snapshot_false_positive;
        else if (it->oracle_peer_hit)
          ++m_stats.snapshot_false_negative;
        else
          ++m_stats.snapshot_true_negative;
        const bool query_peer_hit = has_exact_peer(it->requester, it->mf);
        if (it->oracle_peer_hit && !query_peer_hit)
          ++m_stats.peer_lost_before_query;
        else if (!it->oracle_peer_hit && query_peer_hit)
          ++m_stats.peer_gained_before_query;
        if (!it->candidates.empty() && query_peer_hit)
          ++m_stats.snapshot_query_true_positive;
        else if (!it->candidates.empty())
          ++m_stats.snapshot_query_false_positive;
        else if (query_peer_hit)
          ++m_stats.snapshot_query_false_negative;
        else
          ++m_stats.snapshot_query_true_negative;
      }
    } else if (m_config.scheme == c2p_cache_config::ATA_SCHEME) {
      // ATA has an exact aggregate tag array within one cluster.  The lookup
      // itself represents one tag access per L1 in that cluster.
      m_stats.peer_l1_accesses += cluster_size();
    } else if (m_config.scheme == c2p_cache_config::CCD_SCHEME) {
      const unsigned cluster = cluster_id(it->requester_sid);
      // A two-bit predictor must learn both taken and not-taken outcomes.
      // `candidates` is the exact tag-time in-cluster outcome retained for
      // CCD's TP/FN/FP/TN accounting.  Updating only after a taken broadcast
      // makes a counter that first sees a miss fall permanently below the
      // taken threshold, so later false negatives can never retrain it.
      // Update from this observed tag-time outcome before clearing a
      // not-taken transaction's probe list.
      if (it->candidates.empty()) {
        if (m_ccd_counters[cluster]) --m_ccd_counters[cluster];
      } else if (m_ccd_counters[cluster] < 3) {
        ++m_ccd_counters[cluster];
      }
      if (it->sharing_attempt) {
        // CCD broadcasts to all cluster peers when its two-bit predictor is
        // taken; exact tags then select the peer that returns the line.
        m_stats.peer_l1_accesses += cluster_size();
      } else {
        it->candidates.clear();
      }
    } else {
      assert(m_config.scheme == c2p_cache_config::RING_SCHEME);
      if (!it->ring_started) {
        const unsigned requester_sid = it->requester_sid;
        std::stable_sort(it->candidates.begin(), it->candidates.end(),
                         [this, requester_sid](unsigned a, unsigned b) {
                           return ring_distance(requester_sid, a) <
                                  ring_distance(requester_sid, b);
                         });
        const bool no_copied_tag_match = it->candidates.empty();
        const unsigned hops =
            no_copied_tag_match
                ? m_num_sms - 1
                : ring_distance(it->requester_sid, it->candidates.front());
        unsigned long long arrival = now;
        if (m_config.ring_link_pipeline) {
          // A ring is serial along a directed link, not at a chip-global
          // injection point.  Reserve exactly the links from requester to
          // the first copied-tag match (or all other SMs on a no-match), so
          // unrelated segments stay pipelined while true link conflicts queue.
          for (unsigned hop = 0; hop < hops; ++hop) {
            const unsigned link = (it->requester_sid + hop) % m_num_sms;
            const unsigned long long start = std::max(
                arrival, m_ring_link_next_issue_cycle[link]);
            m_stats.ring_network_wait_cycles += start - arrival;
            arrival = start + m_config.ring_hop_latency;
            m_ring_link_next_issue_cycle[link] = arrival;
          }
        } else {
          const unsigned long long start =
              std::max(now, m_ring_next_issue_cycle);
          // Preserve the pre-pipeline point as an explicit A/B comparator.
          m_stats.ring_network_wait_cycles += start - now;
          m_ring_next_issue_cycle = start + m_config.ring_hop_latency;
          arrival = start +
                    (unsigned long long)hops * m_config.ring_hop_latency;
        }
        ++m_stats.ring_traversals;
        if (no_copied_tag_match) ++m_stats.ring_no_match_traversals;
        m_stats.ring_traversal_hops += hops;
        it->ready_cycle = arrival + m_config.remote_tag_latency;
        it->ring_started = true;
        if (now < it->ready_cycle) continue;
      }
    }
    m_stats.candidate_total += it->candidates.size();
    ++m_stats.candidate_queries;
    transition(*it, READY_TO_PROBE, now);
    it->probe_wait_start = now;
  }
}

void c2p_cache::transition(transaction &txn, transaction_state state,
                           unsigned long long now) {
  assert(now >= txn.state_enter_cycle);
  const unsigned long long residence = now - txn.state_enter_cycle;
  switch (txn.state) {
    case WAIT_ENCODE:
      m_stats.residence_encode_cycles += residence;
      break;
    case WAIT_ROWS:
      m_stats.residence_rows_cycles += residence;
      break;
    case WAIT_MATCH:
      m_stats.residence_match_cycles += residence;
      break;
    case READY_TO_PROBE:
      m_stats.residence_ready_cycles += residence;
      break;
    case WAIT_TARGET_PROBE:
      m_stats.residence_target_probe_cycles += residence;
      break;
    case WAIT_PROBE:
      m_stats.residence_probe_cycles += residence;
      break;
    case WAIT_RETURN:
      m_stats.residence_return_cycles += residence;
      break;
    case WAIT_FALLBACK:
      m_stats.residence_fallback_cycles += residence;
      break;
  }
  txn.state = state;
  txn.state_enter_cycle = now;
}

void c2p_cache::retire(transaction &txn, unsigned long long now) {
  // Account for the final residence without introducing an artificial state.
  transition(txn, txn.state, now);
}

void c2p_cache::begin_fallback(transaction &txn, unsigned long long now) {
  m_stats.fallback_probe_ordinal_total += txn.candidate_next;
  ++m_stats.fallback_probe_ordinal_samples;
  transition(txn, WAIT_FALLBACK, now);
}

bool c2p_cache::adaptive_should_start_package(transaction &txn) {
  // The only adaptive decision happens immediately after the mandatory first
  // probe misses. The selected request then keeps confirming through ordinal
  // four. Every candidate-count bin uses this same 64 x 4 table.
  assert(m_config.adaptive_probe_package_policy);
  assert(txn.candidate_next == 1);
  ++m_stats.adaptive_continuation_opportunities;
  ++m_stats.adaptive_package_opportunities;

  if (m_config.adaptive_probe_force_full_small_candidates &&
      txn.candidates.size() <= C2P_PROBE_POLICY_MAX_ORDINAL) {
    txn.probe_reason = PROBE_FORCED;
    ++m_stats.adaptive_continue_forced;
    ++m_stats.adaptive_package_start_forced;
    return true;
  }

  const unsigned score =
      m_adaptive_package_scores[adaptive_package_score_index(txn)];
  ++m_stats.adaptive_score_hist[score];
  ++m_stats.adaptive_package_score_hist[score];
  const bool explore_due = m_config.adaptive_probe_explore_period != 0 &&
      (m_adaptive_explore_counter++ %
           m_config.adaptive_probe_explore_period ==
       0);
  if (score >= m_config.adaptive_probe_score_threshold) {
    txn.probe_reason = PROBE_PREDICTOR;
    ++m_stats.adaptive_continue_predictor;
    ++m_stats.adaptive_package_start_predictor;
    return true;
  }
  if (explore_due) {
    txn.probe_reason = PROBE_EXPLORATION;
    ++m_stats.adaptive_continue_exploration;
    ++m_stats.adaptive_package_start_exploration;
    return true;
  }
  ++m_stats.adaptive_stop_predictor;
  ++m_stats.adaptive_package_stop_predictor;
  return false;
}

unsigned c2p_cache::adaptive_candidate_bin(const transaction &txn) const {
  const unsigned count = txn.candidates.size();
  assert(count != 0);
  if (count <= 2) return 0;
  if (count <= 4) return 1;
  if (count <= 8) return 2;
  return 3;
}

unsigned c2p_cache::adaptive_package_score_index(
    const transaction &txn) const {
  const unsigned feature_bucket = m_config.adaptive_probe_addr_topology_policy
                                      ? adaptive_addr_topology_bucket(txn)
                                      : txn.probe_pc_bucket;
  return feature_bucket * C2P_PROBE_POLICY_CANDIDATE_BINS +
         adaptive_candidate_bin(txn);
}

unsigned c2p_cache::adaptive_addr_topology_bucket(
    const transaction &txn) const {
  // Reduce the 32 address-region x requester-cluster feature space to the
  // same 64 entries used by the PC package table.  The comparison therefore
  // changes predictor information only; capacity, counter width, threshold,
  // exploration, and the four-probe cap remain identical.
  const unsigned region =
      fold_hash(txn.line_tag, 0xA771u) & (C2P_ADDR_OBSERVE_REGION_BUCKETS - 1);
  const unsigned requester_cluster = cluster_id(txn.requester_sid);
  const uint64_t feature = ((uint64_t)region << 32) | requester_cluster;
  return fold_hash(feature, 0xA772u) & (C2P_PROBE_POLICY_PC_BUCKETS - 1);
}

void c2p_cache::adaptive_record_probe_result(transaction &txn, bool hit) {
  // A completed probe consumes the saved issue decision. A selected package
  // reinstates that decision without rereading its score; a FIFO stall must
  // not create another policy decision.
  txn.adaptive_continue_decided = false;
  txn.adaptive_tail_observed = false;
  if (!m_config.adaptive_probe_policy) return;
  if (txn.probe_reason == PROBE_FIRST) {
    if (hit)
      ++m_stats.adaptive_first_probe_hits;
    else
      ++m_stats.adaptive_first_probe_misses;
  } else if (txn.probe_reason == PROBE_PREDICTOR) {
    if (hit)
      ++m_stats.adaptive_predictor_probe_hits;
    else
      ++m_stats.adaptive_predictor_probe_misses;
  } else if (txn.probe_reason == PROBE_EXPLORATION) {
    if (hit)
      ++m_stats.adaptive_exploration_probe_hits;
    else
      ++m_stats.adaptive_exploration_probe_misses;
  } else {
    assert(txn.probe_reason == PROBE_FORCED);
    if (hit)
      ++m_stats.adaptive_forced_probe_hits;
    else
      ++m_stats.adaptive_forced_probe_misses;
  }
  if (hit) adaptive_record_package_outcome(txn, PACKAGE_HIT);
}

void c2p_cache::adaptive_observe_tail(transaction &txn) {
  if (!m_config.adaptive_probe_observe_tail) return;
  assert(txn.candidate_next != 0);
  assert(txn.candidate_next < txn.candidates.size());
  const unsigned candidate_bin = adaptive_candidate_bin(txn);
  ++m_stats.adaptive_tail_observe_opportunities[candidate_bin];

  // A read-only exact scan records the counterfactual result of stopping at
  // this decision point. It is never used by the adaptive policy itself.
  bool later_peer = false;
  unsigned distance = 0;
  for (unsigned index = txn.candidate_next; index < txn.candidates.size();
       ++index) {
    if (m_l1s[txn.candidates[index]]->c2p_probe(txn.mf)) {
      later_peer = true;
      distance = index - txn.candidate_next + 1;
      ++m_stats.adaptive_tail_observe_later_peer[candidate_bin];
      const unsigned distance_bin =
          distance <= C2P_PROBE_POLICY_MAX_ORDINAL
              ? distance - 1
              : C2P_PROBE_POLICY_MAX_ORDINAL;
      ++m_stats.adaptive_tail_observe_distance[candidate_bin][distance_bin];
      break;
    }
  }
  if (!later_peer)
    ++m_stats.adaptive_tail_observe_no_later_peer[candidate_bin];

  // Package selection is made only after the mandatory first probe misses.
  // Keep this feature study at that same single decision point per request;
  // later observations belong to an already selected confirmation sequence.
  if (m_config.adaptive_probe_observe_addr_topology && txn.candidate_next == 1) {
    const unsigned target_sid = txn.candidates[txn.candidate_next];
    const bool lower_ready = txn.requester->c2p_lower_ready(txn.mf);
    const bool target_credit =
        m_target_probe_queues[target_sid].size() < m_config.target_probe_queue_size;
    adaptive_observe_addr_topology(txn, later_peer, distance, lower_ready,
                                   target_credit);
  }
}

void c2p_cache::adaptive_observe_addr_topology(
    const transaction &txn, bool later_peer, unsigned distance,
    bool lower_ready, bool target_credit) {
  const unsigned candidate_bin = adaptive_candidate_bin(txn);
  const unsigned region =
      fold_hash(txn.line_tag, 0xA771u) & (C2P_ADDR_OBSERVE_REGION_BUCKETS - 1);
  const unsigned cluster = cluster_id(txn.requester_sid);
  assert(cluster < m_addr_observe_cluster_count);

  addr_topology_observation *entries[] = {
      &m_addr_observe_region[region * C2P_PROBE_POLICY_CANDIDATE_BINS +
                             candidate_bin],
      &m_addr_observe_cluster[cluster * C2P_PROBE_POLICY_CANDIDATE_BINS +
                              candidate_bin],
      &m_addr_observe_region_cluster[
          (region * m_addr_observe_cluster_count + cluster) *
              C2P_PROBE_POLICY_CANDIDATE_BINS +
          candidate_bin]};
  for (unsigned entry_i = 0; entry_i != sizeof(entries) / sizeof(entries[0]);
       ++entry_i) {
    addr_topology_observation &entry = *entries[entry_i];
    ++entry.opportunities;
    if (later_peer) ++entry.later_peer;
    if (later_peer && distance <= C2P_PROBE_POLICY_MAX_ORDINAL)
      ++entry.within_4;
    if (lower_ready) ++entry.lower_ready;
    if (target_credit) ++entry.target_credit;
  }
}

void c2p_cache::adaptive_record_probe_timeout(transaction &txn) {
  if (!m_config.adaptive_probe_policy) return;
  // peer_probes is charged when a C2P target FIFO accepts the candidate.
  // A queue-timeout is therefore a third terminal outcome of that issued
  // probe, distinct from both a tag hit and a tag miss.
  if (txn.probe_reason == PROBE_FIRST)
    ++m_stats.adaptive_first_probe_timeouts;
  else if (txn.probe_reason == PROBE_PREDICTOR)
    ++m_stats.adaptive_predictor_probe_timeouts;
  else if (txn.probe_reason == PROBE_EXPLORATION)
    ++m_stats.adaptive_exploration_probe_timeouts;
  else {
    assert(txn.probe_reason == PROBE_FORCED);
    ++m_stats.adaptive_forced_probe_timeouts;
  }
  adaptive_record_package_outcome(txn, PACKAGE_TIMEOUT);
}

void c2p_cache::adaptive_record_package_outcome(transaction &txn,
                                                 package_outcome outcome) {
  if (!m_config.adaptive_probe_package_policy ||
      !txn.adaptive_package_active || txn.adaptive_package_outcome_recorded)
    return;
  txn.adaptive_package_outcome_recorded = true;
  txn.adaptive_package_active = false;
  unsigned char &score =
      m_adaptive_package_scores[adaptive_package_score_index(txn)];
  if (outcome == PACKAGE_HIT) {
    ++m_stats.adaptive_package_hit;
    score = std::min(7U, (unsigned)score + 2);
  } else if (outcome == PACKAGE_NO_HIT) {
    ++m_stats.adaptive_package_no_hit;
    if (score) --score;
  } else {
    assert(outcome == PACKAGE_TIMEOUT);
    ++m_stats.adaptive_package_timeout;
  }
}

void c2p_cache::adaptive_record_package_residual(const transaction &txn) {
  if (!m_config.adaptive_probe_package_policy ||
      !txn.adaptive_package_active ||
      txn.candidate_next >= txn.candidates.size())
    return;
  ++m_stats.adaptive_package_residual_opportunities;
  m_stats.adaptive_package_residual_remaining_candidates +=
      txn.candidates.size() - txn.candidate_next;
  for (unsigned index = txn.candidate_next; index < txn.candidates.size();
       ++index) {
    if (m_l1s[txn.candidates[index]]->c2p_probe(txn.mf)) {
      ++m_stats.adaptive_package_residual_later_peer;
      m_stats.adaptive_package_residual_next_peer_distance_total +=
          index - txn.candidate_next + 1;
      return;
    }
  }
  ++m_stats.adaptive_package_residual_no_later_peer;
}

void c2p_cache::adaptive_record_stop(transaction &txn, bool hard_cap,
                                     unsigned long long now) {
  assert(m_config.adaptive_probe_policy);
  assert(txn.candidate_next < txn.candidates.size());
  if (hard_cap)
    ++m_stats.adaptive_stop_hard_cap;
  const unsigned candidate_bin = adaptive_candidate_bin(txn);
  if (!hard_cap) {
    ++m_stats.adaptive_early_stop_opportunities[candidate_bin];
    txn.adaptive_early_stop_pending = true;
    unsigned fallback_waiters = 0;
    for (std::list<transaction>::const_iterator it = m_transactions.begin();
         it != m_transactions.end(); ++it)
      if (&*it != &txn && it->state == WAIT_FALLBACK) ++fallback_waiters;
    txn.adaptive_early_stop_pressure =
        fallback_waiters == 0 ? 0 : fallback_waiters == 1 ? 1
                               : fallback_waiters <= 3 ? 2 : 3;
    txn.adaptive_early_stop_cycle = now;
  }
  m_stats.adaptive_stop_remaining_candidates +=
      txn.candidates.size() - txn.candidate_next;

  // This exact scan is diagnostic-only and does not reserve a tag/data port,
  // alter state, or delay the lower fallback.  It tells us whether stopping
  // saved an FP tail or discarded an exact peer still resident at stop time.
  for (unsigned index = txn.candidate_next; index < txn.candidates.size();
       ++index) {
    const unsigned sid = txn.candidates[index];
    if (m_l1s[sid]->c2p_probe(txn.mf)) {
      ++m_stats.adaptive_stop_later_peer;
      m_stats.adaptive_stop_next_peer_distance_total +=
          index - txn.candidate_next + 1;
      if (!hard_cap) {
        ++m_stats.adaptive_early_stop_later_peer[candidate_bin];
        const unsigned distance = index - txn.candidate_next + 1;
        const unsigned distance_bin =
            distance <= C2P_PROBE_POLICY_MAX_ORDINAL
                ? distance - 1
                : C2P_PROBE_POLICY_MAX_ORDINAL;
        ++m_stats.adaptive_early_stop_distance[candidate_bin][distance_bin];
      }
      return;
    }
  }
  ++m_stats.adaptive_stop_no_later_peer;
  if (!hard_cap)
    ++m_stats.adaptive_early_stop_no_later_peer[candidate_bin];
}

void c2p_cache::service_target_probe_queues(unsigned long long now) {
  for (unsigned sid = 0; sid < m_target_probe_queues.size(); ++sid) {
    std::deque<transaction *> &queue = m_target_probe_queues[sid];
    if (queue.empty()) continue;
    const bool separate_tag_port =
        m_config.scheme == c2p_cache_config::C2P_SCHEME &&
        m_config.separate_target_tag_port;
    if (separate_tag_port && now < m_target_tag_next_issue_cycle[sid]) {
      ++m_stats.target_tag_port_busy_cycles;
      continue;
    }
    if (!separate_tag_port && !m_config.diagnostic_target_port_bypass &&
        !m_l1s[sid]->data_port_free()) {
      ++m_stats.target_probe_port_busy_cycles;
      continue;
    }

    // The normal model admits one target-array access when its shared data
    // port is free.  C2P+ can instead issue one pipelined remote-tag lookup
    // per target per cycle without reserving that data port.  The diagnostic
    // control remains the unlimited upper bound, not this implementation.
    do {
      transaction *txn = queue.front();
      queue.pop_front();
      assert(txn->state == WAIT_TARGET_PROBE);
      m_stats.target_probe_queue_wait_cycles += now - txn->probe_wait_start;
      if (!separate_tag_port && !m_config.diagnostic_target_port_bypass)
        m_l1s[sid]->c2p_reserve_probe_port(txn->probe_latency);
      txn->probe_sid = sid;
      transition(*txn, WAIT_PROBE, now);
      txn->ready_cycle = now + txn->probe_latency;
      if (separate_tag_port)
        m_target_tag_next_issue_cycle[sid] = now + 1;
    } while (m_config.diagnostic_target_port_bypass && !queue.empty());
  }
}

void c2p_cache::advance_probes(unsigned long long now) {
  service_target_probe_queues(now);
  for (std::list<transaction>::iterator it = m_transactions.begin();
       it != m_transactions.end();) {
    bool erase = false;
    if (it->state == WAIT_PROBE && now >= it->ready_cycle) {
      assert(it->probe_sid < m_l1s.size() && m_l1s[it->probe_sid] != NULL);
      if (m_l1s[it->probe_sid]->c2p_probe(it->mf)) {
        // The probe is later than accept_miss(): another L1 may have filled
        // this line in between, so a real peer hit need not have appeared in
        // the accept-time oracle snapshot.
        ++m_stats.peer_probe_hits;
        ++m_stats.remote_hits;
        if (m_config.scheme == c2p_cache_config::RING_SCHEME)
          ring_throttle_record_hit(*it);
        adaptive_record_probe_result(*it, true);
        const unsigned ordinal = probe_policy_ordinal_bin(it->candidate_next);
        const unsigned candidate_bin = adaptive_candidate_bin(*it);
        ++m_stats.probe_ordinal_hits[ordinal];
        ++m_stats.probe_pc_ordinal_hits[it->probe_pc_bucket][ordinal];
        ++m_stats.probe_candidate_bin_ordinal_hits[candidate_bin][ordinal];
        m_stats.remote_hit_probe_ordinal_total += it->candidate_next;
        ++m_stats.remote_hit_probe_ordinal_samples;
        transition(*it, WAIT_RETURN, now);
        it->ready_cycle = now + it->return_latency;
      } else {
        ++m_stats.peer_probe_misses;
        adaptive_record_probe_result(*it, false);
        const unsigned ordinal = probe_policy_ordinal_bin(it->candidate_next);
        const unsigned candidate_bin = adaptive_candidate_bin(*it);
        ++m_stats.probe_ordinal_misses[ordinal];
        ++m_stats.probe_pc_ordinal_misses[it->probe_pc_bucket][ordinal];
        ++m_stats.probe_candidate_bin_ordinal_misses[candidate_bin][ordinal];
        transition(*it, READY_TO_PROBE, now);
        it->probe_wait_start = now;
      }
    }

    if (it->state == WAIT_RETURN && now >= it->ready_cycle) {
      if (it->requester->fill_port_free()) {
        it->requester->c2p_fill(it->mf, now);
        record_peer_accesses(true, it->peer_accesses);
        retire(*it, now);
        erase = true;
      } else {
        ++m_stats.requester_fill_wait_cycles;
      }
    }

    if (it->state == WAIT_TARGET_PROBE &&
        now - it->probe_wait_start >= m_config.probe_timeout) {
      // A finite target FIFO decouples ordinary data-port contention from
      // candidate issue.  It must still have a bounded escape when the
      // target port itself makes no progress (for example while a writeback
      // path is blocked); otherwise its head can retain the original miss
      // forever and prevent the baseline L2 path from recovering progress.
      std::deque<transaction *> &queue =
          m_target_probe_queues[it->candidates[it->candidate_next - 1]];
      std::deque<transaction *>::iterator queued =
          std::find(queue.begin(), queue.end(), &*it);
      assert(queued != queue.end());
      queue.erase(queued);
      m_stats.target_probe_queue_wait_cycles += now - it->probe_wait_start;
      ++m_stats.fallback_probe_timeout;
      ++m_stats.fallback_target_wait_timeout;
      adaptive_record_probe_timeout(*it);
      begin_fallback(*it, now);
    }

    if (it->state == READY_TO_PROBE) {
      if (m_config.scheme == c2p_cache_config::C2P_SCHEME &&
          m_config.adaptive_probe_observe_tail && it->candidate_next != 0 &&
          it->candidate_next < it->candidates.size() &&
          !it->adaptive_tail_observed) {
        adaptive_observe_tail(*it);
        it->adaptive_tail_observed = true;
      }
      if (it->candidate_next >= it->candidates.size()) {
        if (it->candidates.empty())
          ++m_stats.fallback_no_candidate;
        else
          ++m_stats.fallback_candidates_exhausted;
        adaptive_record_package_outcome(*it, PACKAGE_NO_HIT);
        begin_fallback(*it, now);
      } else if (m_config.scheme == c2p_cache_config::C2P_SCHEME &&
                 m_config.adaptive_probe_policy &&
                 it->candidate_next >= C2P_PROBE_POLICY_MAX_ORDINAL) {
        // The adaptive policy never confirms more than four candidates.  This
        // is independent of its learned score and bounds both request state
        // and diagnostic oracle work.
        adaptive_record_package_residual(*it);
        adaptive_record_stop(*it, true, now);
        adaptive_record_package_outcome(*it, PACKAGE_NO_HIT);
        begin_fallback(*it, now);
      } else if (m_config.scheme == c2p_cache_config::C2P_SCHEME &&
                 m_config.max_candidate_probes != 0 &&
                 it->candidate_next >= m_config.max_candidate_probes) {
        // C2P+ bounds only failed probes.  A selected candidate is always
        // allowed to complete; after this many misses the original request
        // takes the normal lower path instead of extending a serial FP tail.
        ++m_stats.fallback_candidate_budget;
        begin_fallback(*it, now);
      } else {
        if (m_config.scheme == c2p_cache_config::C2P_SCHEME &&
            m_config.adaptive_probe_policy && it->candidate_next != 0 &&
            !it->adaptive_continue_decided) {
          if (it->adaptive_package_active) {
            it->adaptive_continue_decided = true;
          } else if (it->candidate_next == 1) {
            if (adaptive_should_start_package(*it)) {
              it->adaptive_package_active = true;
              it->adaptive_package_outcome_recorded = false;
              it->adaptive_continue_decided = true;
            } else {
              adaptive_record_stop(*it, false, now);
              begin_fallback(*it, now);
            }
          } else {
            // A valid adaptive transaction reaches ordinal two and beyond
            // only after the one package decision above selected it.
            assert(false);
          }
        }
        if (it->state == READY_TO_PROBE) {
          const unsigned sid = it->candidates[it->candidate_next];
          l1_cache *target = m_l1s[sid];
          if (m_config.adaptive_probe_policy && it->candidate_next == 0)
            it->probe_reason = PROBE_FIRST;
          if (it->candidate_next != 0) {
            const unsigned ordinal =
                probe_policy_ordinal_bin(it->candidate_next);
            const unsigned lower_ready =
                it->requester->c2p_lower_ready(it->mf) ? 1 : 0;
            const unsigned target_credit =
                m_target_probe_queues[sid].size() <
                        m_config.target_probe_queue_size
                    ? 1
                    : 0;
            ++m_stats.continuation_decisions[ordinal][lower_ready]
                                               [target_credit];
          }
          // C2P uses a finite request FIFO at each target L1. Queueing a
          // selected peer preserves the target-port contention model without
          // discarding a useful probe merely because that port is busy this
          // cycle. The timeout below applies only when that finite FIFO is
          // full, at which point the original request may safely fall back.
          if (m_config.scheme == c2p_cache_config::C2P_SCHEME &&
              m_config.diagnostic_target_port_bypass) {
            // This is a counterfactual diagnostic, not an architectural C2P
            // mode: remove just target-port/FIFO contention. Candidate order,
            // tag latency, return latency, requester-fill pressure, and all
            // fallback behavior outside that contention remain unchanged.
            it->probe_sid = sid;
            ++it->candidate_next;
            ++it->peer_accesses;
            ++m_stats.peer_probes;
            ++m_stats.peer_l1_accesses;
            transition(*it, WAIT_PROBE, now);
            it->ready_cycle = now + it->probe_latency;
          } else if (m_config.scheme == c2p_cache_config::C2P_SCHEME &&
                     m_target_probe_queues[sid].size() <
                         m_config.target_probe_queue_size) {
            m_target_probe_queues[sid].push_back(&*it);
            ++it->candidate_next;
            ++it->peer_accesses;
            ++m_stats.peer_probes;
            ++m_stats.peer_l1_accesses;
            transition(*it, WAIT_TARGET_PROBE, now);
          } else if (target->data_port_free()) {
            target->c2p_reserve_probe_port(it->probe_latency);
            it->probe_sid = sid;
            ++it->candidate_next;
            ++it->peer_accesses;
            ++m_stats.peer_probes;
            if (m_config.scheme == c2p_cache_config::C2P_SCHEME ||
                m_config.scheme == c2p_cache_config::RING_SCHEME)
              ++m_stats.peer_l1_accesses;
            transition(*it, WAIT_PROBE, now);
            it->ready_cycle = now + it->probe_latency;
          } else {
            if (m_config.scheme == c2p_cache_config::C2P_SCHEME &&
                m_target_probe_queues[sid].size() >=
                    m_config.target_probe_queue_size)
              ++m_stats.target_probe_queue_full_cycles;
            if (now - it->probe_wait_start >= m_config.probe_timeout) {
              ++m_stats.fallback_probe_timeout;
              ++m_stats.fallback_target_admission_timeout;
              adaptive_record_package_outcome(*it, PACKAGE_TIMEOUT);
              begin_fallback(*it, now);
            }
          }
        }
      }
    }

    if (it->state == WAIT_FALLBACK && it->requester->c2p_lower_ready(it->mf)) {
      if (it->adaptive_early_stop_pending) {
        const unsigned pressure = it->adaptive_early_stop_pressure;
        assert(pressure < C2P_PROBE_POLICY_FALLBACK_PRESSURE_BINS &&
               now >= it->adaptive_early_stop_cycle);
        const unsigned long long delay = now - it->adaptive_early_stop_cycle;
        ++m_stats.adaptive_early_stop_lower_samples[pressure];
        m_stats.adaptive_early_stop_lower_cycles[pressure] += delay;
        if (delay) ++m_stats.adaptive_early_stop_lower_waited[pressure];
        it->adaptive_early_stop_pending = false;
      }
      it->requester->c2p_send_lower(it->mf);
      ++m_stats.fallback_queue;
      record_peer_accesses(false, it->peer_accesses);
      retire(*it, now);
      erase = true;
    }
    if (erase)
      it = m_transactions.erase(it);
    else
      ++it;
  }
}

void c2p_cache::record_peer_accesses(bool hit, unsigned accesses) {
  // Figure 14 counts candidate L1 caches actually consulted.  A request
  // pruned with no candidate has zero such accesses and belongs to the
  // no-candidate/fallback counters, not the hit/miss probe distribution.
  if (!accesses) return;
  std::vector<unsigned long long> &hist =
      hit ? m_peer_access_hit_hist : m_peer_access_miss_hist;
  assert(accesses < hist.size());
  ++hist[accesses];
}

void c2p_cache::cycle(unsigned long long now) {
  if (!m_config.enabled) return;
  if (m_config.scheme == c2p_cache_config::C2P_SCHEME) {
    unsigned engines_left = m_config.bf_engines;
    issue_query_encodes(now, engines_left);
    issue_update(now, engines_left);
    schedule_rows(now);
  }
  complete_matches(now);
  advance_probes(now);
}

void c2p_cache::display_state(FILE *fout) const {
  static const char *const state_names[] = {
      "encode", "rows", "match", "ready", "target", "probe", "return",
      "fallback"};
  unsigned counts[sizeof(state_names) / sizeof(state_names[0])] = {0};
  for (std::list<transaction>::const_iterator it = m_transactions.begin();
       it != m_transactions.end(); ++it) {
    assert(it->state < sizeof(counts) / sizeof(counts[0]));
    ++counts[it->state];
  }
  unsigned queued = 0;
  unsigned nonempty_targets = 0;
  unsigned max_depth = 0;
  for (unsigned sid = 0; sid < m_target_probe_queues.size(); ++sid) {
    const unsigned depth = m_target_probe_queues[sid].size();
    queued += depth;
    if (depth) ++nonempty_targets;
    max_depth = std::max(max_depth, depth);
  }
  fprintf(fout, "C2P deadlock state: scheme=%u transactions=%zu ",
          m_config.scheme, m_transactions.size());
  for (unsigned state = 0; state < sizeof(counts) / sizeof(counts[0]); ++state)
    fprintf(fout, "%s=%u%s", state_names[state], counts[state],
            state + 1 == sizeof(counts) / sizeof(counts[0]) ? "" : " ");
  fprintf(fout,
          "\ntarget probe queues: entries=%u nonempty=%u max_depth=%u / %u\n",
          queued, nonempty_targets, max_depth, m_config.target_probe_queue_size);
}

void c2p_cache::print_stats(FILE *fout) const {
  fprintf(fout, "\nC2P_cache_stats:\n");
  fprintf(fout, "c2p_l1_misses = %llu\n", m_stats.l1_misses);
  fprintf(fout, "c2p_oracle_peer_hits = %llu\n", m_stats.oracle_peer_hits);
  fprintf(fout, "c2p_queries_accepted = %llu\n", m_stats.queries_accepted);
  fprintf(fout, "c2p_queries_queue_bypass = %llu\n", m_stats.queries_queue_bypass);
  fprintf(fout, "c2p_updates_queue_bypass = %llu\n", m_stats.updates_queue_bypass);
  fprintf(fout, "c2p_candidate_total = %llu\n", m_stats.candidate_total);
  fprintf(fout, "c2p_candidate_queries = %llu\n", m_stats.candidate_queries);
  fprintf(fout, "c2p_peer_probes = %llu\n", m_stats.peer_probes);
  fprintf(fout, "c2p_peer_probe_hits = %llu\n", m_stats.peer_probe_hits);
  fprintf(fout, "c2p_peer_probe_misses = %llu\n", m_stats.peer_probe_misses);
  fprintf(fout, "c2p_peer_l1_accesses = %llu\n", m_stats.peer_l1_accesses);
  fprintf(fout, "c2p_ring_traversals = %llu\n", m_stats.ring_traversals);
  fprintf(fout, "c2p_ring_no_match_traversals = %llu\n",
          m_stats.ring_no_match_traversals);
  fprintf(fout, "c2p_ring_traversal_hops = %llu\n",
          m_stats.ring_traversal_hops);
  fprintf(fout, "c2p_ring_network_wait_cycles = %llu\n",
          m_stats.ring_network_wait_cycles);
  fprintf(fout, "c2p_ring_queue_bypasses = %llu\n",
          m_stats.ring_queue_bypasses);
  fprintf(fout, "c2p_ring_throttle_bypasses = %llu\n",
          m_stats.ring_throttle_bypasses);
  fprintf(fout, "c2p_ring_throttle_samples = %llu\n",
          m_stats.ring_throttle_samples);
  fprintf(fout, "c2p_ring_throttle_sample_requests = %llu\n",
          m_stats.ring_throttle_sample_requests);
  fprintf(fout, "c2p_ring_throttle_sample_hits = %llu\n",
          m_stats.ring_throttle_sample_hits);
  fprintf(fout, "c2p_target_probe_port_busy_cycles = %llu\n",
          m_stats.target_probe_port_busy_cycles);
  fprintf(fout, "c2p_target_tag_port_busy_cycles = %llu\n",
          m_stats.target_tag_port_busy_cycles);
  fprintf(fout, "c2p_target_probe_queue_wait_cycles = %llu\n",
          m_stats.target_probe_queue_wait_cycles);
  fprintf(fout, "c2p_target_probe_queue_full_cycles = %llu\n",
          m_stats.target_probe_queue_full_cycles);
  fprintf(fout, "c2p_requester_fill_wait_cycles = %llu\n",
          m_stats.requester_fill_wait_cycles);
  fprintf(fout, "c2p_residence_encode_cycles = %llu\n",
          m_stats.residence_encode_cycles);
  fprintf(fout, "c2p_residence_rows_cycles = %llu\n",
          m_stats.residence_rows_cycles);
  fprintf(fout, "c2p_residence_match_cycles = %llu\n",
          m_stats.residence_match_cycles);
  fprintf(fout, "c2p_residence_ready_cycles = %llu\n",
          m_stats.residence_ready_cycles);
  fprintf(fout, "c2p_residence_target_probe_cycles = %llu\n",
          m_stats.residence_target_probe_cycles);
  fprintf(fout, "c2p_residence_probe_cycles = %llu\n",
          m_stats.residence_probe_cycles);
  fprintf(fout, "c2p_residence_return_cycles = %llu\n",
          m_stats.residence_return_cycles);
  fprintf(fout, "c2p_residence_fallback_cycles = %llu\n",
          m_stats.residence_fallback_cycles);
  fprintf(fout, "c2p_remote_hit_probe_ordinal_total = %llu\n",
          m_stats.remote_hit_probe_ordinal_total);
  fprintf(fout, "c2p_remote_hit_probe_ordinal_samples = %llu\n",
          m_stats.remote_hit_probe_ordinal_samples);
  fprintf(fout, "c2p_fallback_probe_ordinal_total = %llu\n",
          m_stats.fallback_probe_ordinal_total);
  fprintf(fout, "c2p_fallback_probe_ordinal_samples = %llu\n",
          m_stats.fallback_probe_ordinal_samples);
  for (unsigned ordinal = 0; ordinal < C2P_PROBE_POLICY_MAX_ORDINAL;
       ++ordinal) {
    fprintf(fout, "c2p_probe_ordinal_%u_hits = %llu\n", ordinal + 1,
            m_stats.probe_ordinal_hits[ordinal]);
    fprintf(fout, "c2p_probe_ordinal_%u_misses = %llu\n", ordinal + 1,
            m_stats.probe_ordinal_misses[ordinal]);
  }
  fprintf(fout, "c2p_probe_ordinal_overflow_hits = %llu\n",
          m_stats.probe_ordinal_hits[C2P_PROBE_POLICY_MAX_ORDINAL]);
  fprintf(fout, "c2p_probe_ordinal_overflow_misses = %llu\n",
          m_stats.probe_ordinal_misses[C2P_PROBE_POLICY_MAX_ORDINAL]);
  for (unsigned bucket = 0; bucket < C2P_PROBE_POLICY_PC_BUCKETS; ++bucket)
    for (unsigned ordinal = 0; ordinal < C2P_PROBE_POLICY_MAX_ORDINAL;
         ++ordinal) {
      fprintf(fout, "c2p_probe_pc_bucket_%u_ordinal_%u_hits = %llu\n", bucket,
              ordinal + 1, m_stats.probe_pc_ordinal_hits[bucket][ordinal]);
      fprintf(fout, "c2p_probe_pc_bucket_%u_ordinal_%u_misses = %llu\n", bucket,
              ordinal + 1, m_stats.probe_pc_ordinal_misses[bucket][ordinal]);
    }
  for (unsigned bin = 0; bin < C2P_PROBE_POLICY_CANDIDATE_BINS; ++bin)
    for (unsigned ordinal = 0; ordinal < C2P_PROBE_POLICY_ORDINAL_BINS;
         ++ordinal) {
      if (ordinal == C2P_PROBE_POLICY_MAX_ORDINAL) {
        fprintf(fout, "c2p_candidate_bin_%u_probe_ordinal_overflow_hits = %llu\n",
                bin, m_stats.probe_candidate_bin_ordinal_hits[bin][ordinal]);
        fprintf(fout, "c2p_candidate_bin_%u_probe_ordinal_overflow_misses = %llu\n",
                bin, m_stats.probe_candidate_bin_ordinal_misses[bin][ordinal]);
      } else {
        fprintf(fout, "c2p_candidate_bin_%u_probe_ordinal_%u_hits = %llu\n",
                bin, ordinal + 1,
                m_stats.probe_candidate_bin_ordinal_hits[bin][ordinal]);
        fprintf(fout, "c2p_candidate_bin_%u_probe_ordinal_%u_misses = %llu\n",
                bin, ordinal + 1,
                m_stats.probe_candidate_bin_ordinal_misses[bin][ordinal]);
      }
    }
  for (unsigned ordinal = 0; ordinal < C2P_PROBE_POLICY_MAX_ORDINAL;
       ++ordinal)
    for (unsigned lower_ready = 0; lower_ready != 2; ++lower_ready)
      for (unsigned target_credit = 0; target_credit != 2; ++target_credit)
        fprintf(fout,
                "c2p_continuation_after_fail_%u_lower_ready_%u_target_credit_%u = %llu\n",
                ordinal + 1, lower_ready, target_credit,
                m_stats.continuation_decisions[ordinal][lower_ready]
                                                  [target_credit]);
  for (unsigned lower_ready = 0; lower_ready != 2; ++lower_ready)
    for (unsigned target_credit = 0; target_credit != 2; ++target_credit)
      fprintf(fout,
              "c2p_continuation_after_fail_overflow_lower_ready_%u_target_credit_%u = %llu\n",
              lower_ready, target_credit,
              m_stats.continuation_decisions[C2P_PROBE_POLICY_MAX_ORDINAL]
                                                [lower_ready][target_credit]);
  fprintf(fout, "c2p_adaptive_continuation_opportunities = %llu\n",
          m_stats.adaptive_continuation_opportunities);
  fprintf(fout, "c2p_adaptive_continue_predictor = %llu\n",
          m_stats.adaptive_continue_predictor);
  fprintf(fout, "c2p_adaptive_continue_exploration = %llu\n",
          m_stats.adaptive_continue_exploration);
  fprintf(fout, "c2p_adaptive_continue_forced = %llu\n",
          m_stats.adaptive_continue_forced);
  fprintf(fout, "c2p_adaptive_stop_predictor = %llu\n",
          m_stats.adaptive_stop_predictor);
  fprintf(fout, "c2p_adaptive_stop_hard_cap = %llu\n",
          m_stats.adaptive_stop_hard_cap);
  fprintf(fout, "c2p_adaptive_stop_later_peer = %llu\n",
          m_stats.adaptive_stop_later_peer);
  fprintf(fout, "c2p_adaptive_stop_no_later_peer = %llu\n",
          m_stats.adaptive_stop_no_later_peer);
  fprintf(fout, "c2p_adaptive_stop_remaining_candidates = %llu\n",
          m_stats.adaptive_stop_remaining_candidates);
  fprintf(fout, "c2p_adaptive_stop_next_peer_distance_total = %llu\n",
          m_stats.adaptive_stop_next_peer_distance_total);
  for (unsigned bin = 0; bin < C2P_PROBE_POLICY_CANDIDATE_BINS; ++bin) {
    fprintf(fout, "c2p_adaptive_early_stop_bin_%u_opportunities = %llu\n", bin,
            m_stats.adaptive_early_stop_opportunities[bin]);
    fprintf(fout, "c2p_adaptive_early_stop_bin_%u_later_peer = %llu\n", bin,
            m_stats.adaptive_early_stop_later_peer[bin]);
    fprintf(fout, "c2p_adaptive_early_stop_bin_%u_no_later_peer = %llu\n", bin,
            m_stats.adaptive_early_stop_no_later_peer[bin]);
    for (unsigned distance = 0;
         distance < C2P_PROBE_POLICY_DISTANCE_BINS; ++distance) {
      if (distance == C2P_PROBE_POLICY_MAX_ORDINAL)
        fprintf(fout,
                "c2p_adaptive_early_stop_bin_%u_distance_overflow = %llu\n",
                bin, m_stats.adaptive_early_stop_distance[bin][distance]);
      else
        fprintf(fout,
                "c2p_adaptive_early_stop_bin_%u_distance_%u = %llu\n", bin,
                distance + 1,
                m_stats.adaptive_early_stop_distance[bin][distance]);
    }
  }
  for (unsigned pressure = 0;
       pressure != C2P_PROBE_POLICY_FALLBACK_PRESSURE_BINS; ++pressure) {
    fprintf(fout, "c2p_adaptive_early_stop_lower_pressure_%u_samples = %llu\n",
            pressure, m_stats.adaptive_early_stop_lower_samples[pressure]);
    fprintf(fout, "c2p_adaptive_early_stop_lower_pressure_%u_cycles = %llu\n",
            pressure, m_stats.adaptive_early_stop_lower_cycles[pressure]);
    fprintf(fout, "c2p_adaptive_early_stop_lower_pressure_%u_waited = %llu\n",
            pressure, m_stats.adaptive_early_stop_lower_waited[pressure]);
  }
  fprintf(fout, "c2p_adaptive_first_probe_hits = %llu\n",
          m_stats.adaptive_first_probe_hits);
  fprintf(fout, "c2p_adaptive_first_probe_misses = %llu\n",
          m_stats.adaptive_first_probe_misses);
  fprintf(fout, "c2p_adaptive_first_probe_timeouts = %llu\n",
          m_stats.adaptive_first_probe_timeouts);
  fprintf(fout, "c2p_adaptive_predictor_probe_hits = %llu\n",
          m_stats.adaptive_predictor_probe_hits);
  fprintf(fout, "c2p_adaptive_predictor_probe_misses = %llu\n",
          m_stats.adaptive_predictor_probe_misses);
  fprintf(fout, "c2p_adaptive_predictor_probe_timeouts = %llu\n",
          m_stats.adaptive_predictor_probe_timeouts);
  fprintf(fout, "c2p_adaptive_exploration_probe_hits = %llu\n",
          m_stats.adaptive_exploration_probe_hits);
  fprintf(fout, "c2p_adaptive_exploration_probe_misses = %llu\n",
          m_stats.adaptive_exploration_probe_misses);
  fprintf(fout, "c2p_adaptive_exploration_probe_timeouts = %llu\n",
          m_stats.adaptive_exploration_probe_timeouts);
  fprintf(fout, "c2p_adaptive_forced_probe_hits = %llu\n",
          m_stats.adaptive_forced_probe_hits);
  fprintf(fout, "c2p_adaptive_forced_probe_misses = %llu\n",
          m_stats.adaptive_forced_probe_misses);
  fprintf(fout, "c2p_adaptive_forced_probe_timeouts = %llu\n",
          m_stats.adaptive_forced_probe_timeouts);
  for (unsigned score = 0; score != 8; ++score)
    fprintf(fout, "c2p_adaptive_score_%u_samples = %llu\n", score,
            m_stats.adaptive_score_hist[score]);
  fprintf(fout, "c2p_adaptive_package_opportunities = %llu\n",
          m_stats.adaptive_package_opportunities);
  fprintf(fout, "c2p_adaptive_package_start_predictor = %llu\n",
          m_stats.adaptive_package_start_predictor);
  fprintf(fout, "c2p_adaptive_package_start_exploration = %llu\n",
          m_stats.adaptive_package_start_exploration);
  fprintf(fout, "c2p_adaptive_package_start_forced = %llu\n",
          m_stats.adaptive_package_start_forced);
  fprintf(fout, "c2p_adaptive_package_stop_predictor = %llu\n",
          m_stats.adaptive_package_stop_predictor);
  fprintf(fout, "c2p_adaptive_package_hit = %llu\n",
          m_stats.adaptive_package_hit);
  fprintf(fout, "c2p_adaptive_package_no_hit = %llu\n",
          m_stats.adaptive_package_no_hit);
  fprintf(fout, "c2p_adaptive_package_timeout = %llu\n",
          m_stats.adaptive_package_timeout);
  for (unsigned score = 0; score != 8; ++score)
    fprintf(fout, "c2p_adaptive_package_score_%u_samples = %llu\n", score,
            m_stats.adaptive_package_score_hist[score]);
  fprintf(fout, "c2p_adaptive_package_residual_opportunities = %llu\n",
          m_stats.adaptive_package_residual_opportunities);
  fprintf(fout, "c2p_adaptive_package_residual_later_peer = %llu\n",
          m_stats.adaptive_package_residual_later_peer);
  fprintf(fout, "c2p_adaptive_package_residual_no_later_peer = %llu\n",
          m_stats.adaptive_package_residual_no_later_peer);
  fprintf(fout,
          "c2p_adaptive_package_residual_remaining_candidates = %llu\n",
          m_stats.adaptive_package_residual_remaining_candidates);
  fprintf(fout,
          "c2p_adaptive_package_residual_next_peer_distance_total = %llu\n",
          m_stats.adaptive_package_residual_next_peer_distance_total);
  for (unsigned bin = 0; bin != C2P_PROBE_POLICY_CANDIDATE_BINS; ++bin) {
    fprintf(fout, "c2p_adaptive_tail_bin_%u_opportunities = %llu\n", bin,
            m_stats.adaptive_tail_observe_opportunities[bin]);
    fprintf(fout, "c2p_adaptive_tail_bin_%u_later_peer = %llu\n", bin,
            m_stats.adaptive_tail_observe_later_peer[bin]);
    fprintf(fout, "c2p_adaptive_tail_bin_%u_no_later_peer = %llu\n", bin,
            m_stats.adaptive_tail_observe_no_later_peer[bin]);
    for (unsigned distance = 0;
         distance != C2P_PROBE_POLICY_DISTANCE_BINS; ++distance)
      fprintf(fout,
              "c2p_adaptive_tail_bin_%u_distance_%s%u = %llu\n", bin,
              distance == C2P_PROBE_POLICY_MAX_ORDINAL ? "overflow_" : "",
              distance == C2P_PROBE_POLICY_MAX_ORDINAL ? 5 : distance + 1,
              m_stats.adaptive_tail_observe_distance[bin][distance]);
  }
  if (m_config.adaptive_probe_observe_addr_topology) {
    // Emit only populated buckets.  These are offline observation records,
    // not predictor state and never participate in a timing decision.
    for (unsigned region = 0; region != C2P_ADDR_OBSERVE_REGION_BUCKETS;
         ++region)
      for (unsigned bin = 0; bin != C2P_PROBE_POLICY_CANDIDATE_BINS; ++bin) {
        const addr_topology_observation &entry =
            m_addr_observe_region[region * C2P_PROBE_POLICY_CANDIDATE_BINS +
                                  bin];
        if (!entry.opportunities) continue;
        fprintf(fout,
                "c2p_addr_obs_region_%u_bin_%u_opportunities = %llu\n",
                region, bin, entry.opportunities);
        fprintf(fout, "c2p_addr_obs_region_%u_bin_%u_later_peer = %llu\n",
                region, bin, entry.later_peer);
        fprintf(fout, "c2p_addr_obs_region_%u_bin_%u_within_4 = %llu\n",
                region, bin, entry.within_4);
        fprintf(fout, "c2p_addr_obs_region_%u_bin_%u_lower_ready = %llu\n",
                region, bin, entry.lower_ready);
        fprintf(fout,
                "c2p_addr_obs_region_%u_bin_%u_target_credit = %llu\n",
                region, bin, entry.target_credit);
      }
    for (unsigned cluster = 0; cluster != m_addr_observe_cluster_count;
         ++cluster)
      for (unsigned bin = 0; bin != C2P_PROBE_POLICY_CANDIDATE_BINS; ++bin) {
        const addr_topology_observation &entry =
            m_addr_observe_cluster[cluster * C2P_PROBE_POLICY_CANDIDATE_BINS +
                                   bin];
        if (!entry.opportunities) continue;
        fprintf(fout,
                "c2p_addr_obs_cluster_%u_bin_%u_opportunities = %llu\n",
                cluster, bin, entry.opportunities);
        fprintf(fout, "c2p_addr_obs_cluster_%u_bin_%u_later_peer = %llu\n",
                cluster, bin, entry.later_peer);
        fprintf(fout, "c2p_addr_obs_cluster_%u_bin_%u_within_4 = %llu\n",
                cluster, bin, entry.within_4);
        fprintf(fout, "c2p_addr_obs_cluster_%u_bin_%u_lower_ready = %llu\n",
                cluster, bin, entry.lower_ready);
        fprintf(fout,
                "c2p_addr_obs_cluster_%u_bin_%u_target_credit = %llu\n",
                cluster, bin, entry.target_credit);
      }
    for (unsigned region = 0; region != C2P_ADDR_OBSERVE_REGION_BUCKETS;
         ++region)
      for (unsigned cluster = 0; cluster != m_addr_observe_cluster_count;
           ++cluster)
        for (unsigned bin = 0; bin != C2P_PROBE_POLICY_CANDIDATE_BINS;
             ++bin) {
          const addr_topology_observation &entry =
              m_addr_observe_region_cluster[
                  (region * m_addr_observe_cluster_count + cluster) *
                      C2P_PROBE_POLICY_CANDIDATE_BINS +
                  bin];
          if (!entry.opportunities) continue;
          fprintf(fout,
                  "c2p_addr_obs_region_%u_cluster_%u_bin_%u_opportunities = %llu\n",
                  region, cluster, bin, entry.opportunities);
          fprintf(fout,
                  "c2p_addr_obs_region_%u_cluster_%u_bin_%u_later_peer = %llu\n",
                  region, cluster, bin, entry.later_peer);
          fprintf(fout,
                  "c2p_addr_obs_region_%u_cluster_%u_bin_%u_within_4 = %llu\n",
                  region, cluster, bin, entry.within_4);
          fprintf(fout,
                  "c2p_addr_obs_region_%u_cluster_%u_bin_%u_lower_ready = %llu\n",
                  region, cluster, bin, entry.lower_ready);
          fprintf(fout,
                  "c2p_addr_obs_region_%u_cluster_%u_bin_%u_target_credit = %llu\n",
                  region, cluster, bin, entry.target_credit);
        }
  }
  fprintf(fout, "c2p_fallback_target_wait_timeout = %llu\n",
          m_stats.fallback_target_wait_timeout);
  fprintf(fout, "c2p_fallback_target_admission_timeout = %llu\n",
          m_stats.fallback_target_admission_timeout);
  fprintf(fout, "c2p_peer_lost_before_query = %llu\n",
          m_stats.peer_lost_before_query);
  fprintf(fout, "c2p_peer_gained_before_query = %llu\n",
          m_stats.peer_gained_before_query);
  fprintf(fout, "c2p_remote_hits = %llu\n", m_stats.remote_hits);
  // Each completed remote hit consumes the original L1 MSHR/fill path but
  // never sends that miss to the lower level, so this is the directly
  // attributable redundant-L2 reduction metric.
  fprintf(fout, "c2p_l2_requests_avoided = %llu\n", m_stats.remote_hits);
  fprintf(fout, "c2p_fallback_no_candidate = %llu\n", m_stats.fallback_no_candidate);
  fprintf(fout, "c2p_fallback_candidates_exhausted = %llu\n",
          m_stats.fallback_candidates_exhausted);
  fprintf(fout, "c2p_fallback_candidate_budget = %llu\n",
          m_stats.fallback_candidate_budget);
  fprintf(fout, "c2p_fallback_probe_timeout = %llu\n", m_stats.fallback_probe_timeout);
  fprintf(fout, "c2p_fallback_queue = %llu\n", m_stats.fallback_queue);
  fprintf(fout, "c2p_snapshot_false_positive = %llu\n", m_stats.snapshot_false_positive);
  fprintf(fout, "c2p_snapshot_false_negative = %llu\n", m_stats.snapshot_false_negative);
  fprintf(fout, "c2p_snapshot_true_positive = %llu\n", m_stats.snapshot_true_positive);
  fprintf(fout, "c2p_snapshot_true_negative = %llu\n", m_stats.snapshot_true_negative);
  fprintf(fout, "c2p_snapshot_query_false_positive = %llu\n",
          m_stats.snapshot_query_false_positive);
  fprintf(fout, "c2p_snapshot_query_false_negative = %llu\n",
          m_stats.snapshot_query_false_negative);
  fprintf(fout, "c2p_snapshot_query_true_positive = %llu\n",
          m_stats.snapshot_query_true_positive);
  fprintf(fout, "c2p_snapshot_query_true_negative = %llu\n",
          m_stats.snapshot_query_true_negative);
  fprintf(fout, "c2p_ccd_false_positive = %llu\n", m_stats.ccd_false_positive);
  fprintf(fout, "c2p_ccd_false_negative = %llu\n", m_stats.ccd_false_negative);
  fprintf(fout, "c2p_ccd_true_positive = %llu\n", m_stats.ccd_true_positive);
  fprintf(fout, "c2p_ccd_true_negative = %llu\n", m_stats.ccd_true_negative);
  fprintf(fout, "c2p_snapshot_updates = %llu\n", m_stats.snapshot_updates);
  fprintf(fout, "c2p_snapshot_rebuilds = %llu\n", m_stats.snapshot_rebuilds);
  fprintf(fout, "c2p_snapshot_rebuild_transport_tags = %llu\n",
          m_stats.snapshot_rebuild_transport_tags);
  unsigned long long hit_samples = 0;
  unsigned long long miss_samples = 0;
  for (unsigned i = 0; i < m_peer_access_hit_hist.size(); ++i) {
    hit_samples += m_peer_access_hit_hist[i];
    miss_samples += m_peer_access_miss_hist[i];
  }
  fprintf(fout, "c2p_peer_access_hit_samples = %llu\n", hit_samples);
  fprintf(fout, "c2p_peer_access_hit_p90 = %u\n",
          histogram_percentile(m_peer_access_hit_hist, 90));
  fprintf(fout, "c2p_peer_access_hit_p95 = %u\n",
          histogram_percentile(m_peer_access_hit_hist, 95));
  fprintf(fout, "c2p_peer_access_hit_p99 = %u\n",
          histogram_percentile(m_peer_access_hit_hist, 99));
  fprintf(fout, "c2p_peer_access_hit_max = %u\n",
          histogram_max(m_peer_access_hit_hist));
  fprintf(fout, "c2p_peer_access_miss_samples = %llu\n", miss_samples);
  fprintf(fout, "c2p_peer_access_miss_p90 = %u\n",
          histogram_percentile(m_peer_access_miss_hist, 90));
  fprintf(fout, "c2p_peer_access_miss_p95 = %u\n",
          histogram_percentile(m_peer_access_miss_hist, 95));
  fprintf(fout, "c2p_peer_access_miss_p99 = %u\n",
          histogram_percentile(m_peer_access_miss_hist, 99));
  fprintf(fout, "c2p_peer_access_miss_max = %u\n",
          histogram_max(m_peer_access_miss_hist));
  // Keep the complete distributions as well as percentile summaries.  The
  // paper's Figure 14 compares the number of peer L1s consulted by completed
  // remote hits and by probe attempts that ultimately fall back; a percentile
  // alone cannot recreate that plot or expose a long tail.
  for (unsigned i = 1; i < m_peer_access_hit_hist.size(); ++i) {
    if (m_peer_access_hit_hist[i])
      fprintf(fout, "c2p_peer_access_hit_count_%u = %llu\n", i,
              m_peer_access_hit_hist[i]);
    if (m_peer_access_miss_hist[i])
      fprintf(fout, "c2p_peer_access_miss_count_%u = %llu\n", i,
              m_peer_access_miss_hist[i]);
  }
}
