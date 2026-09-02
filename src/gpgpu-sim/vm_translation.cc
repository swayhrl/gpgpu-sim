#include "vm_translation.h"

#include <assert.h>

namespace vm_translation {

uint64_t present_page_mapper::translate(const translation_key &key) const {
  assert(vm_core::valid_page_size(key.page_size));
  return key.vpn;  // distinct resident VPNs retain distinct PPNs.
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
         l2.valid();
}

translation_controller::translation_controller(const translation_config &config)
    : m_config(config), m_mapper(), m_l1s(), m_l2(config.l2), m_stats() {
  assert(m_config.valid());
  for (unsigned sid = 0; sid < m_config.num_sms; ++sid)
    m_l1s.push_back(set_associative_tlb(m_config.l1));
}

lookup_result translation_controller::translate(unsigned sid, unsigned asid,
                                                uint64_t sim_va,
                                                uint64_t cycle,
                                                uint64_t *sim_pa) {
  assert(sid < m_l1s.size());
  translation_key key(asid, vm_core::vpn(sim_va, m_config.page_size),
                      m_config.page_size);
  set_associative_tlb &l1_tlb = m_l1s[sid];
  if (!l1_tlb.try_consume_port(cycle)) return L1_PORT_STALL;
  uint64_t ppn = 0;
  if (!l1_tlb.probe(key, cycle, &ppn)) {
    if (!m_l2.try_consume_port(cycle)) return L2_PORT_STALL;
    if (!m_l2.probe(key, cycle, &ppn)) {
      ppn = m_mapper.translate(key);
      ++m_stats.mapper_lookups;
      m_l2.fill(key, ppn, cycle);
    }
    l1_tlb.fill(key, ppn, cycle);
  }
  *sim_pa = ppn * key.page_size + vm_core::page_offset(sim_va, key.page_size);
  ++m_stats.completed;
  return READY;
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
