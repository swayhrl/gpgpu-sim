#include "vm_translation.h"

#include <assert.h>

namespace vm_translation {

namespace {

const uint64_t kPteBytes = 8;
const uint64_t kBasePage64KB = 64ULL * 1024ULL;
const uint64_t kLargePage2MB = 2ULL * 1024ULL * 1024ULL;

unsigned log2_exact(uint64_t value) {
  assert(value != 0 && (value & (value - 1)) == 0);
  unsigned result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

}  // namespace

uint64_t present_page_mapper::translate(const translation_key &key) const {
  assert(vm_core::valid_page_size(key.page_size));
  return key.vpn;  // distinct resident VPNs retain distinct PPNs.
}

bool page_table_config::valid() const {
  if (levels == 0 || virtual_address_bits != 49 ||
      application_physical_limit > pte_physical_base ||
      (pte_physical_base % kBasePage64KB) != 0 ||
      pte_physical_bytes == 0 ||
      pte_physical_base > ~0ULL - pte_physical_bytes)
    return false;

  // The largest supported key class is 64KB: four levels x two classes x
  // 2^(49-16) entries x 8 bytes = 512 GiB for the generic default.
  const unsigned max_vpn_bits = virtual_address_bits - log2_exact(kBasePage64KB);
  const uint64_t required_bytes =
      uint64_t(levels) * 2ULL * (1ULL << max_vpn_bits) * kPteBytes;
  return pte_physical_bytes >= required_bytes;
}

radix_page_table_backend::radix_page_table_backend(
    const page_table_config &config)
    : m_config(config) {
  assert(valid());
}

bool radix_page_table_backend::valid() const { return m_config.valid(); }

unsigned radix_page_table_backend::page_size_class(uint64_t page_size) const {
  if (page_size == kBasePage64KB) return 0;
  if (page_size == kLargePage2MB) return 1;
  assert(false && "M3 generic PTE backend supports 64KB and 2MB only");
  return 0;
}

unsigned radix_page_table_backend::vpn_bits(uint64_t page_size) const {
  assert(vm_core::valid_page_size(page_size));
  const unsigned shift = log2_exact(page_size);
  assert(shift < m_config.virtual_address_bits);
  return m_config.virtual_address_bits - shift;
}

uint64_t radix_page_table_backend::pte_address(const translation_key &key,
                                                unsigned level) const {
  assert(valid());
  assert(level < m_config.levels);
  const unsigned key_vpn_bits = vpn_bits(key.page_size);
  assert(key.vpn < (1ULL << key_vpn_bits));
  const uint64_t slot =
      (uint64_t(page_size_class(key.page_size) * m_config.levels + level)
       << key_vpn_bits) |
      key.vpn;
  const uint64_t offset = slot * kPteBytes;
  assert(offset <= m_config.pte_physical_bytes - kPteBytes);
  const uint64_t result = m_config.pte_physical_base + offset;
  assert(owns_pte_physical_address(result));
  return result;
}

uint64_t radix_page_table_backend::resolve_ppn(
    const translation_key &key) const {
  assert(vm_core::valid_page_size(key.page_size));
  return key.vpn;  // Preserve the frozen identity-like data mapping.
}

pte_request radix_page_table_backend::make_pte_request(
    const translation_key &key, unsigned level, uint64_t request_id) const {
  return pte_request(key, level, pte_address(key, level), request_id);
}

bool radix_page_table_backend::owns_pte_physical_address(uint64_t pa) const {
  return pa >= m_config.pte_physical_base &&
         pa < m_config.pte_physical_base + m_config.pte_physical_bytes;
}

bool tlb_config::valid() const {
  return entries != 0 && assoc != 0 && ports_per_cycle != 0 &&
         entries >= assoc && (entries % assoc) == 0;
}

unsigned tlb_config::sets() const {
  assert(valid());
  return entries / assoc;
}

set_associative_tlb::set_associative_tlb(const tlb_config &config)
    : m_config(config), m_entries(), m_stats(), m_port_cycle(~0ULL),
      m_ports_used(0), m_touch_clock(0) {
  assert(m_config.valid());
  m_entries.resize(m_config.entries);
}

unsigned set_associative_tlb::set_for(const translation_key &key) const {
  const uint64_t mixed = key.vpn ^ (uint64_t(key.asid) << 17) ^
                         (key.page_size >> 12);
  return unsigned(mixed % m_config.sets());
}

bool set_associative_tlb::try_consume_port(uint64_t cycle) {
  if (m_port_cycle != cycle) {
    m_port_cycle = cycle;
    m_ports_used = 0;
  }
  if (m_ports_used >= m_config.ports_per_cycle) {
    ++m_stats.port_stalls;
    return false;
  }
  ++m_ports_used;
  return true;
}

bool set_associative_tlb::probe(const translation_key &key, uint64_t cycle,
                                uint64_t *ppn) {
  (void)cycle;
  ++m_stats.accesses;
  const unsigned begin = set_for(key) * m_config.assoc;
  for (unsigned way = 0; way < m_config.assoc; ++way) {
    entry &candidate = m_entries[begin + way];
    if (candidate.valid && candidate.key == key) {
      candidate.last_touch = ++m_touch_clock;
      *ppn = candidate.ppn;
      ++m_stats.hits;
      return true;
    }
  }
  ++m_stats.misses;
  return false;
}

void set_associative_tlb::fill(const translation_key &key, uint64_t ppn,
                               uint64_t cycle) {
  (void)cycle;
  const unsigned begin = set_for(key) * m_config.assoc;
  entry *victim = 0;
  for (unsigned way = 0; way < m_config.assoc; ++way) {
    entry &candidate = m_entries[begin + way];
    if (candidate.valid && candidate.key == key) {
      candidate.ppn = ppn;
      candidate.last_touch = ++m_touch_clock;
      return;
    }
    if (!candidate.valid && victim == 0) victim = &candidate;
  }
  if (victim == 0) {
    victim = &m_entries[begin];
    for (unsigned way = 1; way < m_config.assoc; ++way) {
      entry &candidate = m_entries[begin + way];
      if (candidate.last_touch < victim->last_touch) victim = &candidate;
    }
    ++m_stats.evictions;
  }
  victim->valid = true;
  victim->key = key;
  victim->ppn = ppn;
  victim->last_touch = ++m_touch_clock;
}

unsigned set_associative_tlb::occupancy() const {
  unsigned result = 0;
  for (unsigned i = 0; i < m_entries.size(); ++i)
    if (m_entries[i].valid) ++result;
  return result;
}

bool translation_config::valid() const {
  return num_sms != 0 && vm_core::valid_page_size(page_size) && l1.valid() &&
         l2.valid() && mshr_entries != 0 && pwq_entries != 0 && walkers != 0 &&
         walk_latency != 0 && page_table.valid();
}

translation_controller::translation_controller(const translation_config &config)
    : m_config(config), m_default_page_table(config.page_table),
      m_page_table(&m_default_page_table), m_l1s(), m_l2(config.l2), m_mshrs(),
      m_pwq(), m_active_walks(), m_stats() {
  assert(m_config.valid());
  for (unsigned sid = 0; sid < m_config.num_sms; ++sid)
    m_l1s.push_back(set_associative_tlb(m_config.l1));
}

translation_controller::translation_controller(const translation_config &config,
                                               page_table_backend *backend)
    : m_config(config), m_default_page_table(config.page_table),
      m_page_table(backend), m_l1s(), m_l2(config.l2), m_mshrs(), m_pwq(),
      m_active_walks(), m_stats() {
  assert(m_config.valid());
  assert(m_page_table != 0 && m_page_table->valid());
  for (unsigned sid = 0; sid < m_config.num_sms; ++sid)
    m_l1s.push_back(set_associative_tlb(m_config.l1));
}

bool translation_controller::mshr_entry::has_waiter(uint64_t uid) const {
  for (unsigned i = 0; i < waiters.size(); ++i)
    if (waiters[i].uid == uid) return true;
  return false;
}

translation_controller::mshr_entry *translation_controller::find_mshr(
    const translation_key &key) {
  for (unsigned i = 0; i < m_mshrs.size(); ++i)
    if (m_mshrs[i].key == key) return &m_mshrs[i];
  return 0;
}

const translation_controller::mshr_entry *translation_controller::find_mshr(
    const translation_key &key) const {
  for (unsigned i = 0; i < m_mshrs.size(); ++i)
    if (m_mshrs[i].key == key) return &m_mshrs[i];
  return 0;
}

lookup_result translation_controller::allocate_or_merge(
    unsigned sid, uint64_t waiter_uid, const translation_key &key,
    uint64_t cycle) {
  mshr_entry *existing = find_mshr(key);
  if (existing != 0) {
    if (existing->has_waiter(waiter_uid)) return TRANSLATION_PENDING;
    existing->waiters.push_back(waiter(sid, waiter_uid));
    ++m_stats.mshr_merges;
    ++m_stats.waiter_registrations;
    return TRANSLATION_PENDING;
  }
  if (m_mshrs.size() >= m_config.mshr_entries) {
    ++m_stats.mshr_full_events;
    return MSHR_FULL;
  }
  if (m_pwq.size() >= m_config.pwq_entries) {
    ++m_stats.pwq_full_events;
    return PWQ_FULL;
  }
  assert(find_mshr(key) == 0);
  m_mshrs.push_back(mshr_entry(key, cycle));
  m_mshrs.back().waiters.push_back(waiter(sid, waiter_uid));
  m_pwq.push_back(key);
  ++m_stats.mshr_allocations;
  ++m_stats.waiter_registrations;
  return TRANSLATION_PENDING;
}

lookup_result translation_controller::translate(unsigned sid, unsigned asid,
                                                uint64_t sim_va,
                                                uint64_t cycle,
                                                uint64_t waiter_uid,
                                                uint64_t *sim_pa) {
  assert(sid < m_l1s.size());
  translation_key key(asid, vm_core::vpn(sim_va, m_config.page_size),
                      m_config.page_size);
  set_associative_tlb &l1_tlb = m_l1s[sid];
  if (!l1_tlb.try_consume_port(cycle)) return L1_PORT_STALL;
  uint64_t ppn = 0;
  if (!l1_tlb.probe(key, cycle, &ppn)) {
    if (!m_l2.try_consume_port(cycle)) return L2_PORT_STALL;
    if (!m_l2.probe(key, cycle, &ppn))
      return allocate_or_merge(sid, waiter_uid, key, cycle);
    l1_tlb.fill(key, ppn, cycle);
  }
  *sim_pa = ppn * key.page_size + vm_core::page_offset(sim_va, key.page_size);
  ++m_stats.completed;
  return READY;
}

bool translation_controller::complete_translation(const translation_key &key,
                                                   uint64_t cycle) {
  for (unsigned index = 0; index < m_mshrs.size(); ++index) {
    if (!(m_mshrs[index].key == key)) continue;
    const uint64_t ppn = m_page_table->resolve_ppn(key);
    ++m_stats.mapper_lookups;
    m_l2.fill(key, ppn, cycle);
    for (unsigned waiter_index = 0;
         waiter_index < m_mshrs[index].waiters.size(); ++waiter_index) {
      const waiter &entry_waiter = m_mshrs[index].waiters[waiter_index];
      assert(entry_waiter.sid < m_l1s.size());
      m_l1s[entry_waiter.sid].fill(key, ppn, cycle);
      ++m_stats.waiter_wakeups;
    }
    m_mshrs.erase(m_mshrs.begin() + index);
    for (unsigned queued = 0; queued < m_pwq.size(); ++queued) {
      if (m_pwq[queued] == key) {
        m_pwq.erase(m_pwq.begin() + queued);
        break;
      }
    }
    ++m_stats.mshr_releases;
    return true;
  }
  return false;
}

void translation_controller::cycle(uint64_t cycle) {
  for (unsigned index = 0; index < m_active_walks.size();) {
    if (m_active_walks[index].ready_cycle > cycle) {
      ++index;
      continue;
    }
    const active_walk finished = m_active_walks[index];
    assert(complete_translation(finished.key, cycle));
    ++m_stats.walk_completions;
    m_stats.walk_service_cycles += cycle - finished.start_cycle;
    m_active_walks.erase(m_active_walks.begin() + index);
  }
  while (!m_pwq.empty() && m_active_walks.size() < m_config.walkers) {
    const translation_key key = m_pwq.front();
    m_pwq.erase(m_pwq.begin());
    mshr_entry *entry = find_mshr(key);
    assert(entry != 0);
    m_stats.pwq_wait_cycles += cycle - entry->enqueue_cycle;
    m_active_walks.push_back(
        active_walk(key, cycle, cycle + m_config.walk_latency));
    ++m_stats.walk_starts;
  }
  assert(m_active_walks.size() <= m_config.walkers);
}

bool translation_controller::invariants_hold() const {
  if (m_stats.mshr_allocations < m_stats.mshr_releases ||
      m_stats.mshr_allocations - m_stats.mshr_releases != m_mshrs.size() ||
      m_stats.waiter_registrations < m_stats.waiter_wakeups ||
      m_stats.walk_starts < m_stats.walk_completions ||
      m_active_walks.size() > m_config.walkers)
    return false;
  uint64_t active_waiters = 0;
  for (unsigned i = 0; i < m_mshrs.size(); ++i) {
    for (unsigned j = i + 1; j < m_mshrs.size(); ++j)
      if (m_mshrs[i].key == m_mshrs[j].key) return false;
    active_waiters += m_mshrs[i].waiters.size();
    for (unsigned a = 0; a < m_mshrs[i].waiters.size(); ++a)
      for (unsigned b = a + 1; b < m_mshrs[i].waiters.size(); ++b)
        if (m_mshrs[i].waiters[a].uid == m_mshrs[i].waiters[b].uid)
          return false;
  }
  if (m_stats.waiter_registrations - m_stats.waiter_wakeups != active_waiters)
    return false;
  for (unsigned i = 0; i < m_pwq.size(); ++i)
    if (find_mshr(m_pwq[i]) == 0) return false;
  for (unsigned i = 0; i < m_active_walks.size(); ++i)
    if (find_mshr(m_active_walks[i].key) == 0) return false;
  return true;
}

bool translation_controller::quiescent_invariants_hold() const {
  return m_mshrs.empty() && invariants_hold();
}

const set_associative_tlb &translation_controller::l1(unsigned sid) const {
  assert(sid < m_l1s.size());
  return m_l1s[sid];
}

void translation_controller::print_stats(FILE *fout) const {
  tlb_stats l1_total;
  unsigned l1_occupancy = 0;
  for (unsigned sid = 0; sid < m_l1s.size(); ++sid) {
    const tlb_stats &stats = m_l1s[sid].stats();
    l1_total.accesses += stats.accesses;
    l1_total.hits += stats.hits;
    l1_total.misses += stats.misses;
    l1_total.evictions += stats.evictions;
    l1_total.port_stalls += stats.port_stalls;
    l1_occupancy += m_l1s[sid].occupancy();
  }
  const tlb_stats &l2_stats = m_l2.stats();
  fprintf(fout, "vm_functional_mapper_lookups = %llu\n",
          (unsigned long long)m_stats.mapper_lookups);
  fprintf(fout, "vm_functional_completed = %llu\n",
          (unsigned long long)m_stats.completed);
  fprintf(fout, "vm_translation_mshr_allocations = %llu\n",
          (unsigned long long)m_stats.mshr_allocations);
  fprintf(fout, "vm_translation_mshr_merges = %llu\n",
          (unsigned long long)m_stats.mshr_merges);
  fprintf(fout, "vm_translation_mshr_full_events = %llu\n",
          (unsigned long long)m_stats.mshr_full_events);
  fprintf(fout, "vm_translation_mshr_active = %u\n", active_mshrs());
  fprintf(fout, "vm_translation_pwq_occupancy = %u\n", (unsigned)m_pwq.size());
  fprintf(fout, "vm_translation_pwq_full_events = %llu\n",
          (unsigned long long)m_stats.pwq_full_events);
  fprintf(fout, "vm_translation_walkers_active = %u\n", active_walkers());
  fprintf(fout, "vm_translation_walk_starts = %llu\n",
          (unsigned long long)m_stats.walk_starts);
  fprintf(fout, "vm_translation_walk_completions = %llu\n",
          (unsigned long long)m_stats.walk_completions);
  fprintf(fout, "vm_translation_waiter_registrations = %llu\n",
          (unsigned long long)m_stats.waiter_registrations);
  fprintf(fout, "vm_translation_waiter_wakeups = %llu\n",
          (unsigned long long)m_stats.waiter_wakeups);
  fprintf(fout, "vm_l1_tlb_accesses = %llu\n",
          (unsigned long long)l1_total.accesses);
  fprintf(fout, "vm_l1_tlb_hits = %llu\n", (unsigned long long)l1_total.hits);
  fprintf(fout, "vm_l1_tlb_misses = %llu\n",
          (unsigned long long)l1_total.misses);
  fprintf(fout, "vm_l1_tlb_evictions = %llu\n",
          (unsigned long long)l1_total.evictions);
  fprintf(fout, "vm_l1_tlb_port_stalls = %llu\n",
          (unsigned long long)l1_total.port_stalls);
  fprintf(fout, "vm_l1_tlb_occupancy = %u\n", l1_occupancy);
  fprintf(fout, "vm_l2_tlb_accesses = %llu\n",
          (unsigned long long)l2_stats.accesses);
  fprintf(fout, "vm_l2_tlb_hits = %llu\n", (unsigned long long)l2_stats.hits);
  fprintf(fout, "vm_l2_tlb_misses = %llu\n",
          (unsigned long long)l2_stats.misses);
  fprintf(fout, "vm_l2_tlb_evictions = %llu\n",
          (unsigned long long)l2_stats.evictions);
  fprintf(fout, "vm_l2_tlb_port_stalls = %llu\n",
          (unsigned long long)l2_stats.port_stalls);
  fprintf(fout, "vm_l2_tlb_occupancy = %u\n", m_l2.occupancy());
}

}  // namespace vm_translation
