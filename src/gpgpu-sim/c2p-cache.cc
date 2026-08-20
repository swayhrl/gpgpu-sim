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
      snapshot_rebuild_interval(100000),
      probe_timeout(32),
      snapshot_copies(4) {}

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
  option_parser_register(opp, "-c2p_cache_snapshot_rebuild_interval",
                         OPT_UINT32, &snapshot_rebuild_interval,
                         "cycles between background C2P L1 snapshot rebuilds",
                         "100000");
  option_parser_register(opp, "-c2p_cache_probe_timeout", OPT_UINT32,
                         &probe_timeout,
                         "C2P target-L1 busy timeout before L2 fallback", "32");
  option_parser_register(opp, "-c2p_cache_snapshot_copies", OPT_UINT32,
                         &snapshot_copies, "C2P Snapshot Matrix copies", "4");
}

c2p_cache_stats::c2p_cache_stats() { clear(); }

void c2p_cache_stats::clear() {
  l1_misses = 0;
  oracle_peer_hits = 0;
  queries_accepted = 0;
  queries_queue_bypass = 0;
  candidate_total = 0;
  candidate_queries = 0;
  peer_probes = 0;
  peer_probe_hits = 0;
  peer_probe_misses = 0;
  remote_hits = 0;
  fallback_no_candidate = 0;
  fallback_probe_timeout = 0;
  fallback_queue = 0;
  snapshot_false_positive = 0;
  snapshot_false_negative = 0;
  snapshot_updates = 0;
  snapshot_rebuilds = 0;
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
      oracle_peer_hit(false) {}

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
      m_rebuild_next_tag(0),
      m_bank_copy_used(kSnapshotBanks,
                       std::vector<bool>(std::max(1U, config.snapshot_copies),
                                         false)) {}

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
  m_rebuild_next_tag = 0;
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
  for (std::list<transaction>::const_iterator it = m_transactions.begin();
       it != m_transactions.end(); ++it)
    assert(it->mf != mf);

  transaction txn(requester, mf, requester->c2p_sid(),
                  requester->c2p_line_tag(mf->get_addr()), now);
  txn.oracle_peer_hit = oracle;
  txn.rows = query_rows(txn.line_tag);
  txn.row_done.assign(txn.rows.size(), false);
  m_transactions.push_back(txn);
  ++m_stats.queries_accepted;
  return true;
}

void c2p_cache::on_l1_fill(l1_cache *cache, mem_fetch *mf) {
  if (!m_config.enabled || mf->get_is_write()) return;
  if (m_update_queue.size() >= m_config.update_queue_size) return;
  m_update_queue.push_back(
      update_entry(cache->c2p_sid(), cache->c2p_line_tag(mf->get_addr())));
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
    m_rebuild_next_tag = 0;
    m_rebuild_active = true;
    ++m_stats.snapshot_rebuilds;
    return;
  }
}

void c2p_cache::issue_update(unsigned long long now, unsigned &engines_left) {
  if (!m_rebuild_active && now >= m_next_rebuild_cycle) {
    begin_next_rebuild();
  }
  while (engines_left && !m_update_queue.empty()) {
    const update_entry entry = m_update_queue.front();
    m_update_queue.pop_front();
    set_snapshot_bits(entry.first, entry.second);
    ++m_stats.snapshot_updates;
    --engines_left;
  }
  while (engines_left && m_rebuild_next_tag < m_rebuild_tags.size()) {
    assert(m_rebuild_target_sid < m_l1s.size());
    set_snapshot_bits(m_rebuild_target_sid,
                      m_rebuild_tags[m_rebuild_next_tag++]);
    ++m_stats.snapshot_updates;
    --engines_left;
  }
  if (m_rebuild_active && m_rebuild_next_tag >= m_rebuild_tags.size()) {
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
    it->candidates = ordered_candidates(*it);
    m_stats.candidate_total += it->candidates.size();
    ++m_stats.candidate_queries;
    if (!m_config.ideal_peer_lookup && !it->candidates.empty() &&
        !it->oracle_peer_hit)
      ++m_stats.snapshot_false_positive;
    if (!m_config.ideal_peer_lookup && it->candidates.empty() &&
        it->oracle_peer_hit)
      ++m_stats.snapshot_false_negative;
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
        it->ready_cycle = now + m_config.remote_return_latency;
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
        ++m_stats.fallback_no_candidate;
        it->state = WAIT_FALLBACK;
      } else {
        const unsigned sid = it->candidates[it->candidate_next];
        l1_cache *target = m_l1s[sid];
        if (target->data_port_free()) {
          target->c2p_reserve_probe_port(m_config.remote_tag_latency);
          it->probe_sid = sid;
          ++it->candidate_next;
          ++m_stats.peer_probes;
          it->state = WAIT_PROBE;
          it->ready_cycle = now + m_config.remote_tag_latency;
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
  unsigned engines_left = m_config.bf_engines;
  issue_query_encodes(now, engines_left);
  issue_update(now, engines_left);
  schedule_rows(now);
  complete_matches(now);
  advance_probes(now);
}

void c2p_cache::print_stats(FILE *fout) const {
  fprintf(fout, "\nC2P_cache_stats:\n");
  fprintf(fout, "c2p_l1_misses = %llu\n", m_stats.l1_misses);
  fprintf(fout, "c2p_oracle_peer_hits = %llu\n", m_stats.oracle_peer_hits);
  fprintf(fout, "c2p_queries_accepted = %llu\n", m_stats.queries_accepted);
  fprintf(fout, "c2p_queries_queue_bypass = %llu\n", m_stats.queries_queue_bypass);
  fprintf(fout, "c2p_candidate_total = %llu\n", m_stats.candidate_total);
  fprintf(fout, "c2p_candidate_queries = %llu\n", m_stats.candidate_queries);
  fprintf(fout, "c2p_peer_probes = %llu\n", m_stats.peer_probes);
  fprintf(fout, "c2p_peer_probe_hits = %llu\n", m_stats.peer_probe_hits);
  fprintf(fout, "c2p_peer_probe_misses = %llu\n", m_stats.peer_probe_misses);
  fprintf(fout, "c2p_remote_hits = %llu\n", m_stats.remote_hits);
  // Each completed remote hit consumes the original L1 MSHR/fill path but
  // never sends that miss to the lower level, so this is the directly
  // attributable redundant-L2 reduction metric.
  fprintf(fout, "c2p_l2_requests_avoided = %llu\n", m_stats.remote_hits);
  fprintf(fout, "c2p_fallback_no_candidate = %llu\n", m_stats.fallback_no_candidate);
  fprintf(fout, "c2p_fallback_probe_timeout = %llu\n", m_stats.fallback_probe_timeout);
  fprintf(fout, "c2p_fallback_queue = %llu\n", m_stats.fallback_queue);
  fprintf(fout, "c2p_snapshot_false_positive = %llu\n", m_stats.snapshot_false_positive);
  fprintf(fout, "c2p_snapshot_false_negative = %llu\n", m_stats.snapshot_false_negative);
  fprintf(fout, "c2p_snapshot_updates = %llu\n", m_stats.snapshot_updates);
  fprintf(fout, "c2p_snapshot_rebuilds = %llu\n", m_stats.snapshot_rebuilds);
}
