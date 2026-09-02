// Decoupled-Tag L1 common M1 model helpers.
//
// This header deliberately has no dependency on cache implementation classes.
// It gives Base/IO/OO one definition of a 128B logical reference and of the
// bounded paper front-end resources.  LEGACY code never constructs this model.

#ifndef DTC_L1_COMMON_H
#define DTC_L1_COMMON_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <deque>
#include <set>
#include <vector>

namespace dtc_l1 {

constexpr uint64_t kLogicalLineBytes = 128;
constexpr unsigned kSectorsPerLogicalLine = 4;

enum class mode { LEGACY, PAPER_BASE, PAPER_IO, PAPER_OO, MODERN_OO_SECTOR };

struct config {
  mode selected_mode = mode::LEGACY;
  unsigned pib_entries = 8;
  unsigned tag_banks = 4;
  unsigned tag_requests_per_bank_per_cycle = 1;
  unsigned tag_requests_per_cycle = 4;
  unsigned coalescer_threads_per_cycle = 32;
  unsigned logical_sets = 32;
  unsigned logical_ways = 4;
  unsigned physical_lines = 640;
  unsigned allocation_width = 4;
  unsigned io_pib_entries = 256;
};

enum class io_access_kind { VALID_HIT, PENDING_HIT, NEW_MISS, NO_FREE_LINE };

struct physical_identity {
  unsigned id = 0;
  uint64_t generation = 0;
};

struct io_access_result {
  io_access_kind kind = io_access_kind::NO_FREE_LINE;
  physical_identity physical;
};

// Deterministic whole-line IO-DTC model used by M2 directed tests and by the
// later LD/ST integration.  It deliberately owns no conventional MSHR state.
class io_frontend {
 public:
  explicit io_frontend(config cfg) : m_cfg(cfg), m_tags(cfg.logical_sets * cfg.logical_ways), m_phys(cfg.physical_lines) {
    assert(cfg.logical_sets && cfg.logical_ways && cfg.physical_lines);
    assert(cfg.allocation_width && cfg.io_pib_entries);
    assert(cfg.tag_banks && cfg.tag_requests_per_bank_per_cycle &&
           cfg.tag_requests_per_cycle);
    m_minimum_free_lines = m_phys.size();
  }

  void begin_cycle(uint64_t cycle) {
    if (cycle == m_cycle) return;
    m_cycle = cycle;
    m_allocations_this_cycle = 0;
  }

  bool admit(uint64_t uid) {
    if (find_entry(uid)) return true;
    if (m_entries.size() >= m_cfg.io_pib_entries) return false;
    m_entries.push_back({uid, {}, {}, false, 0});
    return true;
  }

  // IO owns its data/merge state, but it obeys the same frozen logical Tag
  // service contract as Paper Base: one reference per bank and four total at
  // the default configuration. This is intentionally independent of the
  // conventional L1D Tag/MSHR implementation.
  bool try_serve_tag(uint64_t cycle, uint64_t address) {
    if (cycle != m_tag_cycle) {
      m_tag_cycle = cycle;
      std::fill(m_tag_requests_this_cycle.begin(),
                m_tag_requests_this_cycle.end(), 0);
      m_total_tag_requests_this_cycle = 0;
    }
    const unsigned logical_set = static_cast<unsigned>(
        (address / kLogicalLineBytes) % m_cfg.logical_sets);
    const unsigned bank = logical_set % m_cfg.tag_banks;
    if (m_total_tag_requests_this_cycle >= m_cfg.tag_requests_per_cycle ||
        m_tag_requests_this_cycle[bank] >=
            m_cfg.tag_requests_per_bank_per_cycle) {
      ++m_tag_conflicts;
      return false;
    }
    ++m_total_tag_requests_this_cycle;
    ++m_tag_requests_this_cycle[bank];
    ++m_tag_requests;
    ++m_tag_requests_per_bank[bank];
    return true;
  }

  io_access_result access(uint64_t cycle, uint64_t uid, uint64_t address) {
    begin_cycle(cycle);
    entry *owner = find_entry(uid);
    assert(owner);
    const uint64_t line = address & ~(kLogicalLineBytes - 1);
    tag_entry *tag = find_tag(line);
    if (tag) {
      if (owner->has_unresolved_line && owner->unresolved_line == line)
        owner->has_unresolved_line = false;
      if (!has_reference(*owner, tag->physical))
        owner->references.push_back(tag->physical);
      const bool ready = m_phys[tag->physical.id].ready;
      if (ready)
        ++m_valid_hits;
      else
        ++m_pending_hits;
      return {ready ? io_access_kind::VALID_HIT : io_access_kind::PENDING_HIT,
              tag->physical};
    }
    if (m_allocations_this_cycle >= m_cfg.allocation_width) {
      if (!owner->has_unresolved_line) ++m_partial_allocation_events;
      ++m_allocation_width_limited_events;
      owner->has_unresolved_line = true;
      owner->unresolved_line = line;
      sample_resource_extrema();
      return {io_access_kind::NO_FREE_LINE, {}};
    }
    const int free_id = find_free_physical();
    if (free_id < 0) {
      if (!owner->has_unresolved_line) ++m_partial_allocation_events;
      ++m_no_free_physical_events;
      owner->has_unresolved_line = true;
      owner->unresolved_line = line;
      sample_resource_extrema();
      return {io_access_kind::NO_FREE_LINE, {}};
    }
    tag_entry *victim = select_victim(line);
    if (victim->valid) {
      ++m_tag_evictions;
      if (!m_phys[victim->physical.id].ready)
        m_evicted_pending_lines[victim->line] = victim->physical;
      owner->release_on_retire.push_back(victim->physical);
    }
    // Count a duplicate only while the original evicted allocation is still
    // pending.  A later re-access after that response has completed is an
    // ordinary cold reallocation, not duplicate in-flight lower traffic.
    if (m_evicted_pending_lines.erase(line)) ++m_duplicate_after_eviction;
    physical_identity identity{static_cast<unsigned>(free_id), ++m_phys[free_id].generation};
    m_phys[free_id].allocated = true;
    m_phys[free_id].ready = false;
    *victim = {true, line, identity, ++m_lru_clock};
    owner->references.push_back(identity);
    if (owner->has_unresolved_line && owner->unresolved_line == line)
      owner->has_unresolved_line = false;
    ++m_allocations_this_cycle;
    ++m_new_misses;
    sample_resource_extrema();
    return {io_access_kind::NEW_MISS, identity};
  }

  void complete(physical_identity identity) {
    assert(identity.id < m_phys.size());
    physical_line &line = m_phys[identity.id];
    assert(line.allocated && line.generation == identity.generation);
    line.ready = true;
    for (auto it = m_evicted_pending_lines.begin();
         it != m_evicted_pending_lines.end();) {
      if (it->second.id == identity.id &&
          it->second.generation == identity.generation)
        it = m_evicted_pending_lines.erase(it);
      else
        ++it;
    }
  }

  bool head_ready() const {
    return !m_entries.empty() && entry_ready(m_entries.front());
  }

  uint64_t head_uid() const {
    assert(!m_entries.empty());
    return m_entries.front().uid;
  }

  bool retire_head(uint64_t expected_uid) {
    assert(!m_entries.empty() && m_entries.front().uid == expected_uid);
    if (m_entries.empty() || !entry_ready(m_entries.front())) return false;
    for (const physical_identity identity : m_entries.front().release_on_retire)
      release(identity);
    m_entries.pop_front();
    ++m_retires;
    return true;
  }

  bool retire_head() { return head_ready() && retire_head(head_uid()); }

  size_t occupancy() const { return m_entries.size(); }
  uint64_t new_misses() const { return m_new_misses; }
  uint64_t retires() const { return m_retires; }
  uint64_t tag_evictions() const { return m_tag_evictions; }
  uint64_t duplicate_after_eviction() const { return m_duplicate_after_eviction; }
  uint64_t partial_allocation_events() const { return m_partial_allocation_events; }
  uint64_t valid_hits() const { return m_valid_hits; }
  uint64_t pending_hits() const { return m_pending_hits; }
  uint64_t releases() const { return m_releases; }
  uint64_t allocation_width_limited_events() const {
    return m_allocation_width_limited_events;
  }
  uint64_t no_free_physical_events() const { return m_no_free_physical_events; }
  uint64_t tag_requests() const { return m_tag_requests; }
  uint64_t tag_conflicts() const { return m_tag_conflicts; }
  const std::vector<uint64_t> &tag_requests_per_bank() const {
    return m_tag_requests_per_bank;
  }
  size_t allocated_lines() const { return m_phys.size() - free_lines(); }
  size_t allocated_lines_peak() const { return m_allocated_lines_peak; }
  size_t minimum_free_lines() const { return m_minimum_free_lines; }
  size_t partial_entries() const {
    size_t result = 0;
    for (const entry &e : m_entries) result += e.has_unresolved_line;
    return result;
  }
  size_t partial_entries_peak() const { return m_partial_entries_peak; }
  size_t partial_lines_held() const {
    size_t result = 0;
    for (const entry &e : m_entries)
      if (e.has_unresolved_line) result += e.references.size();
    return result;
  }
  size_t partial_lines_held_peak() const { return m_partial_lines_held_peak; }
  bool younger_ready_exists() const {
    if (m_entries.size() < 2) return false;
    for (size_t i = 1; i < m_entries.size(); ++i)
      if (entry_ready(m_entries[i])) return true;
    return false;
  }
  size_t ready_younger_count() const {
    size_t result = 0;
    for (size_t i = 1; i < m_entries.size(); ++i)
      result += entry_ready(m_entries[i]);
    return result;
  }
  size_t free_lines() const {
    size_t result = 0;
    for (const physical_line &line : m_phys) result += !line.allocated;
    return result;
  }

 private:
  struct physical_line { bool allocated = false; bool ready = false; uint64_t generation = 0; };
  struct tag_entry { bool valid = false; uint64_t line = 0; physical_identity physical; uint64_t lru = 0; };
  struct entry { uint64_t uid; std::vector<physical_identity> references; std::vector<physical_identity> release_on_retire; bool has_unresolved_line; uint64_t unresolved_line; };
  entry *find_entry(uint64_t uid) { for (entry &e : m_entries) if (e.uid == uid) return &e; return nullptr; }
  static bool has_reference(const entry &e, physical_identity identity) {
    for (const physical_identity &existing : e.references)
      if (existing.id == identity.id && existing.generation == identity.generation)
        return true;
    return false;
  }
  void sample_resource_extrema() {
    m_allocated_lines_peak = std::max(m_allocated_lines_peak, allocated_lines());
    m_minimum_free_lines = std::min(m_minimum_free_lines, free_lines());
    m_partial_entries_peak = std::max(m_partial_entries_peak, partial_entries());
    m_partial_lines_held_peak =
        std::max(m_partial_lines_held_peak, partial_lines_held());
  }
  tag_entry *find_tag(uint64_t line) {
    const unsigned set = static_cast<unsigned>((line / kLogicalLineBytes) % m_cfg.logical_sets);
    for (unsigned way = 0; way < m_cfg.logical_ways; ++way) { tag_entry &t = m_tags[set * m_cfg.logical_ways + way]; if (t.valid && t.line == line) { t.lru = ++m_lru_clock; return &t; } }
    return nullptr;
  }
  tag_entry *select_victim(uint64_t line) {
    const unsigned set = static_cast<unsigned>((line / kLogicalLineBytes) % m_cfg.logical_sets);
    tag_entry *victim = &m_tags[set * m_cfg.logical_ways];
    for (unsigned way = 0; way < m_cfg.logical_ways; ++way) { tag_entry &t = m_tags[set * m_cfg.logical_ways + way]; if (!t.valid) return &t; if (t.lru < victim->lru) victim = &t; }
    return victim;
  }
  int find_free_physical() {
    for (unsigned offset = 0; offset < m_phys.size(); ++offset) { const unsigned id = (m_rr_next + offset) % m_phys.size(); if (!m_phys[id].allocated) { m_rr_next = (id + 1) % m_phys.size(); return id; } }
    return -1;
  }
  bool entry_ready(const entry &e) const { if (e.has_unresolved_line) return false; for (const physical_identity id : e.references) if (!m_phys[id.id].ready || m_phys[id.id].generation != id.generation) return false; return true; }
  void release(physical_identity identity) { physical_line &line = m_phys[identity.id]; assert(line.generation == identity.generation); line.allocated = false; line.ready = false; ++m_releases; sample_resource_extrema(); }
  config m_cfg; std::vector<tag_entry> m_tags; std::vector<physical_line> m_phys; std::deque<entry> m_entries;
  uint64_t m_cycle = UINT64_MAX, m_lru_clock = 0, m_new_misses = 0, m_retires = 0;
  uint64_t m_tag_evictions = 0, m_duplicate_after_eviction = 0,
           m_partial_allocation_events = 0, m_valid_hits = 0,
           m_pending_hits = 0, m_releases = 0,
           m_allocation_width_limited_events = 0,
           m_no_free_physical_events = 0, m_tag_requests = 0,
           m_tag_conflicts = 0, m_tag_cycle = UINT64_MAX;
  size_t m_allocated_lines_peak = 0, m_minimum_free_lines = 0,
         m_partial_entries_peak = 0, m_partial_lines_held_peak = 0;
  std::map<uint64_t, physical_identity> m_evicted_pending_lines;
  unsigned m_allocations_this_cycle = 0, m_rr_next = 0;
  std::vector<unsigned> m_tag_requests_this_cycle =
      std::vector<unsigned>(m_cfg.tag_banks, 0);
  std::vector<uint64_t> m_tag_requests_per_bank =
      std::vector<uint64_t>(m_cfg.tag_banks, 0);
  unsigned m_total_tag_requests_this_cycle = 0;
};

// A value snapshot lets the normal SM/cluster aggregation path emit a compact
// kernel summary without exposing mutable front-end state.
struct paper_frontend_stats {
  uint64_t admits = 0;
  uint64_t retires = 0;
  uint64_t tag_requests = 0;
  uint64_t tag_conflicts = 0;
  uint64_t pib_full_events = 0;
  uint64_t pib_full_stall_cycles = 0;
  uint64_t tag_conflict_stall_cycles = 0;
  // Kept separate from primary accounting so later stages can observe several
  // unavailable resources in one cycle without changing the primary reason.
  uint64_t nonexclusive_pib_full_cycles = 0;
  uint64_t nonexclusive_tag_conflict_cycles = 0;
  uint64_t frontend_stall_cycles = 0;
  uint64_t pib_occupancy_cycle_sum = 0;
  uint64_t pib_occupancy_sample_cycles = 0;
  uint64_t pib_occupancy = 0;
  uint64_t pib_peak = 0;
  std::vector<uint64_t> requests_per_bank;
  uint64_t io_lower_created = 0;
  uint64_t io_lower_issued = 0;
  uint64_t io_lower_responses = 0;
  uint64_t io_inflight_current = 0;
  uint64_t io_inflight_peak = 0;
  uint64_t io_inflight_identity_mismatch = 0;
  uint64_t io_responses_routed_dtc = 0;
  uint64_t io_responses_routed_conventional = 0;
  uint64_t io_pib_occupancy = 0;
  uint64_t io_pib_peak = 0;
  uint64_t io_pib_head_ready_cycles = 0;
  uint64_t io_head_not_ready_cycles = 0;
  uint64_t io_retire_count = 0;
  uint64_t io_ready_but_writeback_blocked_cycles = 0;
  uint64_t io_completion_dependencies = 0;
  uint64_t io_completion_dependencies_closed = 0;
  uint64_t io_valid_hits = 0;
  uint64_t io_pending_hits = 0;
  uint64_t io_physical_allocations = 0;
  uint64_t io_physical_releases = 0;
  uint64_t io_tag_evictions = 0;
  uint64_t io_duplicate_after_eviction = 0;
  uint64_t io_partial_allocation_events = 0;
  uint64_t io_allocation_width_limited_events = 0;
  uint64_t io_no_free_physical_events = 0;
  uint64_t io_physical_allocated_current = 0;
  uint64_t io_physical_allocated_peak = 0;
  uint64_t io_physical_free_current = 0;
  uint64_t io_physical_free_minimum = 0;
  uint64_t io_partial_entries_current = 0;
  uint64_t io_partial_entries_peak = 0;
  uint64_t io_partial_lines_held_current = 0;
  uint64_t io_partial_lines_held_peak = 0;
  uint64_t io_hol_ready_younger_cycles = 0;
  uint64_t io_hol_ready_younger_count_sum = 0;
  uint64_t io_hol_ready_younger_peak = 0;
  uint64_t io_tag_requests = 0;
  uint64_t io_tag_conflicts = 0;
  std::vector<uint64_t> io_tag_requests_per_bank;

  void add(const paper_frontend_stats &other) {
    admits += other.admits;
    retires += other.retires;
    tag_requests += other.tag_requests;
    tag_conflicts += other.tag_conflicts;
    pib_full_events += other.pib_full_events;
    pib_full_stall_cycles += other.pib_full_stall_cycles;
    tag_conflict_stall_cycles += other.tag_conflict_stall_cycles;
    nonexclusive_pib_full_cycles += other.nonexclusive_pib_full_cycles;
    nonexclusive_tag_conflict_cycles += other.nonexclusive_tag_conflict_cycles;
    frontend_stall_cycles += other.frontend_stall_cycles;
    pib_occupancy_cycle_sum += other.pib_occupancy_cycle_sum;
    pib_occupancy_sample_cycles += other.pib_occupancy_sample_cycles;
    pib_occupancy += other.pib_occupancy;
    pib_peak = std::max(pib_peak, other.pib_peak);
    if (requests_per_bank.size() < other.requests_per_bank.size()) {
      requests_per_bank.resize(other.requests_per_bank.size(), 0);
    }
    for (size_t bank = 0; bank < other.requests_per_bank.size(); ++bank) {
      requests_per_bank[bank] += other.requests_per_bank[bank];
    }
    io_lower_created += other.io_lower_created;
    io_lower_issued += other.io_lower_issued;
    io_lower_responses += other.io_lower_responses;
    io_inflight_current += other.io_inflight_current;
    io_inflight_peak = std::max(io_inflight_peak, other.io_inflight_peak);
    io_inflight_identity_mismatch += other.io_inflight_identity_mismatch;
    io_responses_routed_dtc += other.io_responses_routed_dtc;
    io_responses_routed_conventional += other.io_responses_routed_conventional;
    io_pib_occupancy += other.io_pib_occupancy;
    io_pib_peak = std::max(io_pib_peak, other.io_pib_peak);
    io_pib_head_ready_cycles += other.io_pib_head_ready_cycles;
    io_head_not_ready_cycles += other.io_head_not_ready_cycles;
    io_retire_count += other.io_retire_count;
    io_ready_but_writeback_blocked_cycles +=
        other.io_ready_but_writeback_blocked_cycles;
    io_completion_dependencies += other.io_completion_dependencies;
    io_completion_dependencies_closed +=
        other.io_completion_dependencies_closed;
    io_valid_hits += other.io_valid_hits;
    io_pending_hits += other.io_pending_hits;
    io_physical_allocations += other.io_physical_allocations;
    io_physical_releases += other.io_physical_releases;
    io_tag_evictions += other.io_tag_evictions;
    io_duplicate_after_eviction += other.io_duplicate_after_eviction;
    io_partial_allocation_events += other.io_partial_allocation_events;
    io_allocation_width_limited_events += other.io_allocation_width_limited_events;
    io_no_free_physical_events += other.io_no_free_physical_events;
    io_physical_allocated_current += other.io_physical_allocated_current;
    io_physical_allocated_peak =
        std::max(io_physical_allocated_peak, other.io_physical_allocated_peak);
    io_physical_free_current += other.io_physical_free_current;
    if (!io_physical_free_minimum ||
        (other.io_physical_free_minimum &&
         other.io_physical_free_minimum < io_physical_free_minimum))
      io_physical_free_minimum = other.io_physical_free_minimum;
    io_partial_entries_current += other.io_partial_entries_current;
    io_partial_entries_peak =
        std::max(io_partial_entries_peak, other.io_partial_entries_peak);
    io_partial_lines_held_current += other.io_partial_lines_held_current;
    io_partial_lines_held_peak = std::max(io_partial_lines_held_peak,
                                           other.io_partial_lines_held_peak);
    io_hol_ready_younger_cycles += other.io_hol_ready_younger_cycles;
    io_hol_ready_younger_count_sum += other.io_hol_ready_younger_count_sum;
    io_hol_ready_younger_peak =
        std::max(io_hol_ready_younger_peak, other.io_hol_ready_younger_peak);
    io_tag_requests += other.io_tag_requests;
    io_tag_conflicts += other.io_tag_conflicts;
    if (io_tag_requests_per_bank.size() <
        other.io_tag_requests_per_bank.size())
      io_tag_requests_per_bank.resize(other.io_tag_requests_per_bank.size(), 0);
    for (size_t bank = 0; bank < other.io_tag_requests_per_bank.size(); ++bank)
      io_tag_requests_per_bank[bank] += other.io_tag_requests_per_bank[bank];
  }
};

struct sector_access {
  uint64_t address = 0;
  // One bit for each 32B sector touched by this existing coalesced access.
  uint8_t sector_mask = 0;
};

struct line_reference {
  uint64_t line_address = 0;
  uint8_t sector_mask = 0;
};

// Groups already-coalesced accesses.  It intentionally does not inspect lanes
// or create transactions: that remains the upstream coalescer's job.
inline std::vector<line_reference> group_128b_references(
    const std::vector<sector_access> &accesses) {
  std::map<uint64_t, uint8_t> grouped;
  for (const sector_access &access : accesses) {
    const uint64_t line = access.address & ~(kLogicalLineBytes - 1);
    const unsigned sector = (access.address >> 5) & (kSectorsPerLogicalLine - 1);
    const uint8_t canonical_mask = access.sector_mask
                                       ? access.sector_mask
                                       : static_cast<uint8_t>(1U << sector);
    assert((canonical_mask & ~0xFU) == 0);
    grouped[line] |= canonical_mask;
  }
  std::vector<line_reference> result;
  result.reserve(grouped.size());
  for (const auto &entry : grouped) {
    result.push_back({entry.first, entry.second});
  }
  return result;
}

// A deterministic, bounded M1 front-end model.  The caller owns dynamic
// instruction completion; this class only owns admission and Tag scheduling.
class paper_frontend {
 public:
  explicit paper_frontend(config cfg) : m_cfg(cfg) {
    assert(m_cfg.pib_entries > 0);
    assert(m_cfg.tag_banks > 0);
    assert(m_cfg.tag_requests_per_bank_per_cycle > 0);
    assert(m_cfg.tag_requests_per_cycle > 0);
  }

  bool enabled() const { return m_cfg.selected_mode != mode::LEGACY; }

  bool try_admit(uint64_t dynamic_instruction_id) {
    if (!enabled()) return true;
    if (m_live_instructions.count(dynamic_instruction_id)) return true;
    if (m_live_instructions.size() >= m_cfg.pib_entries) {
      ++m_pib_full_events;
      ++m_pib_full_stall_cycles;
      ++m_nonexclusive_pib_full_cycles;
      ++m_frontend_stall_cycles;
      return false;
    }
    m_live_instructions.insert(dynamic_instruction_id);
    ++m_admits;
    m_pib_peak = std::max(m_pib_peak, m_live_instructions.size());
    return true;
  }

  void retire(uint64_t dynamic_instruction_id) {
    if (!enabled()) return;
    const size_t erased = m_live_instructions.erase(dynamic_instruction_id);
    assert(erased == 1 && "DTC PIB release requires one admitted instruction");
    ++m_retires;
  }

  void begin_cycle(uint64_t cycle) {
    if (cycle == m_cycle) return;
    m_cycle = cycle;
    m_total_served_this_cycle = 0;
    std::fill(m_served_per_bank.begin(), m_served_per_bank.end(), 0);
  }

  // Must be called once per modeled LD/ST cycle so occupancy does not depend
  // on whether that cycle happens to contain Tag work.
  void sample_cycle(uint64_t cycle) {
    if (!enabled() || cycle == m_last_occupancy_sample_cycle) return;
    m_last_occupancy_sample_cycle = cycle;
    m_pib_occupancy_cycle_sum += m_live_instructions.size();
    ++m_pib_occupancy_sample_cycles;
  }

  unsigned tag_bank(uint64_t line_address, unsigned logical_set_count) const {
    assert(logical_set_count > 0);
    const unsigned logical_set =
        static_cast<unsigned>((line_address / kLogicalLineBytes) % logical_set_count);
    return logical_set % m_cfg.tag_banks;
  }

  bool try_serve_tag(uint64_t cycle, uint64_t line_address,
                     unsigned logical_set_count) {
    if (!enabled()) return true;
    begin_cycle(cycle);
    const unsigned bank = tag_bank(line_address, logical_set_count);
    if (m_total_served_this_cycle >= m_cfg.tag_requests_per_cycle ||
        m_served_per_bank[bank] >= m_cfg.tag_requests_per_bank_per_cycle) {
      ++m_tag_conflicts;
      ++m_tag_conflict_stall_cycles;
      ++m_nonexclusive_tag_conflict_cycles;
      ++m_frontend_stall_cycles;
      return false;
    }
    ++m_served_per_bank[bank];
    ++m_requests_per_bank[bank];
    ++m_total_served_this_cycle;
    ++m_tag_requests;
    return true;
  }

  size_t pib_occupancy() const { return m_live_instructions.size(); }
  uint64_t admits() const { return m_admits; }
  uint64_t retires() const { return m_retires; }
  uint64_t tag_requests() const { return m_tag_requests; }
  uint64_t tag_conflicts() const { return m_tag_conflicts; }
  uint64_t pib_full_events() const { return m_pib_full_events; }
  uint64_t pib_full_stall_cycles() const { return m_pib_full_stall_cycles; }
  uint64_t tag_conflict_stall_cycles() const {
    return m_tag_conflict_stall_cycles;
  }
  uint64_t nonexclusive_pib_full_cycles() const {
    return m_nonexclusive_pib_full_cycles;
  }
  uint64_t nonexclusive_tag_conflict_cycles() const {
    return m_nonexclusive_tag_conflict_cycles;
  }
  uint64_t frontend_stall_cycles() const { return m_frontend_stall_cycles; }
  size_t pib_peak() const { return m_pib_peak; }
  uint64_t pib_occupancy_cycle_sum() const {
    return m_pib_occupancy_cycle_sum;
  }
  uint64_t pib_occupancy_sample_cycles() const {
    return m_pib_occupancy_sample_cycles;
  }
  const std::vector<uint64_t> &requests_per_bank() const {
    return m_requests_per_bank;
  }

  paper_frontend_stats stats() const {
    paper_frontend_stats result;
    result.admits = m_admits;
    result.retires = m_retires;
    result.tag_requests = m_tag_requests;
    result.tag_conflicts = m_tag_conflicts;
    result.pib_full_events = m_pib_full_events;
    result.pib_full_stall_cycles = m_pib_full_stall_cycles;
    result.tag_conflict_stall_cycles = m_tag_conflict_stall_cycles;
    result.nonexclusive_pib_full_cycles = m_nonexclusive_pib_full_cycles;
    result.nonexclusive_tag_conflict_cycles =
        m_nonexclusive_tag_conflict_cycles;
    result.frontend_stall_cycles = m_frontend_stall_cycles;
    result.pib_occupancy_cycle_sum = m_pib_occupancy_cycle_sum;
    result.pib_occupancy_sample_cycles = m_pib_occupancy_sample_cycles;
    result.pib_occupancy = m_live_instructions.size();
    result.pib_peak = m_pib_peak;
    result.requests_per_bank = m_requests_per_bank;
    return result;
  }

  void assert_accounting() const {
    assert(m_admits >= m_retires);
    assert(m_admits - m_retires == m_live_instructions.size());
    assert(m_live_instructions.size() <= m_cfg.pib_entries);
    assert(m_pib_full_stall_cycles + m_tag_conflict_stall_cycles ==
           m_frontend_stall_cycles);
  }

  void assert_drained() const {
    assert_accounting();
    assert(m_live_instructions.empty());
  }

 private:
  config m_cfg;
  std::set<uint64_t> m_live_instructions;
  uint64_t m_cycle = UINT64_MAX;
  unsigned m_total_served_this_cycle = 0;
  std::vector<unsigned> m_served_per_bank =
      std::vector<unsigned>(m_cfg.tag_banks, 0);
  std::vector<uint64_t> m_requests_per_bank =
      std::vector<uint64_t>(m_cfg.tag_banks, 0);
  uint64_t m_admits = 0;
  uint64_t m_retires = 0;
  uint64_t m_tag_requests = 0;
  uint64_t m_tag_conflicts = 0;
  uint64_t m_pib_full_events = 0;
  uint64_t m_pib_full_stall_cycles = 0;
  uint64_t m_tag_conflict_stall_cycles = 0;
  uint64_t m_nonexclusive_pib_full_cycles = 0;
  uint64_t m_nonexclusive_tag_conflict_cycles = 0;
  uint64_t m_frontend_stall_cycles = 0;
  size_t m_pib_peak = 0;
  uint64_t m_last_occupancy_sample_cycle = UINT64_MAX;
  uint64_t m_pib_occupancy_cycle_sum = 0;
  uint64_t m_pib_occupancy_sample_cycles = 0;
};

}  // namespace dtc_l1

#endif  // DTC_L1_COMMON_H
