#include "l2-char-stats.h"

#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <sstream>

l2_char_occ_stats::l2_char_occ_stats() { init(0); }

void l2_char_occ_stats::init(unsigned c, bool sparse) {
  capacity = c;
  unbounded = c == 0;
  sparse_histogram = sparse;
  samples = sum = maximum = nonzero_cycles = full_cycles = 0;
  hist.clear();
  sparse_hist.clear();
  if (!sparse_histogram) hist.assign(unbounded ? 1 : c + 1, 0);
}

void l2_char_occ_stats::sample(unsigned value) {
  if (sparse_histogram) {
    ++sparse_hist[value];
  } else if (unbounded) {
    if (value >= hist.size()) hist.resize(value + 1, 0);
  } else {
    assert(value <= capacity);
  }
  ++samples;
  sum += value;
  maximum = std::max<unsigned long long>(maximum, value);
  if (value) ++nonzero_cycles;
  if (!unbounded && capacity && value == capacity) ++full_cycles;
  if (!sparse_histogram) ++hist[value];
}

unsigned l2_char_occ_stats::percentile(unsigned numerator,
                                        unsigned denominator) const {
  if (!samples) return 0;
  const unsigned long long rank =
      (samples * numerator + denominator - 1) / denominator;
  unsigned long long seen = 0;
  if (sparse_histogram) {
    for (std::map<unsigned, unsigned long long>::const_iterator it =
             sparse_hist.begin(); it != sparse_hist.end(); ++it) {
      seen += it->second;
      if (seen >= rank) return it->first;
    }
    return sparse_hist.empty() ? 0 : sparse_hist.rbegin()->first;
  }
  for (unsigned i = 0; i < hist.size(); ++i) {
    seen += hist[i];
    if (seen >= rank) return i;
  }
  return unbounded ? static_cast<unsigned>(hist.size() - 1) : capacity;
}

double l2_char_occ_stats::average() const {
  return samples ? static_cast<double>(sum) / samples : 0.0;
}
double l2_char_occ_stats::utilization() const {
  return !unbounded && capacity && samples ? static_cast<double>(sum) / (capacity * samples)
                             : 0.0;
}
double l2_char_occ_stats::full_ratio() const {
  return !unbounded && samples ? static_cast<double>(full_cycles) / samples : 0.0;
}

l2_char_block_stats::l2_char_block_stats()
    : eligible_cycles(0), blocked_cycles(0), blocked_requests(0),
      blocking_episodes(0) {}

l2_char_cycle_sample::l2_char_cycle_sample()
    : mshr_entries(0), mshr_ready_entries(0), mshr_targets(0),
      mshr_ready_targets(0), merge_limit_entries(0), max_merge_depth(0),
      missq(0), missq_demand(0), missq_wb(0), missq_other_write(0),
      l2dramq(0), draml2q(0), l2icntq(0), icntl2q(0), rop(0),
      data_port_busy(false), fill_port_busy(false) {}

l2_char_collector::l2_char_collector(
    unsigned slice_id, unsigned sets, unsigned ways, unsigned mshr_entries,
    unsigned merge_limit, unsigned missq_capacity, unsigned l2dramq_capacity,
    unsigned draml2q_capacity, unsigned l2icntq_capacity,
    unsigned icntl2q_capacity, unsigned window_cycles, bool set_detail,
    bool emit_windows)
    : m_slice_id(slice_id), m_sets(sets), m_ways(ways),
      m_window_cycles(window_cycles ? window_cycles : 5000),
      m_set_detail(set_detail), m_emit_windows(emit_windows), m_cycles(0),
      m_window_start(0), m_window_samples(0), m_data_busy(0), m_fill_busy(0),
      m_window_data_busy(0), m_window_fill_busy(0),
      m_max_reserved_ways_any_set(0), m_cycles_any_set_all_reserved(0),
      m_dram_issue_eligible(0), m_dram_issue_returnq(0), m_dram_issue_credit(0),
      m_dram_issue_scheduler(0), m_dram_read_returnq(0),
      m_dram_read_credit(0), m_dram_read_scheduler(0), m_dram_wb_credit(0),
      m_dram_wb_scheduler(0), m_data_accept_hit(0), m_data_accept_dirty(0),
      m_data_accept_other(0), m_wb_requests(0), m_wb_bytes(0),
      m_window_wb_requests(0), m_window_wb_bytes(0),
      m_l2dram_class_error(false) {
  m_reserved.init(sets * ways); m_dirty.init(sets * ways); m_valid.init(sets * ways);
  m_mshr_entries.init(mshr_entries); m_mshr_ready.init(mshr_entries);
  m_mshr_targets.init(mshr_entries * merge_limit);
  m_merge_limit_entries.init(mshr_entries); m_merge_depth.init(merge_limit);
  m_missq.init(missq_capacity); m_missq_demand.init(missq_capacity);
  m_missq_wb.init(missq_capacity); m_missq_other.init(missq_capacity);
  m_l2dramq.init(l2dramq_capacity); m_draml2q.init(draml2q_capacity);
  m_l2icntq.init(l2icntq_capacity); m_icntl2q.init(icntl2q_capacity);
  // ROP delay is modeled as an unbounded production queue.  It is causal
  // context, not a finite-capacity resource, so report occupancy but never a
  // fabricated utilization/full ratio derived from the ICNT input depth.
  m_rop.init(0, true); m_set_reserved_distribution.init(ways);
  m_all_reserved_sets.init(sets);
  m_window_reserved.init(sets * ways); m_window_mshr_entries.init(mshr_entries);
  m_window_mshr_targets.init(mshr_entries * merge_limit); m_window_missq.init(missq_capacity);
  m_window_missq_wb.init(missq_capacity); m_window_l2dramq.init(l2dramq_capacity);
  m_window_draml2q.init(draml2q_capacity);
  m_lifetime_pending.init(0); m_lifetime_drain.init(0); m_lifetime_total.init(0);
  for (unsigned i = 0; i < kQueueClasses; ++i) {
    m_l2dram_class[i] = 0;
    m_l2dram_pushes[i] = m_l2dram_push_bytes[i] = 0;
  }
}

void l2_char_collector::sample_occ(l2_char_occ_stats &all,
                                   l2_char_occ_stats &window, unsigned value) {
  all.sample(value); window.sample(value);
}

void l2_char_collector::observe_mshr_lifetimes(
    unsigned long long cycle, const std::vector<l2_char_mshr_state> &states) {
  std::map<unsigned long long, bool> present;
  for (std::vector<l2_char_mshr_state>::const_iterator it = states.begin();
       it != states.end(); ++it) {
    present[it->address] = true;
    lifetime_state &life = m_lifetimes[it->address];
    if (!life.allocated) { life.alloc_cycle = cycle; life.allocated = true; }
    if (it->ready && !life.ready_seen) {
      life.ready_seen = true;
      life.ready_cycle = cycle;
    }
  }
  for (std::map<unsigned long long, lifetime_state>::iterator it =
           m_lifetimes.begin(); it != m_lifetimes.end();) {
    if (present.find(it->first) != present.end()) { ++it; continue; }
    const lifetime_state life = it->second;
    if (life.ready_seen) {
      m_lifetime_pending.sample(static_cast<unsigned>(life.ready_cycle - life.alloc_cycle));
      m_lifetime_drain.sample(static_cast<unsigned>(cycle - life.ready_cycle));
    }
    m_lifetime_total.sample(static_cast<unsigned>(cycle - life.alloc_cycle));
    m_lifetimes.erase(it++);
  }
}

void l2_char_collector::sample_cycle(unsigned long long cycle,
                                     const l2_char_cycle_sample &s) {
  if (!m_cycles) m_window_start = cycle;
  ++m_cycles; ++m_window_samples;
  sample_occ(m_reserved, m_window_reserved, s.storage.reserved);
  m_dirty.sample(s.storage.dirty); m_valid.sample(s.storage.valid);
  sample_occ(m_mshr_entries, m_window_mshr_entries, s.mshr_entries);
  m_mshr_ready.sample(s.mshr_ready_entries);
  sample_occ(m_mshr_targets, m_window_mshr_targets, s.mshr_targets);
  m_merge_limit_entries.sample(s.merge_limit_entries); m_merge_depth.sample(s.max_merge_depth);
  sample_occ(m_missq, m_window_missq, s.missq);
  m_missq_demand.sample(s.missq_demand); sample_occ(m_missq_wb, m_window_missq_wb, s.missq_wb);
  m_missq_other.sample(s.missq_other_write);
  sample_occ(m_l2dramq, m_window_l2dramq, s.l2dramq);
  sample_occ(m_draml2q, m_window_draml2q, s.draml2q);
  m_l2icntq.sample(s.l2icntq); m_icntl2q.sample(s.icntl2q); m_rop.sample(s.rop);
  if (s.data_port_busy) { ++m_data_busy; ++m_window_data_busy; }
  if (s.fill_port_busy) { ++m_fill_busy; ++m_window_fill_busy; }
  if (m_set_detail)
    for (std::vector<unsigned>::const_iterator it = s.storage.reserved_by_set.begin();
         it != s.storage.reserved_by_set.end(); ++it) m_set_reserved_distribution.sample(*it);
  m_all_reserved_sets.sample(s.storage.all_reserved_sets);
  m_max_reserved_ways_any_set = std::max<unsigned long long>(
      m_max_reserved_ways_any_set, s.storage.max_reserved_set);
  if (s.storage.all_reserved_sets) ++m_cycles_any_set_all_reserved;
  observe_mshr_lifetimes(cycle, s.mshr_states);
  if (m_window_samples == m_window_cycles) close_window(cycle);
}

void l2_char_collector::record_block(l2_char_block_stats &stats, bool eligible,
                                     bool blocked) {
  if (eligible) ++stats.eligible_cycles;
  if (blocked) ++stats.blocked_cycles;
}

void l2_char_collector::record_frontend(mem_fetch *mf, unsigned long long,
                                        unsigned eligible_mask, unsigned blocked_mask) {
  request_state &state = m_frontend_requests[mf];
  for (unsigned i = 0; i < kFrontendReasons; ++i) {
    const unsigned bit = 1u << i;
    const bool eligible = eligible_mask & bit, blocked = blocked_mask & bit;
    record_block(m_frontend[i], eligible, blocked);
    record_block(m_window_frontend[i], eligible, blocked);
    if (blocked && !(state.prev_blocked_mask & bit)) {
      ++m_frontend[i].blocked_requests;
      ++m_frontend[i].blocking_episodes;
    }
  }
  state.prev_blocked_mask = blocked_mask;
}

void l2_char_collector::clear_frontend_request(mem_fetch *mf) {
  m_frontend_requests.erase(mf);
}
void l2_char_collector::record_fill(bool e, bool b) {
  record_block(m_fill, e, b);
  record_block(m_window_fill, e, b);
}
void l2_char_collector::record_rop(bool e, bool b) { record_block(m_rop_block, e, b); }
void l2_char_collector::record_mshr_response(bool e, bool b) { record_block(m_mshr_response, e, b); }
void l2_char_collector::record_lower_drain(bool e, bool b) { record_block(m_lower_drain, e, b); }
void l2_char_collector::record_dram_return(bool e, bool b) { record_block(m_dram_return, e, b); }

void l2_char_collector::record_dram_issue(bool is_read, bool is_wb, bool return_block,
                                          bool credit_block, bool scheduler_block) {
  ++m_dram_issue_eligible;
  if (return_block) ++m_dram_issue_returnq;
  if (credit_block) ++m_dram_issue_credit;
  if (scheduler_block) ++m_dram_issue_scheduler;
  if (is_read) { if (return_block) ++m_dram_read_returnq; if (credit_block) ++m_dram_read_credit; if (scheduler_block) ++m_dram_read_scheduler; }
  if (is_wb) { if (credit_block) ++m_dram_wb_credit; if (scheduler_block) ++m_dram_wb_scheduler; }
}
void l2_char_collector::record_data_port_accept(unsigned source) {
  if (source == 0) ++m_data_accept_hit; else if (source == 1) ++m_data_accept_dirty; else ++m_data_accept_other;
}
void l2_char_collector::record_wb_generated(unsigned bytes) {
  ++m_wb_requests; m_wb_bytes += bytes;
  ++m_window_wb_requests; m_window_wb_bytes += bytes;
}
void l2_char_collector::record_l2dram_push_class(unsigned klass, unsigned bytes) {
  if (klass >= kQueueClasses) { m_l2dram_class_error = true; return; }
  ++m_l2dram_class[klass];
  ++m_l2dram_pushes[klass];
  m_l2dram_push_bytes[klass] += bytes;
}
void l2_char_collector::record_l2dram_pop_class(unsigned klass) {
  if (klass >= kQueueClasses || !m_l2dram_class[klass]) { m_l2dram_class_error = true; return; } --m_l2dram_class[klass];
}

void l2_char_collector::observe_queue_classes(unsigned missq_total,
                                               unsigned missq_demand,
                                               unsigned missq_wb,
                                               unsigned missq_other,
                                               unsigned l2dram_total) {
  if (missq_demand + missq_wb + missq_other != missq_total)
    m_l2dram_class_error = true;
  unsigned l2dram_classes = 0;
  for (unsigned i = 0; i < kQueueClasses; ++i) l2dram_classes += m_l2dram_class[i];
  if (l2dram_classes != l2dram_total) m_l2dram_class_error = true;
}

std::string l2_char_collector::occ_fields(const char *prefix, const l2_char_occ_stats &o) const {
  std::ostringstream s; s.setf(std::ios::fixed); s.precision(6);
  s << '|' << prefix << "_cap=" << o.capacity << '|' << prefix << "_avg=" << o.average()
    << '|' << prefix << "_p50=" << o.percentile(50, 100) << '|' << prefix << "_p95=" << o.percentile(95, 100)
    << '|' << prefix << "_max=" << o.maximum << '|' << prefix << "_util_avg=" << o.utilization()
    << '|' << prefix << "_full_ratio=" << o.full_ratio(); return s.str();
}
std::string l2_char_collector::block_fields(const char *prefix, const l2_char_block_stats &b) const {
  std::ostringstream s; s.setf(std::ios::fixed); s.precision(6);
  const double ratio = b.eligible_cycles ? static_cast<double>(b.blocked_cycles) / b.eligible_cycles : 0.0;
  s << '|' << prefix << "_eligible=" << b.eligible_cycles << '|' << prefix << "_blocked=" << b.blocked_cycles
    << '|' << prefix << "_requests=" << b.blocked_requests << '|' << prefix << "_episodes=" << b.blocking_episodes
    << '|' << prefix << "_ratio=" << ratio; return s.str();
}
std::string l2_char_collector::hist_record(const char *metric,
                                            const l2_char_occ_stats &o) const {
  std::ostringstream s;
  s << "L2CHARV1|HIST|slice=" << m_slice_id << "|metric=" << metric
    << "|capacity=" << o.capacity << "|unbounded=" << (o.unbounded ? 1 : 0)
    << "|encoding=" << (o.sparse_histogram ? "sparse" : "dense")
    << "|samples=" << o.samples << "|bins=";
  if (o.sparse_histogram) {
    for (std::map<unsigned, unsigned long long>::const_iterator it =
             o.sparse_hist.begin(); it != o.sparse_hist.end(); ++it) {
      if (it != o.sparse_hist.begin()) s << ',';
      s << it->first << ':' << it->second;
    }
    return s.str();
  }
  for (unsigned i = 0; i < o.hist.size(); ++i) {
    if (i) s << ',';
    s << o.hist[i];
  }
  return s.str();
}

void l2_char_collector::close_window(unsigned long long end_cycle) {
  if (!m_emit_windows || !m_window_samples) return;
  std::ostringstream s; s.setf(std::ios::fixed); s.precision(6);
  s << "L2CHARV1|WINDOW|slice=" << m_slice_id << "|window=" << m_windows.size()
    << "|start_l2_cycle=" << m_window_start << "|end_l2_cycle=" << end_cycle
    << "|samples=" << m_window_samples
    << "|reserved_avg=" << m_window_reserved.average() << "|reserved_max=" << m_window_reserved.maximum
    << "|mshr_avg=" << m_window_mshr_entries.average() << "|mshr_max=" << m_window_mshr_entries.maximum
    << "|mshr_target_avg=" << m_window_mshr_targets.average() << "|merge_depth_max=" << m_merge_depth.maximum
    << "|missq_avg=" << m_window_missq.average() << "|missq_wb_avg=" << m_window_missq_wb.average()
    << "|l2dramq_avg=" << m_window_l2dramq.average() << "|draml2q_avg=" << m_window_draml2q.average()
    << "|fill_port_busy_ratio=" << (static_cast<double>(m_window_fill_busy) / m_window_samples)
    << "|data_port_busy_ratio=" << (static_cast<double>(m_window_data_busy) / m_window_samples)
    << "|wb_generated=" << m_window_wb_requests << "|wb_bytes=" << m_window_wb_bytes
    << block_fields("fill", m_window_fill)
    << block_fields("block_set", m_window_frontend[0])
    << block_fields("block_mshr_new", m_window_frontend[1])
    << block_fields("block_mshr_merge", m_window_frontend[2])
    << block_fields("block_missq", m_window_frontend[3])
    << block_fields("block_dataport", m_window_frontend[4])
    << block_fields("block_respq", m_window_frontend[5]);
  m_windows.push_back(s.str());
  m_window_samples = m_window_data_busy = m_window_fill_busy = 0;
  m_window_wb_requests = m_window_wb_bytes = 0;
  m_window_fill = l2_char_block_stats();
  for (unsigned i = 0; i < kFrontendReasons; ++i)
    m_window_frontend[i] = l2_char_block_stats();
  m_window_reserved.init(m_reserved.capacity); m_window_mshr_entries.init(m_mshr_entries.capacity);
  m_window_mshr_targets.init(m_mshr_targets.capacity); m_window_missq.init(m_missq.capacity);
  m_window_missq_wb.init(m_missq_wb.capacity); m_window_l2dramq.init(m_l2dramq.capacity);
  m_window_draml2q.init(m_draml2q.capacity); m_window_start = end_cycle + 1;
}

bool l2_char_collector::invariants_hold(
    std::string *why, unsigned long long native_data_busy,
    unsigned long long native_fill_busy,
    unsigned long long native_port_samples) const {
  if (m_l2dram_class_error) { if (why) *why = "l2dram_class_accounting"; return false; }
  for (unsigned i = 0; i < kFrontendReasons; ++i)
    if (m_frontend[i].blocked_cycles > m_frontend[i].eligible_cycles) { if (why) *why = "frontend_block_denominator"; return false; }
  if (m_fill.blocked_cycles > m_fill.eligible_cycles || m_rop_block.blocked_cycles > m_rop_block.eligible_cycles ||
      m_mshr_response.blocked_cycles > m_mshr_response.eligible_cycles ||
      m_lower_drain.blocked_cycles > m_lower_drain.eligible_cycles ||
      m_dram_return.blocked_cycles > m_dram_return.eligible_cycles) { if (why) *why = "causal_block_denominator"; return false; }
  if (!m_lifetimes.empty()) { if (why) *why = "mshr_lifetime_tracker"; return false; }
  if (m_data_busy != native_data_busy) {
    if (why) *why = "data_port_busy_snapshot";
    return false;
  }
  if (m_fill_busy != native_fill_busy) {
    if (why) *why = "fill_port_busy_snapshot";
    return false;
  }
  if (m_cycles != native_port_samples) {
    if (why) *why = "port_sample_count";
    return false;
  }
  if (why) *why = "ok";
  return true;
}

void l2_char_collector::print(FILE *fp, unsigned long long native_data_busy,
                              unsigned long long native_fill_busy,
                              unsigned long long native_port_samples) const {
  l2_char_collector *self = const_cast<l2_char_collector *>(this);
  if (self->m_window_samples) self->close_window(self->m_window_start + self->m_window_samples - 1);
  std::string why;
  const bool ok = invariants_hold(&why, native_data_busy, native_fill_busy,
                                  native_port_samples);
  const l2_char_occ_stats &set_hist = m_set_reserved_distribution;
  const unsigned half = (m_ways + 1) / 2;
  unsigned long long set_zero = set_hist.hist.empty() ? 0 : set_hist.hist[0];
  unsigned long long set_half = 0, set_all = 0;
  for (unsigned i = 0; i < set_hist.hist.size(); ++i) {
    if (i >= half) set_half += set_hist.hist[i];
    if (i >= m_ways) set_all += set_hist.hist[i];
  }
  std::ostringstream slice;
  slice << "L2CHARV1|SLICE|slice=" << m_slice_id << "|cycles=" << m_cycles
        << occ_fields("reserved", m_reserved) << occ_fields("dirty", m_dirty)
        << occ_fields("valid", m_valid) << occ_fields("mshr", m_mshr_entries)
        << occ_fields("mshr_ready", m_mshr_ready) << occ_fields("mshr_target", m_mshr_targets)
        << occ_fields("merge_limit_entries", m_merge_limit_entries) << occ_fields("merge_depth", m_merge_depth)
        << occ_fields("missq", m_missq) << occ_fields("missq_demand", m_missq_demand)
        << occ_fields("missq_wb", m_missq_wb) << occ_fields("missq_other", m_missq_other)
        << occ_fields("l2dramq", m_l2dramq) << occ_fields("draml2q", m_draml2q)
        << occ_fields("l2icntq", m_l2icntq) << occ_fields("icntl2q", m_icntl2q)
        << occ_fields("rop", m_rop) << occ_fields("set_reserved", m_set_reserved_distribution)
        << "|max_reserved_ways_any_set=" << m_max_reserved_ways_any_set
        << "|sets_all_ways_reserved_avg=" << m_all_reserved_sets.average()
        << "|sets_all_ways_reserved_max=" << m_all_reserved_sets.maximum
        << "|cycles_any_set_all_reserved=" << m_cycles_any_set_all_reserved
        << "|set_reserved_fraction_0=" << (set_hist.samples ? static_cast<double>(set_zero) / set_hist.samples : 0.0)
        << "|set_reserved_fraction_1plus=" << (set_hist.samples ? static_cast<double>(set_hist.samples - set_zero) / set_hist.samples : 0.0)
        << "|set_reserved_fraction_half_or_more=" << (set_hist.samples ? static_cast<double>(set_half) / set_hist.samples : 0.0)
        << "|set_reserved_fraction_all=" << (set_hist.samples ? static_cast<double>(set_all) / set_hist.samples : 0.0)
        << block_fields("block_set", m_frontend[0]) << block_fields("block_mshr_new", m_frontend[1])
        << block_fields("block_mshr_merge", m_frontend[2]) << block_fields("block_missq", m_frontend[3])
        << block_fields("block_dataport", m_frontend[4]) << block_fields("block_respq", m_frontend[5])
        << block_fields("block_mshr_rw_pending", m_frontend[6]);
  fprintf(fp, "%s\n", slice.str().c_str());
  fprintf(fp, "L2CHARV1|SLICE_DETAIL|slice=%u|data_busy_ratio=%.6f|fill_busy_ratio=%.6f|char_data_busy_cycles=%llu|native_data_busy_cycles=%llu|char_fill_busy_cycles=%llu|native_fill_busy_cycles=%llu|char_port_samples=%llu|native_port_samples=%llu|data_accept_hit=%llu|data_accept_dirty=%llu|data_accept_other=%llu|wb_requests=%llu|wb_bytes=%llu|l2dram_requests=%llu|l2dram_bytes=%llu|l2dram_wb_requests=%llu|l2dram_wb_bytes=%llu%s%s%s%s%s|dram_issue_eligible=%llu|dram_issue_returnq=%llu|dram_issue_credit=%llu|dram_issue_scheduler=%llu|dram_read_returnq=%llu|dram_read_credit=%llu|dram_read_scheduler=%llu|dram_wb_credit=%llu|dram_wb_scheduler=%llu%s%s%s\n",
          m_slice_id, m_cycles ? static_cast<double>(m_data_busy) / m_cycles : 0.0,
          m_cycles ? static_cast<double>(m_fill_busy) / m_cycles : 0.0,
          m_data_busy, native_data_busy, m_fill_busy, native_fill_busy,
          m_cycles, native_port_samples, m_data_accept_hit, m_data_accept_dirty,
          m_data_accept_other, m_wb_requests, m_wb_bytes,
          m_l2dram_pushes[0] + m_l2dram_pushes[1] + m_l2dram_pushes[2] + m_l2dram_pushes[3],
          m_l2dram_push_bytes[0] + m_l2dram_push_bytes[1] + m_l2dram_push_bytes[2] + m_l2dram_push_bytes[3],
          m_l2dram_pushes[1], m_l2dram_push_bytes[1], block_fields("fill", m_fill).c_str(), block_fields("rop_input", m_rop_block).c_str(),
          block_fields("mshr_response", m_mshr_response).c_str(), block_fields("lower_drain", m_lower_drain).c_str(),
          block_fields("dram_return", m_dram_return).c_str(), m_dram_issue_eligible, m_dram_issue_returnq,
          m_dram_issue_credit, m_dram_issue_scheduler, m_dram_read_returnq, m_dram_read_credit,
          m_dram_read_scheduler, m_dram_wb_credit, m_dram_wb_scheduler,
          occ_fields("mshr_lifetime_pending", m_lifetime_pending).c_str(), occ_fields("mshr_lifetime_drain", m_lifetime_drain).c_str(),
          occ_fields("mshr_lifetime_total", m_lifetime_total).c_str());
  fprintf(fp, "%s\n", hist_record("reserved", m_reserved).c_str());
  fprintf(fp, "%s\n", hist_record("mshr", m_mshr_entries).c_str());
  fprintf(fp, "%s\n", hist_record("mshr_target", m_mshr_targets).c_str());
  fprintf(fp, "%s\n", hist_record("merge_depth", m_merge_depth).c_str());
  fprintf(fp, "%s\n", hist_record("missq", m_missq).c_str());
  fprintf(fp, "%s\n", hist_record("missq_wb", m_missq_wb).c_str());
  fprintf(fp, "%s\n", hist_record("icntl2q", m_icntl2q).c_str());
  fprintf(fp, "%s\n", hist_record("l2dramq", m_l2dramq).c_str());
  fprintf(fp, "%s\n", hist_record("draml2q", m_draml2q).c_str());
  fprintf(fp, "%s\n", hist_record("l2icntq", m_l2icntq).c_str());
  fprintf(fp, "%s\n", hist_record("rop", m_rop).c_str());
  for (std::vector<std::string>::const_iterator it = m_windows.begin(); it != m_windows.end(); ++it) fprintf(fp, "%s\n", it->c_str());
  fprintf(fp, "L2CHARV1|INVARIANT|slice=%u|status=%s|reason=%s|l2dram_class_sum=%u|char_data_busy_cycles=%llu|native_data_busy_cycles=%llu|char_fill_busy_cycles=%llu|native_fill_busy_cycles=%llu|char_port_samples=%llu|native_port_samples=%llu\n",
          m_slice_id, ok ? "PASS" : "FAIL", why.c_str(),
          m_l2dram_class[0] + m_l2dram_class[1] + m_l2dram_class[2] + m_l2dram_class[3],
          m_data_busy, native_data_busy, m_fill_busy, native_fill_busy,
          m_cycles, native_port_samples);
}
