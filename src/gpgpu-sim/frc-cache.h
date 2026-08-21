// Fetch and Replacement Cache (FRC) metadata model.
//
// GPGPU-Sim does not store cache payload bytes.  An frc_cache entry therefore
// represents one full cache-line payload capacity while tracking only the
// ownership and sector state needed by l2_cache.

#ifndef FRC_CACHE_H
#define FRC_CACHE_H

#include "addrdec.h"

#include <vector>

enum frc_entry_state {
  FRC_FREE = 0,
  FRC_FETCHING,
  FRC_FETCHED,
  FRC_EVICTING
};

class frc_cache {
 public:
  struct entry {
    entry()
        : state(FRC_FREE),
          block_addr(0),
          valid_mask(0),
          pending_mask(0),
          dirty_mask(0),
          victim_addr(0),
          victim_mask(0),
          alloc_time(0),
          fetched_time(0) {}

    frc_entry_state state;
    new_addr_type block_addr;
    unsigned valid_mask;
    unsigned pending_mask;
    unsigned dirty_mask;
    new_addr_type victim_addr;
    unsigned victim_mask;
    unsigned long long alloc_time;
    unsigned long long fetched_time;
  };

  frc_cache(unsigned entries, unsigned assoc, unsigned line_size);

  bool enabled() const { return !m_entries.empty(); }
  unsigned entries() const { return m_entries.size(); }
  unsigned assoc() const { return m_assoc; }
  unsigned sets() const { return m_sets; }

  unsigned set_index(new_addr_type addr) const;
  int lookup(new_addr_type addr) const;
  int find_free(new_addr_type addr) const;
  bool has_free(new_addr_type addr) const { return find_free(addr) >= 0; }
  int allocate(new_addr_type addr, unsigned long long time);
  void release(unsigned index);

  entry &at(unsigned index) { return m_entries.at(index); }
  const entry &at(unsigned index) const { return m_entries.at(index); }

 private:
  new_addr_type block_addr(new_addr_type addr) const;

  unsigned m_assoc;
  unsigned m_sets;
  unsigned m_line_size;
  std::vector<entry> m_entries;
};

#endif
