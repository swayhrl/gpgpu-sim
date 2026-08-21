#include "frc-cache.h"

#include <assert.h>

frc_cache::frc_cache(unsigned entries, unsigned assoc, unsigned line_size)
    : m_assoc(assoc), m_sets(0), m_line_size(line_size) {
  assert(entries != 0);
  assert(assoc != 0);
  assert(entries >= assoc);
  assert(entries % assoc == 0);
  assert(line_size != 0);
  assert((line_size & (line_size - 1)) == 0);
  m_sets = entries / assoc;
  m_entries.resize(entries);
}

new_addr_type frc_cache::block_addr(new_addr_type addr) const {
  return addr & ~(new_addr_type)(m_line_size - 1);
}

unsigned frc_cache::set_index(new_addr_type addr) const {
  assert(enabled());
  return (unsigned)((block_addr(addr) / m_line_size) % m_sets);
}

int frc_cache::lookup(new_addr_type addr) const {
  if (!enabled()) return -1;
  new_addr_type line = block_addr(addr);
  unsigned set = set_index(addr);
  unsigned first = set * m_assoc;
  for (unsigned way = 0; way < m_assoc; ++way) {
    unsigned index = first + way;
    if (m_entries[index].state != FRC_FREE &&
        m_entries[index].block_addr == line)
      return (int)index;
  }
  return -1;
}

int frc_cache::find_free(new_addr_type addr) const {
  if (!enabled()) return -1;
  unsigned set = set_index(addr);
  unsigned first = set * m_assoc;
  for (unsigned way = 0; way < m_assoc; ++way) {
    unsigned index = first + way;
    if (m_entries[index].state == FRC_FREE) return (int)index;
  }
  return -1;
}

int frc_cache::allocate(new_addr_type addr, unsigned long long time) {
  int index = find_free(addr);
  assert(index >= 0);
  entry &e = m_entries[index];
  assert(e.state == FRC_FREE);
  e.state = FRC_FETCHING;
  e.block_addr = block_addr(addr);
  e.valid_mask = 0;
  e.pending_mask = 0;
  e.victim_addr = 0;
  e.victim_mask = 0;
  e.alloc_time = time;
  e.fetched_time = 0;
  return index;
}

void frc_cache::release(unsigned index) {
  entry &e = m_entries.at(index);
  e = entry();
}
