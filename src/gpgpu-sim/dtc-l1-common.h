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
  }

  void begin_cycle(uint64_t cycle) {
    if (cycle == m_cycle) return;
    m_cycle = cycle;
    m_allocations_this_cycle = 0;
  }

  bool admit(uint64_t uid) {
    if (m_entries.size() >= m_cfg.io_pib_entries) return false;
    m_entries.push_back({uid, {}, {}, false});
    return true;
  }

  io_access_result access(uint64_t cycle, uint64_t uid, uint64_t address) {
    begin_cycle(cycle);
    entry *owner = find_entry(uid);
    assert(owner);
    const uint64_t line = address & ~(kLogicalLineBytes - 1);
    tag_entry *tag = find_tag(line);
    if (tag) {
      if (!has_reference(*owner, tag->physical))
        owner->references.push_back(tag->physical);
      return {m_phys[tag->physical.id].ready ? io_access_kind::VALID_HIT
                                              : io_access_kind::PENDING_HIT,
              tag->physical};
    }
    if (m_allocations_this_cycle >= m_cfg.allocation_width) {
      owner->allocation_blocked = true;
      return {io_access_kind::NO_FREE_LINE, {}};
    }
    const int free_id = find_free_physical();
    if (free_id < 0) {
      owner->allocation_blocked = true;
      return {io_access_kind::NO_FREE_LINE, {}};
    }
    tag_entry *victim = select_victim(line);
    if (victim->valid) owner->release_on_retire.push_back(victim->physical);
    physical_identity identity{static_cast<unsigned>(free_id), ++m_phys[free_id].generation};
    m_phys[free_id].allocated = true;
    m_phys[free_id].ready = false;
    *victim = {true, line, identity, ++m_lru_clock};
    owner->references.push_back(identity);
    owner->allocation_blocked = false;
    ++m_allocations_this_cycle;
    ++m_new_misses;
    return {io_access_kind::NEW_MISS, identity};
  }

  void complete(physical_identity identity) {
    assert(identity.id < m_phys.size());
    physical_line &line = m_phys[identity.id];
    assert(line.allocated && line.generation == identity.generation);
    line.ready = true;
  }

  bool retire_head() {
    if (m_entries.empty() || !entry_ready(m_entries.front())) return false;
    for (const physical_identity identity : m_entries.front().release_on_retire)
      release(identity);
    m_entries.pop_front();
    ++m_retires;
    return true;
  }

  size_t occupancy() const { return m_entries.size(); }
  uint64_t new_misses() const { return m_new_misses; }
  uint64_t retires() const { return m_retires; }
  size_t free_lines() const {
    size_t result = 0;
    for (const physical_line &line : m_phys) result += !line.allocated;
    return result;
  }

 private:
  struct physical_line { bool allocated = false; bool ready = false; uint64_t generation = 0; };
  struct tag_entry { bool valid = false; uint64_t line = 0; physical_identity physical; uint64_t lru = 0; };
  struct entry { uint64_t uid; std::vector<physical_identity> references; std::vector<physical_identity> release_on_retire; bool allocation_blocked; };
  entry *find_entry(uint64_t uid) { for (entry &e : m_entries) if (e.uid == uid) return &e; return nullptr; }
  static bool has_reference(const entry &e, physical_identity identity) {
    for (const physical_identity &existing : e.references)
      if (existing.id == identity.id && existing.generation == identity.generation)
        return true;
    return false;
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
  bool entry_ready(const entry &e) const { if (e.allocation_blocked) return false; for (const physical_identity id : e.references) if (!m_phys[id.id].ready || m_phys[id.id].generation != id.generation) return false; return true; }
  void release(physical_identity identity) { physical_line &line = m_phys[identity.id]; assert(line.generation == identity.generation); line.allocated = false; line.ready = false; }
  config m_cfg; std::vector<tag_entry> m_tags; std::vector<physical_line> m_phys; std::deque<entry> m_entries;
  uint64_t m_cycle = UINT64_MAX, m_lru_clock = 0, m_new_misses = 0, m_retires = 0; unsigned m_allocations_this_cycle = 0, m_rr_next = 0;
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
