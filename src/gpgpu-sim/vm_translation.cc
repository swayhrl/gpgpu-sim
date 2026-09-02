#include "vm_translation.h"

#include <assert.h>

namespace vm_translation {

namespace {

const uint64_t kPteBytes = 8;
const uint64_t kBasePage64KB = 64ULL * 1024ULL;
const uint64_t kLargePage2MB = 2ULL * 1024ULL * 1024ULL;
const unsigned kMaximumGenericVirtualAddressBits = 56;

unsigned log2_exact(uint64_t value) {
  assert(value != 0 && (value & (value - 1)) == 0);
  unsigned result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

bool add_no_overflow(uint64_t left, uint64_t right, uint64_t *result) {
  if (left > ~0ULL - right) return false;
  *result = left + right;
  return true;
}

// The generic hierarchy is deliberately balanced by VPN bits, not by an
// NVIDIA-specific page-table format: r=ceil(B/L), then [top,r,...,r].
bool hierarchy_namespace_bytes(unsigned levels, unsigned va_bits,
                               uint64_t *required_bytes) {
  const uint64_t page_sizes[] = {kBasePage64KB, kLargePage2MB};
  uint64_t total = 0;
  for (unsigned page_class = 0; page_class < 2; ++page_class) {
    const unsigned vpn_bits = va_bits - log2_exact(page_sizes[page_class]);
    if (levels == 0 || levels > vpn_bits) return false;
    const unsigned radix_bits = (vpn_bits + levels - 1) / levels;
    const unsigned top_bits = vpn_bits - radix_bits * (levels - 1);
    if (top_bits == 0) return false;
    for (unsigned level = 0; level < levels; ++level) {
      const unsigned prefix_bits = top_bits + level * radix_bits;
      // The generic VA cap keeps this below 64, but retain the explicit
      // bound so a future wider configuration cannot silently overflow.
      if (prefix_bits > 60) return false;
      const uint64_t bytes = 1ULL << (prefix_bits + 3);
      if (!add_no_overflow(total, bytes, &total)) return false;
    }
  }
  *required_bytes = total;
  return true;
}

}  // namespace

uint64_t present_page_mapper::translate(const translation_key &key) const {
  assert(vm_core::valid_page_size(key.page_size));
  return key.vpn;  // distinct resident VPNs retain distinct PPNs.
}

bool page_table_config::valid() const {
  const unsigned base_page_bits = log2_exact(kBasePage64KB);
  const unsigned large_page_bits = log2_exact(kLargePage2MB);
  if (levels == 0 || virtual_address_bits <= large_page_bits ||
      virtual_address_bits > kMaximumGenericVirtualAddressBits ||
      application_physical_limit < (1ULL << virtual_address_bits) ||
      application_physical_limit > pte_physical_base ||
      (pte_physical_base % kBasePage64KB) != 0 ||
      pte_physical_bytes == 0 ||
      pte_physical_base > ~0ULL - pte_physical_bytes)
    return false;

  (void)base_page_bits;
  // Prefix namespaces are sized exactly by their radix level.  The current
  // 56-bit generic reservation remains 2^46 bytes (larger than required),
  // but validity must not depend on the old flat full-VPN encoding.
  uint64_t required_bytes = 0;
  if (!hierarchy_namespace_bytes(levels, virtual_address_bits,
                                 &required_bytes))
    return false;
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

unsigned radix_page_table_backend::level_width(uint64_t page_size,
                                                unsigned level) const {
  assert(level < m_config.levels);
  const unsigned bits = vpn_bits(page_size);
  const unsigned radix_bits = (bits + m_config.levels - 1) / m_config.levels;
  if (level == 0) return bits - radix_bits * (m_config.levels - 1);
  return radix_bits;
}

unsigned radix_page_table_backend::prefix_bits_for_level(
    uint64_t page_size, unsigned level) const {
  assert(level < m_config.levels);
  unsigned result = 0;
  for (unsigned index = 0; index <= level; ++index)
    result += level_width(page_size, index);
  return result;
}

uint64_t radix_page_table_backend::vpn_prefix(const translation_key &key,
                                               unsigned level) const {
  assert(supports_key(key));
  const unsigned bits = vpn_bits(key.page_size);
  const unsigned prefix_bits = prefix_bits_for_level(key.page_size, level);
  assert(prefix_bits <= bits);
  return key.vpn >> (bits - prefix_bits);
}

uint64_t radix_page_table_backend::pte_namespace_offset(
    uint64_t page_size, unsigned level) const {
  assert(level < m_config.levels);
  const unsigned target_class = page_size_class(page_size);
  const uint64_t page_sizes[] = {kBasePage64KB, kLargePage2MB};
  uint64_t offset = 0;
  for (unsigned page_class = 0; page_class <= target_class; ++page_class) {
    const unsigned final_level =
        page_class == target_class ? level : m_config.levels;
    for (unsigned index = 0; index < final_level; ++index) {
      const unsigned prefix_bits =
          prefix_bits_for_level(page_sizes[page_class], index);
      const uint64_t bytes = 1ULL << (prefix_bits + 3);
      assert(add_no_overflow(offset, bytes, &offset));
    }
  }
  return offset;
}

bool radix_page_table_backend::pte_prefix_identity(
    const translation_key &key, unsigned level, unsigned *page_class,
    uint64_t *prefix) const {
  if (!supports_key(key) || level >= m_config.levels || page_class == 0 ||
      prefix == 0)
    return false;
  *page_class = page_size_class(key.page_size);
  *prefix = vpn_prefix(key, level);
  return true;
}

bool radix_page_table_backend::supports_key(const translation_key &key) const {
  if (!valid() || (key.page_size != kBasePage64KB &&
                   key.page_size != kLargePage2MB))
    return false;
  const unsigned key_vpn_bits = vpn_bits(key.page_size);
  return key.vpn < (1ULL << key_vpn_bits);
}

uint64_t radix_page_table_backend::pte_address(const translation_key &key,
                                                unsigned level) const {
  assert(valid());
  assert(level < m_config.levels);
  assert(supports_key(key));
  // Physical PTE identities use the radix prefix at each level, never the
  // full VPN.  This gives upper-level PTE locality exactly where two VPNs
  // share their hierarchy prefix, while the leaf remains unique.
  const uint64_t offset =
      pte_namespace_offset(key.page_size, level) +
      vpn_prefix(key, level) * kPteBytes;
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

bool pwc_config::valid() const {
  if (mode > PWC_IDEAL || lookup_latency == 0) return false;
  if (mode == PWC_OFF) return entries == 0;
  if (mode == PWC_FINITE) return entries != 0;
  return true;  // IDEAL is intentionally unbounded; entries is ignored.
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
         walk_latency != 0 && ptw_mode <= 1 && page_table.valid() &&
         pwc.valid();
}

translation_controller::translation_controller(const translation_config &config)
    : m_config(config), m_default_page_table(config.page_table),
      m_page_table(&m_default_page_table), m_l1s(), m_l2(config.l2), m_mshrs(),
      m_pwq(), m_active_walks(), m_pwc(), m_stats(),
      m_next_pte_request_id(1), m_pwc_touch_clock(0) {
  assert(m_config.valid());
  initialize_pwc_stats();
  for (unsigned sid = 0; sid < m_config.num_sms; ++sid)
    m_l1s.push_back(set_associative_tlb(m_config.l1));
}

translation_controller::translation_controller(const translation_config &config,
                                               page_table_backend *backend)
    : m_config(config), m_default_page_table(config.page_table),
      m_page_table(backend), m_l1s(), m_l2(config.l2), m_mshrs(), m_pwq(),
      m_active_walks(), m_pwc(), m_stats(), m_next_pte_request_id(1),
      m_pwc_touch_clock(0) {
  assert(m_config.valid());
  assert(m_page_table != 0 && m_page_table->valid());
  initialize_pwc_stats();
  for (unsigned sid = 0; sid < m_config.num_sms; ++sid)
    m_l1s.push_back(set_associative_tlb(m_config.l1));
}

void translation_controller::initialize_pwc_stats() {
  const unsigned levels = m_page_table->levels();
  m_stats.pwc_accesses_by_level.assign(levels, 0);
  m_stats.pwc_hits_by_level.assign(levels, 0);
  m_stats.pwc_misses_by_level.assign(levels, 0);
  m_stats.pwc_pte_requests_skipped_by_level.assign(levels, 0);
}

bool translation_controller::pwc_identity(const translation_key &key,
                                          unsigned level,
                                          unsigned *page_class,
                                          uint64_t *prefix) const {
  assert(page_class != 0 && prefix != 0);
  return m_page_table->pte_prefix_identity(key, level, page_class, prefix);
}

bool translation_controller::pwc_lookup(const active_walk &walk) {
  assert(pwc_enabled() && !pwc_is_leaf(walk.next_level));
  unsigned page_class = 0;
  uint64_t prefix = 0;
  assert(pwc_identity(walk.key, walk.next_level, &page_class, &prefix));
  const unsigned level = walk.next_level;
  ++m_stats.pwc_accesses;
  ++m_stats.pwc_accesses_by_level[level];
  m_stats.pwc_service_cycles += m_config.pwc.lookup_latency;
  for (unsigned index = 0; index < m_pwc.size(); ++index) {
    pwc_entry &entry = m_pwc[index];
    if (entry.asid == walk.key.asid && entry.page_class == page_class &&
        entry.level == level && entry.prefix == prefix) {
      entry.last_touch = ++m_pwc_touch_clock;
      ++m_stats.pwc_hits;
      ++m_stats.pwc_hits_by_level[level];
      ++m_stats.pwc_pte_requests_skipped_by_level[level];
      return true;
    }
  }
  ++m_stats.pwc_misses;
  ++m_stats.pwc_misses_by_level[level];
  return false;
}

void translation_controller::pwc_insert(const translation_key &key,
                                        unsigned level) {
  if (!pwc_enabled() || pwc_is_leaf(level)) return;
  unsigned page_class = 0;
  uint64_t prefix = 0;
  assert(pwc_identity(key, level, &page_class, &prefix));
  for (unsigned index = 0; index < m_pwc.size(); ++index) {
    pwc_entry &entry = m_pwc[index];
    if (entry.asid == key.asid && entry.page_class == page_class &&
        entry.level == level && entry.prefix == prefix) {
      entry.last_touch = ++m_pwc_touch_clock;
      return;
    }
  }
  if (m_config.pwc.mode == PWC_FINITE &&
      m_pwc.size() >= m_config.pwc.entries) {
    unsigned victim = 0;
    for (unsigned index = 1; index < m_pwc.size(); ++index)
      if (m_pwc[index].last_touch < m_pwc[victim].last_touch) victim = index;
    m_pwc[victim] = pwc_entry(key.asid, page_class, level, prefix,
                              ++m_pwc_touch_clock);
    ++m_stats.pwc_evictions;
  } else {
    m_pwc.push_back(pwc_entry(key.asid, page_class, level, prefix,
                              ++m_pwc_touch_clock));
  }
  ++m_stats.pwc_inserts;
  m_stats.pwc_occupancy = m_pwc.size();
  if (m_stats.pwc_occupancy > m_stats.pwc_occupancy_high_watermark)
    m_stats.pwc_occupancy_high_watermark = m_stats.pwc_occupancy;
}

void translation_controller::service_pwc(uint64_t cycle) {
  if (!pwc_enabled()) return;
  for (unsigned index = 0; index < m_active_walks.size(); ++index) {
    active_walk &walk = m_active_walks[index];
    if (walk.pte_outstanding || walk.pwc_miss_ready ||
        pwc_is_leaf(walk.next_level))
      continue;
    if (!walk.pwc_probe_scheduled) {
      walk.pwc_probe_scheduled = true;
      walk.pwc_probe_ready_cycle = cycle + m_config.pwc.lookup_latency;
      continue;
    }
    if (walk.pwc_probe_ready_cycle > cycle) continue;
    walk.pwc_probe_scheduled = false;
    if (pwc_lookup(walk)) {
      ++walk.next_level;
    } else {
      walk.pwc_miss_ready = true;
    }
  }
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
    if (existing->waiters.size() > m_stats.mshr_waiter_depth_max)
      m_stats.mshr_waiter_depth_max = existing->waiters.size();
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
  note_mshr_occupancy();
  if (m_mshrs.back().waiters.size() > m_stats.mshr_waiter_depth_max)
    m_stats.mshr_waiter_depth_max = m_mshrs.back().waiters.size();
  return TRANSLATION_PENDING;
}

void translation_controller::note_mshr_occupancy() {
  if (m_mshrs.size() > m_stats.mshr_occupancy_high_watermark)
    m_stats.mshr_occupancy_high_watermark = m_mshrs.size();
}

lookup_result translation_controller::translate(unsigned sid, unsigned asid,
                                                uint64_t sim_va,
                                                uint64_t cycle,
                                                uint64_t waiter_uid,
                                                uint64_t *sim_pa) {
  assert(sid < m_l1s.size());
  translation_key key(asid, vm_core::vpn(sim_va, m_config.page_size),
                      m_config.page_size);
  // A previously accepted waiter is already represented by this active MSHR.
  // It must wait without competing for either TLB lookup resource or
  // polluting probe/miss statistics.  A distinct UID deliberately falls
  // through to the normal first lookup and may then merge below.
  const mshr_entry *existing = find_mshr(key);
  if (existing != 0 && existing->has_waiter(waiter_uid)) {
    ++m_stats.pending_waiter_bypasses;
    return TRANSLATION_PENDING;
  }
  ++m_stats.lookup_requests;
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
    const uint64_t waiter_depth = m_mshrs[index].waiters.size();
    const uint64_t lifetime = cycle - m_mshrs[index].enqueue_cycle;
    ++m_stats.mshr_entries_completed;
    m_stats.mshr_waiter_depth_total += waiter_depth;
    if (waiter_depth > m_stats.mshr_waiter_depth_max)
      m_stats.mshr_waiter_depth_max = waiter_depth;
    m_stats.mshr_lifetime_cycles_total += lifetime;
    if (lifetime > m_stats.mshr_lifetime_cycles_max)
      m_stats.mshr_lifetime_cycles_max = lifetime;
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
  if (uses_real_memory_ptw()) {
    while (!m_pwq.empty() && m_active_walks.size() < m_config.walkers) {
      const translation_key key = m_pwq.front();
      m_pwq.erase(m_pwq.begin());
      mshr_entry *entry = find_mshr(key);
      assert(entry != 0);
      m_stats.pwq_wait_cycles += cycle - entry->enqueue_cycle;
      m_active_walks.push_back(active_walk(key, cycle, 0));
      ++m_stats.walk_starts;
    }
    // A PWC probe is a one-cycle generic service, with no modeled port
    // contention.  A hit advances exactly one intermediate level; a miss
    // makes that level available to the existing physical PTE path below.
    service_pwc(cycle);
    assert(m_active_walks.size() <= m_config.walkers);
    return;
  }
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

bool translation_controller::next_pte_request(pte_request *request) const {
  assert(request != 0);
  if (!uses_real_memory_ptw()) return false;
  for (unsigned index = 0; index < m_active_walks.size(); ++index) {
    const active_walk &walk = m_active_walks[index];
    if (walk.pte_outstanding) continue;
    assert(walk.next_level < m_page_table->levels());
    if (pwc_enabled() && !pwc_is_leaf(walk.next_level) &&
        !walk.pwc_miss_ready)
      continue;
    const uint64_t request_id =
        walk.pte_request_id ? walk.pte_request_id : m_next_pte_request_id;
    *request =
        m_page_table->make_pte_request(walk.key, walk.next_level, request_id);
    assert(request->is_physical && request->bypass_translation);
    return true;
  }
  return false;
}

bool translation_controller::pte_request_issued(const pte_request &request,
                                                 uint64_t cycle) {
  (void)cycle;
  if (!uses_real_memory_ptw() || !request.is_physical ||
      !request.bypass_translation ||
      !m_page_table->owns_pte_physical_address(request.physical_address))
    return false;
  for (unsigned index = 0; index < m_active_walks.size(); ++index) {
    active_walk &walk = m_active_walks[index];
    if (walk.key == request.key && !walk.pte_outstanding &&
        walk.next_level == request.level &&
        (walk.pte_request_id == 0 || walk.pte_request_id == request.request_id)) {
      if (pwc_enabled() && !pwc_is_leaf(walk.next_level) &&
          !walk.pwc_miss_ready)
        return false;
      walk.pte_outstanding = true;
      walk.pte_request_id = request.request_id;
      walk.pwc_miss_ready = false;
      if (request.request_id == m_next_pte_request_id) ++m_next_pte_request_id;
      ++m_stats.pte_requests;
      return true;
    }
  }
  return false;
}

bool translation_controller::complete_pte_response(uint64_t request_id,
                                                    uint64_t physical_address,
                                                    bool reached_dram,
                                                    uint64_t cycle) {
  if (!uses_real_memory_ptw() ||
      !m_page_table->owns_pte_physical_address(physical_address))
    return false;
  for (unsigned index = 0; index < m_active_walks.size(); ++index) {
    active_walk &walk = m_active_walks[index];
    if (!walk.pte_outstanding || walk.pte_request_id != request_id) continue;
    const pte_request expected =
        m_page_table->make_pte_request(walk.key, walk.next_level, request_id);
    if (expected.physical_address != physical_address) {
      ++m_stats.pte_response_misassociations;
      return false;
    }
    ++m_stats.pte_responses;
    if (reached_dram)
      ++m_stats.pte_dram_responses;
    else
      ++m_stats.pte_l2_only_responses;
    const unsigned completed_level = walk.next_level;
    walk.pte_outstanding = false;
    walk.pte_request_id = 0;
    pwc_insert(walk.key, completed_level);
    ++walk.next_level;
    if (walk.next_level < m_page_table->levels()) return true;
    assert(complete_translation(walk.key, cycle));
    ++m_stats.walk_completions;
    m_stats.walk_service_cycles += cycle - walk.start_cycle;
    m_active_walks.erase(m_active_walks.begin() + index);
    return true;
  }
  ++m_stats.pte_response_misassociations;
  return false;
}

bool translation_controller::invariants_hold() const {
  if (m_stats.mshr_allocations < m_stats.mshr_releases ||
      m_stats.mshr_allocations - m_stats.mshr_releases != m_mshrs.size() ||
      m_stats.waiter_registrations < m_stats.waiter_wakeups ||
      m_stats.walk_starts < m_stats.walk_completions ||
      m_active_walks.size() > m_config.walkers ||
      m_stats.pwc_occupancy != m_pwc.size() ||
      (m_config.pwc.mode == PWC_FINITE &&
       m_pwc.size() > m_config.pwc.entries))
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
    if (find_mshr(m_active_walks[i].key) == 0 ||
        (uses_real_memory_ptw() &&
         m_active_walks[i].next_level >= m_page_table->levels()))
      return false;
  for (unsigned i = 0; i < m_pwc.size(); ++i)
    for (unsigned j = i + 1; j < m_pwc.size(); ++j)
      if (m_pwc[i].asid == m_pwc[j].asid &&
          m_pwc[i].page_class == m_pwc[j].page_class &&
          m_pwc[i].level == m_pwc[j].level &&
          m_pwc[i].prefix == m_pwc[j].prefix)
        return false;
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
  fprintf(fout, "vm_translation_page_size_bytes = %llu\n",
          (unsigned long long)m_config.page_size);
  fprintf(fout, "vm_translation_lookup_requests = %llu\n",
          (unsigned long long)m_stats.lookup_requests);
  fprintf(fout, "vm_translation_pending_waiter_bypasses = %llu\n",
          (unsigned long long)m_stats.pending_waiter_bypasses);
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
  fprintf(fout, "vm_translation_mshr_occupancy_high_watermark = %llu\n",
          (unsigned long long)m_stats.mshr_occupancy_high_watermark);
  fprintf(fout, "vm_translation_mshr_entries_completed = %llu\n",
          (unsigned long long)m_stats.mshr_entries_completed);
  fprintf(fout, "vm_translation_mshr_waiter_depth_total = %llu\n",
          (unsigned long long)m_stats.mshr_waiter_depth_total);
  fprintf(fout, "vm_translation_mshr_waiter_depth_max = %llu\n",
          (unsigned long long)m_stats.mshr_waiter_depth_max);
  fprintf(fout, "vm_translation_mshr_lifetime_cycles_total = %llu\n",
          (unsigned long long)m_stats.mshr_lifetime_cycles_total);
  fprintf(fout, "vm_translation_mshr_lifetime_cycles_max = %llu\n",
          (unsigned long long)m_stats.mshr_lifetime_cycles_max);
  fprintf(fout, "vm_translation_pwq_occupancy = %u\n", (unsigned)m_pwq.size());
  fprintf(fout, "vm_translation_pwq_full_events = %llu\n",
          (unsigned long long)m_stats.pwq_full_events);
  fprintf(fout, "vm_translation_walkers_active = %u\n", active_walkers());
  fprintf(fout, "vm_translation_walk_starts = %llu\n",
          (unsigned long long)m_stats.walk_starts);
  fprintf(fout, "vm_translation_walk_completions = %llu\n",
          (unsigned long long)m_stats.walk_completions);
  fprintf(fout, "vm_pte_requests = %llu\n",
          (unsigned long long)m_stats.pte_requests);
  fprintf(fout, "vm_pte_responses = %llu\n",
          (unsigned long long)m_stats.pte_responses);
  fprintf(fout, "vm_pte_l2_only_responses = %llu\n",
          (unsigned long long)m_stats.pte_l2_only_responses);
  fprintf(fout, "vm_pte_dram_responses = %llu\n",
          (unsigned long long)m_stats.pte_dram_responses);
  fprintf(fout, "vm_pte_response_misassociations = %llu\n",
          (unsigned long long)m_stats.pte_response_misassociations);
  fprintf(fout, "vm_pwc_mode = %u\n", m_config.pwc.mode);
  fprintf(fout, "vm_pwc_entries_configured = %u\n", m_config.pwc.entries);
  fprintf(fout, "vm_pwc_lookup_latency_cycles = %u\n",
          m_config.pwc.lookup_latency);
  fprintf(fout, "vm_pwc_accesses = %llu\n",
          (unsigned long long)m_stats.pwc_accesses);
  fprintf(fout, "vm_pwc_hits = %llu\n",
          (unsigned long long)m_stats.pwc_hits);
  fprintf(fout, "vm_pwc_misses = %llu\n",
          (unsigned long long)m_stats.pwc_misses);
  fprintf(fout, "vm_pwc_inserts = %llu\n",
          (unsigned long long)m_stats.pwc_inserts);
  fprintf(fout, "vm_pwc_evictions = %llu\n",
          (unsigned long long)m_stats.pwc_evictions);
  fprintf(fout, "vm_pwc_occupancy = %llu\n",
          (unsigned long long)m_stats.pwc_occupancy);
  fprintf(fout, "vm_pwc_occupancy_high_watermark = %llu\n",
          (unsigned long long)m_stats.pwc_occupancy_high_watermark);
  fprintf(fout, "vm_pwc_service_cycles = %llu\n",
          (unsigned long long)m_stats.pwc_service_cycles);
  for (unsigned level = 0; level < m_stats.pwc_accesses_by_level.size();
       ++level) {
    fprintf(fout, "vm_pwc_level_%u_accesses = %llu\n", level,
            (unsigned long long)m_stats.pwc_accesses_by_level[level]);
    fprintf(fout, "vm_pwc_level_%u_hits = %llu\n", level,
            (unsigned long long)m_stats.pwc_hits_by_level[level]);
    fprintf(fout, "vm_pwc_level_%u_misses = %llu\n", level,
            (unsigned long long)m_stats.pwc_misses_by_level[level]);
    fprintf(fout, "vm_pwc_level_%u_pte_requests_skipped = %llu\n", level,
            (unsigned long long)m_stats.pwc_pte_requests_skipped_by_level[level]);
  }
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
