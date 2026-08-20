#include "c2p-cache.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>

#include "../option_parser.h"
#include "gpu-cache.h"
#include "gpu-sim.h"

namespace {
const unsigned kSnapshotRows = 5120;
const unsigned kSnapshotBanks = 64;

uint32_t fold_hash(uint64_t value, uint32_t salt) {
  uint64_t x = value ^ (uint64_t)salt * 0x9e3779b97f4a7c15ULL;
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return (uint32_t)(x ^ (x >> 32));
}
}  // namespace

c2p_cache_config::c2p_cache_config()
    : enabled(false),
      oracle_only(false),
      ideal_peer_lookup(false),
      collect_oracle(true),
      bf_engines(128),
      bf_latency(2),
      snapshot_latency(2),
      remote_tag_latency(7),
      remote_return_latency(2),
      query_queue_size(256),
      update_queue_size(1024),
      update_transport_bytes_per_cycle(128),
      snapshot_rebuild_interval(0),
      probe_timeout(32),
      snapshot_copies(4),
      scheme(C2P_SCHEME),
      ata_cluster_issue_width(4),
      ata_tag_latency(7),
      ccd_predictor_latency(1),
      ccd_broadcast_latency(3),
      ring_hop_latency(2),
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
                         "C2P target-L1 busy timeout before L2 fallback", "32");
  option_parser_register(opp, "-c2p_cache_snapshot_copies", OPT_UINT32,
                         &snapshot_copies, "C2P Snapshot Matrix copies", "4");
  option_parser_register(
      opp, "-c2p_cache_scheme", OPT_UINT32, &scheme,
      "sharing scheme: 0=C2P, 1=ATA-like, 2=CCD-like, 3=RING-like", "0");
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
  remote_hits = 0;
  fallback_no_candidate = 0;
  fallback_candidates_exhausted = 0;
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
      ready_cycle(now),
      probe_wait_start(now),
      state(WAIT_ENCODE),
      candidate_next(0),
      probe_sid((unsigned)-1),
      oracle_peer_hit(false),
      sharing_attempt(false),
      ring_started(false),
      probe_latency(0),
      return_latency(0) {}

c2p_cache::c2p_cache(const c2p_cache_config &config, gpgpu_sim *gpu)
    : m_config(config),
      m_gpu(gpu),
      m_num_sms(gpu->get_config().num_shader()),
      m_words((m_num_sms + 63) / 64),
      m_l1s(m_num_sms, (l1_cache *)NULL),
      m_snapshot(kSnapshotRows, std::vector<uint64_t>(m_words, 0)),
      m_rebuild_sid(0),
      m_rebuild_target_sid((unsigned)-1),
      m_rebuild_active(false),
      m_next_rebuild_cycle(0),
      m_rebuild_enqueue_next_tag(0),
      m_rebuild_pending_tags(0),
      m_bank_copy_used(kSnapshotBanks,
                       std::vector<bool>(std::max(1U, config.snapshot_copies),
                                         false)),
      m_ccd_counters(std::max(1U, gpu->get_config().num_cluster()), 2),
      m_ata_issue_cycle(0),
      m_ata_issues(std::max(1U, gpu->get_config().num_cluster()), 0),
      m_ring_next_issue_cycle(0) {
  assert(m_config.scheme <= c2p_cache_config::RING_SCHEME);
}

void c2p_cache::reset() {
  for (unsigned row = 0; row < m_snapshot.size(); ++row)
    std::fill(m_snapshot[row].begin(), m_snapshot[row].end(), 0);
  m_transactions.clear();
  m_update_queue.clear();
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
  const unsigned lower10 = (unsigned)(line_tag & 0x3ffU);
  unsigned reversed = 0;
  for (unsigned i = 0; i < 10; ++i)
    reversed |= ((lower10 >> i) & 1U) << (9 - i);
  const unsigned tag_bank = reversed >> 4;
  const unsigned tag_offset = 64 + (reversed & 0xfU);
  rows.push_back(tag_bank * 80 + tag_offset);

  const unsigned h1 = fold_hash(line_tag, 0x243f6a88U) & 0xfffU;
  const unsigned h2 = fold_hash(line_tag, 0x85a308d3U) & 0xfffU;
  for (unsigned multiple = 1; multiple <= 3; ++multiple) {
    const unsigned index = (multiple * h1 + h2) & 0xfffU;
    const unsigned bank = index >> 6;
    const unsigned offset = index & 0x3fU;
    rows.push_back(bank * 80 + offset);
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
  const unsigned clusters = std::max(1U, m_gpu->get_config().num_cluster());
  return std::max(1U, m_num_sms / clusters);
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

bool c2p_cache::accept_miss(l1_cache *requester, mem_fetch *mf,
                            unsigned long long now) {
  if ((!m_config.enabled && !m_config.oracle_only) || mf->get_is_write() ||
      mf->isatomic() ||
      mf->get_access_type() != GLOBAL_ACC_R)
    return false;

  ++m_stats.l1_misses;
  const bool oracle = m_config.collect_oracle && has_exact_peer(requester, mf);
  if (oracle) ++m_stats.oracle_peer_hits;
  if (m_config.oracle_only || !m_config.enabled) return false;
  if (m_transactions.size() >= m_config.query_queue_size) {
    ++m_stats.queries_queue_bypass;
    return false;
  }
  if (m_config.scheme == c2p_cache_config::ATA_SCHEME) {
    if (m_ata_issue_cycle != now) {
      m_ata_issue_cycle = now;
      std::fill(m_ata_issues.begin(), m_ata_issues.end(), 0);
    }
    const unsigned cluster = cluster_id(requester->c2p_sid());
    if (m_ata_issues[cluster] >= m_config.ata_cluster_issue_width)
      return false;
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
  m_transactions.push_back(txn);
  ++m_stats.queries_accepted;
  return true;
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
    set_snapshot_bits(entry.sid, entry.line_tag);
    ++m_stats.snapshot_updates;
    if (entry.rebuild) {
      assert(m_rebuild_pending_tags > 0);
      --m_rebuild_pending_tags;
    }
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
    it->state = WAIT_ROWS;
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
      const unsigned bank = it->rows[row_i] / 80;
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
      it->state = WAIT_MATCH;
      it->ready_cycle = now + m_config.snapshot_latency;
    }
  }
}

unsigned c2p_cache::cluster_distance(unsigned from_sid, unsigned to_sid) const {
  const unsigned clusters = std::max(1U, m_gpu->get_config().num_cluster());
  const unsigned sm_per_cluster = std::max(1U, m_num_sms / clusters);
  const unsigned from_cluster = from_sid / sm_per_cluster;
  const unsigned to_cluster = to_sid / sm_per_cluster;
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
      if (it->sharing_attempt) {
        // CCD broadcasts to all cluster peers when its two-bit predictor is
        // taken; exact tags then select the peer that returns the line.
        m_stats.peer_l1_accesses += cluster_size();
        if (it->candidates.empty()) {
          if (m_ccd_counters[cluster]) --m_ccd_counters[cluster];
        } else if (m_ccd_counters[cluster] < 3) {
          ++m_ccd_counters[cluster];
        }
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
        const unsigned hops =
            it->candidates.empty()
                ? m_num_sms - 1
                : ring_distance(it->requester_sid, it->candidates.front());
        const unsigned long long start = std::max(now, m_ring_next_issue_cycle);
        m_ring_next_issue_cycle = start + m_config.ring_hop_latency;
        it->ready_cycle = start +
                          (unsigned long long)hops * m_config.ring_hop_latency +
                          m_config.remote_tag_latency;
        it->ring_started = true;
        if (now < it->ready_cycle) continue;
      }
    }
    m_stats.candidate_total += it->candidates.size();
    ++m_stats.candidate_queries;
    it->state = READY_TO_PROBE;
    it->probe_wait_start = now;
  }
}

void c2p_cache::advance_probes(unsigned long long now) {
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
        it->state = WAIT_RETURN;
        it->ready_cycle = now + it->return_latency;
      } else {
        ++m_stats.peer_probe_misses;
        it->state = READY_TO_PROBE;
        it->probe_wait_start = now;
      }
    }

    if (it->state == WAIT_RETURN && now >= it->ready_cycle &&
        it->requester->fill_port_free()) {
      it->requester->c2p_fill(it->mf, now);
      erase = true;
    }

    if (it->state == READY_TO_PROBE) {
      if (it->candidate_next >= it->candidates.size()) {
        if (it->candidates.empty())
          ++m_stats.fallback_no_candidate;
        else
          ++m_stats.fallback_candidates_exhausted;
        it->state = WAIT_FALLBACK;
      } else {
        const unsigned sid = it->candidates[it->candidate_next];
        l1_cache *target = m_l1s[sid];
        if (target->data_port_free()) {
          target->c2p_reserve_probe_port(it->probe_latency);
          it->probe_sid = sid;
          ++it->candidate_next;
          ++m_stats.peer_probes;
          if (m_config.scheme == c2p_cache_config::C2P_SCHEME ||
              m_config.scheme == c2p_cache_config::RING_SCHEME)
            ++m_stats.peer_l1_accesses;
          it->state = WAIT_PROBE;
          it->ready_cycle = now + it->probe_latency;
        } else if (now - it->probe_wait_start >= m_config.probe_timeout) {
          ++m_stats.fallback_probe_timeout;
          it->state = WAIT_FALLBACK;
        }
      }
    }

    if (it->state == WAIT_FALLBACK && it->requester->c2p_lower_ready(it->mf)) {
      it->requester->c2p_send_lower(it->mf);
      ++m_stats.fallback_queue;
      erase = true;
    }
    if (erase)
      it = m_transactions.erase(it);
    else
      ++it;
  }
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
  fprintf(fout, "c2p_remote_hits = %llu\n", m_stats.remote_hits);
  // Each completed remote hit consumes the original L1 MSHR/fill path but
  // never sends that miss to the lower level, so this is the directly
  // attributable redundant-L2 reduction metric.
  fprintf(fout, "c2p_l2_requests_avoided = %llu\n", m_stats.remote_hits);
  fprintf(fout, "c2p_fallback_no_candidate = %llu\n", m_stats.fallback_no_candidate);
  fprintf(fout, "c2p_fallback_candidates_exhausted = %llu\n",
          m_stats.fallback_candidates_exhausted);
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
  fprintf(fout, "c2p_snapshot_updates = %llu\n", m_stats.snapshot_updates);
  fprintf(fout, "c2p_snapshot_rebuilds = %llu\n", m_stats.snapshot_rebuilds);
  fprintf(fout, "c2p_snapshot_rebuild_transport_tags = %llu\n",
          m_stats.snapshot_rebuild_transport_tags);
}
