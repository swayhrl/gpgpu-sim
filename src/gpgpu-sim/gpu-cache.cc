// Copyright (c) 2009-2021, Tor M. Aamodt, Tayler Hetherington,
// Vijay Kandiah, Nikos Hardavellas, Mahmoud Khairy, Junrui Pan,
// Timothy G. Rogers
// The University of British Columbia, Northwestern University, Purdue
// University All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this
//    list of conditions and the following disclaimer;
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution;
// 3. Neither the names of The University of British Columbia, Northwestern
//    University nor the names of their contributors may be used to
//    endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "gpu-cache.h"
#include "l2_admission_rules.h"
#include <algorithm>
#include <assert.h>
#include "gpu-sim.h"
#include "hashing.h"

namespace {
struct l2_char_tag_state {
  l2_char_tag_state() : reserved(0), dirty(0), valid(0), reserved_set_max(0) {}
  unsigned reserved, dirty, valid, reserved_set_max;
  std::vector<unsigned char> reserved_way, dirty_way, valid_way;
  std::vector<unsigned> reserved_by_set;
};

// Kept outside tag_array deliberately: tag_array is shared by L1 and L2 and
// changing its object layout would make an observation-only feature risky.
std::map<const tag_array *, l2_char_tag_state> g_l2_char_tag_states;
}
#include "stat-tool.h"

// used to allocate memory that is large enough to adapt the changes in cache
// size across kernels

const char *cache_request_status_str(enum cache_request_status status) {
  static const char *static_cache_request_status_str[] = {
      "HIT",         "HIT_RESERVED", "MISS",           "RESERVATION_FAIL",
      "SECTOR_MISS", "MSHR_HIT",     "WRITE_ALLOCATED"};

  assert(sizeof(static_cache_request_status_str) / sizeof(const char *) ==
         NUM_CACHE_REQUEST_STATUS);
  assert(status < NUM_CACHE_REQUEST_STATUS);

  return static_cache_request_status_str[status];
}

const char *cache_fail_status_str(enum cache_reservation_fail_reason status) {
  static const char *static_cache_reservation_fail_reason_str[] = {
      "LINE_ALLOC_FAIL", "MISS_QUEUE_FULL", "MSHR_ENRTY_FAIL",
      "MSHR_MERGE_ENRTY_FAIL", "MSHR_RW_PENDING"};

  assert(sizeof(static_cache_reservation_fail_reason_str) /
             sizeof(const char *) ==
         NUM_CACHE_RESERVATION_FAIL_STATUS);
  assert(status < NUM_CACHE_RESERVATION_FAIL_STATUS);

  return static_cache_reservation_fail_reason_str[status];
}

unsigned l1d_cache_config::set_bank(new_addr_type addr) const {
  // For sector cache, we select one sector per bank (sector interleaving)
  // This is what was found in Volta (one sector per bank, sector interleaving)
  // otherwise, line interleaving
  return cache_config::hash_function(addr, l1_banks,
                                     l1_banks_byte_interleaving_log2,
                                     l1_banks_log2, l1_banks_hashing_function);
}

unsigned cache_config::set_index(new_addr_type addr) const {
  return cache_config::hash_function(addr, m_nset, m_line_sz_log2, m_nset_log2,
                                     m_set_index_function);
}

unsigned cache_config::hash_function(new_addr_type addr, unsigned m_nset,
                                     unsigned m_line_sz_log2,
                                     unsigned m_nset_log2,
                                     unsigned m_index_function) const {
  unsigned set_index = 0;

  switch (m_index_function) {
    case FERMI_HASH_SET_FUNCTION: {
      /*
       * Set Indexing function from "A Detailed GPU Cache Model Based on Reuse
       * Distance Theory" Cedric Nugteren et al. HPCA 2014
       */
      unsigned lower_xor = 0;
      unsigned upper_xor = 0;

      if (m_nset == 32 || m_nset == 64) {
        // Lower xor value is bits 7-11
        lower_xor = (addr >> m_line_sz_log2) & 0x1F;

        // Upper xor value is bits 13, 14, 15, 17, and 19
        upper_xor = (addr & 0xE000) >> 13;    // Bits 13, 14, 15
        upper_xor |= (addr & 0x20000) >> 14;  // Bit 17
        upper_xor |= (addr & 0x80000) >> 15;  // Bit 19

        set_index = (lower_xor ^ upper_xor);

        // 48KB cache prepends the set_index with bit 12
        if (m_nset == 64) set_index |= (addr & 0x1000) >> 7;

      } else { /* Else incorrect number of sets for the hashing function */
        assert(
            "\nGPGPU-Sim cache configuration error: The number of sets should "
            "be "
            "32 or 64 for the hashing set index function.\n" &&
            0);
      }
      break;
    }

    case BITWISE_XORING_FUNCTION: {
      new_addr_type higher_bits = addr >> (m_line_sz_log2 + m_nset_log2);
      unsigned index = (addr >> m_line_sz_log2) & (m_nset - 1);
      set_index = bitwise_hash_function(higher_bits, index, m_nset);
      break;
    }
    case HASH_IPOLY_FUNCTION: {
      new_addr_type higher_bits = addr >> (m_line_sz_log2 + m_nset_log2);
      unsigned index = (addr >> m_line_sz_log2) & (m_nset - 1);
      set_index = ipoly_hash_function(higher_bits, index, m_nset);
      break;
    }
    case CUSTOM_SET_FUNCTION: {
      /* No custom set function implemented */
      break;
    }

    case LINEAR_SET_FUNCTION: {
      set_index = (addr >> m_line_sz_log2) & (m_nset - 1);
      break;
    }

    default: {
      assert("\nUndefined set index function.\n" && 0);
      break;
    }
  }

  // Linear function selected or custom set index function not implemented
  assert((set_index < m_nset) &&
         "\nError: Set index out of bounds. This is caused by "
         "an incorrect or unimplemented custom set index function.\n");

  return set_index;
}

void l2_cache_config::init(linear_to_raw_address_translation *address_mapping) {
  cache_config::init(m_config_string, FuncCachePreferNone);
  m_address_mapping = address_mapping;
}

unsigned l2_cache_config::set_index(new_addr_type addr) const {
  new_addr_type part_addr = addr;

  if (m_address_mapping) {
    // Calculate set index without memory partition bits to reduce set camping
    part_addr = m_address_mapping->partition_address(addr);
  }

  return cache_config::set_index(part_addr);
}

tag_array::~tag_array() {
  g_l2_char_tag_states.erase(this);
  unsigned cache_lines_num = m_config.get_max_num_lines();
  for (unsigned i = 0; i < cache_lines_num; ++i) delete m_lines[i];
  delete[] m_lines;
}

tag_array::tag_array(cache_config &config, int core_id, int type_id,
                     cache_block_t **new_lines)
    : m_config(config), m_lines(new_lines) {
  init(core_id, type_id);
}

void tag_array::update_cache_parameters(cache_config &config) {
  m_config = config;
}

tag_array::tag_array(cache_config &config, int core_id, int type_id)
    : m_config(config) {
  // assert( m_config.m_write_policy == READ_ONLY ); Old assert
  unsigned cache_lines_num = config.get_max_num_lines();
  m_lines = new cache_block_t *[cache_lines_num];
  if (config.m_cache_type == NORMAL) {
    for (unsigned i = 0; i < cache_lines_num; ++i)
      m_lines[i] = new line_cache_block();
  } else if (config.m_cache_type == SECTOR) {
    for (unsigned i = 0; i < cache_lines_num; ++i)
      m_lines[i] = new sector_cache_block();
  } else
    assert(0);

  init(core_id, type_id);
}

void tag_array::init(int core_id, int type_id) {
  m_access = 0;
  m_miss = 0;
  m_pending_hit = 0;
  m_res_fail = 0;
  m_sector_miss = 0;
  // initialize snapshot counters for visualizer
  m_prev_snapshot_access = 0;
  m_prev_snapshot_miss = 0;
  m_prev_snapshot_pending_hit = 0;
  m_core_id = core_id;
  m_type_id = type_id;
  is_used = false;
  m_dirty = 0;
}

void tag_array::l2_char_tracking_enable() {
  l2_char_tag_state &state = g_l2_char_tag_states[this];
  state = l2_char_tag_state();
  state.reserved_way.assign(m_config.get_num_lines(), 0);
  state.dirty_way.assign(m_config.get_num_lines(), 0);
  state.valid_way.assign(m_config.get_num_lines(), 0);
  state.reserved_by_set.assign(m_config.m_nset, 0);
  // One construction-time initialization scan is bounded and occurs before
  // L2 traffic.  Runtime sampling thereafter reads only these maintained
  // counters; it never scans the cache array per L2 cycle.
  for (unsigned i = 0; i < m_config.get_num_lines(); ++i)
    l2_char_tracking_refresh_line(i);
}

void tag_array::l2_char_tracking_refresh_line(unsigned idx) {
  std::map<const tag_array *, l2_char_tag_state>::iterator found =
      g_l2_char_tag_states.find(this);
  if (found == g_l2_char_tag_states.end()) return;
  assert(idx < m_config.get_num_lines());
  l2_char_tag_state &state = found->second;
  const unsigned set = idx / m_config.m_assoc;
  const unsigned char reserved = m_lines[idx]->is_reserved_line() ? 1 : 0;
  const unsigned char dirty = m_lines[idx]->is_modified_line() ? 1 : 0;
  const unsigned char valid = m_lines[idx]->is_valid_line() ? 1 : 0;
  if (reserved != state.reserved_way[idx]) {
    const unsigned old_reserved_in_set = state.reserved_by_set[set];
    state.reserved += reserved ? 1 : -1;
    state.reserved_by_set[set] += reserved ? 1 : -1;
    state.reserved_way[idx] = reserved;
    if (reserved) {
      state.reserved_set_max =
          std::max(state.reserved_set_max, state.reserved_by_set[set]);
    } else if (old_reserved_in_set == state.reserved_set_max) {
      state.reserved_set_max = 0;
      for (unsigned s = 0; s < state.reserved_by_set.size(); ++s)
        state.reserved_set_max =
            std::max(state.reserved_set_max, state.reserved_by_set[s]);
    }
  }
  if (dirty != state.dirty_way[idx]) {
    state.dirty += dirty ? 1 : -1;
    state.dirty_way[idx] = dirty;
  }
  if (valid != state.valid_way[idx]) {
    state.valid += valid ? 1 : -1;
    state.valid_way[idx] = valid;
  }
}

void tag_array::l2_char_storage_snapshot(unsigned &reserved, unsigned &dirty,
                                          unsigned &valid,
                                          std::vector<unsigned> &reserved_by_set) const {
  std::map<const tag_array *, l2_char_tag_state>::const_iterator found =
      g_l2_char_tag_states.find(this);
  assert(found != g_l2_char_tag_states.end());
  reserved = found->second.reserved;
  dirty = found->second.dirty;
  valid = found->second.valid;
  reserved_by_set = found->second.reserved_by_set;
}

void tag_array::l2_char_storage_snapshot_compact(
    unsigned &reserved, unsigned &dirty, unsigned &valid,
    unsigned &reserved_set_max) const {
  std::map<const tag_array *, l2_char_tag_state>::const_iterator found =
      g_l2_char_tag_states.find(this);
  assert(found != g_l2_char_tag_states.end());
  reserved = found->second.reserved;
  dirty = found->second.dirty;
  valid = found->second.valid;
  reserved_set_max = found->second.reserved_set_max;
}

void tag_array::add_pending_line(mem_fetch *mf) {
  assert(mf);
  new_addr_type addr = m_config.block_addr(mf->get_addr());
  line_table::const_iterator i = pending_lines.find(addr);
  if (i == pending_lines.end()) {
    pending_lines[addr] = mf->get_inst().get_uid();
  }
}

void tag_array::remove_pending_line(mem_fetch *mf) {
  assert(mf);
  new_addr_type addr = m_config.block_addr(mf->get_addr());
  line_table::const_iterator i = pending_lines.find(addr);
  if (i != pending_lines.end()) {
    pending_lines.erase(addr);
  }
}

enum cache_request_status tag_array::probe(new_addr_type addr, unsigned &idx,
                                           mem_fetch *mf, bool is_write,
                                           bool probe_mode) const {
  mem_access_sector_mask_t mask = mf->get_access_sector_mask();
  return probe(addr, idx, mask, is_write, probe_mode, mf);
}

enum cache_request_status tag_array::probe(new_addr_type addr, unsigned &idx,
                                           mem_access_sector_mask_t mask,
                                           bool is_write, bool probe_mode,
                                           mem_fetch *mf) const {
  // assert( m_config.m_write_policy == READ_ONLY );
  unsigned set_index = m_config.set_index(addr);
  new_addr_type tag = m_config.tag(addr);

  unsigned invalid_line = (unsigned)-1;
  unsigned valid_line = (unsigned)-1;
  unsigned long long valid_timestamp = (unsigned)-1;
  unsigned dirty_line = (unsigned)-1;
  unsigned long long dirty_timestamp = (unsigned)-1;

  bool all_reserved = true;
  // check for hit or pending hit
  for (unsigned way = 0; way < m_config.m_assoc; way++) {
    unsigned index = set_index * m_config.m_assoc + way;
    cache_block_t *line = m_lines[index];
    if (line->m_tag == tag) {
      if (line->get_status(mask) == RESERVED) {
        idx = index;
        return HIT_RESERVED;
      } else if (line->get_status(mask) == VALID) {
        idx = index;
        return HIT;
      } else if (line->get_status(mask) == MODIFIED) {
        if ((!is_write && line->is_readable(mask)) || is_write) {
          idx = index;
          return HIT;
        } else {
          idx = index;
          return SECTOR_MISS;
        }

      } else if (line->is_valid_line() && line->get_status(mask) == INVALID) {
        idx = index;
        return SECTOR_MISS;
      } else {
        assert(line->get_status(mask) == INVALID);
      }
    }
    if (!line->is_reserved_line()) {
      all_reserved = false;
      // percentage of dirty lines in the cache
      // number of dirty lines / total lines in the cache
      float dirty_line_percentage =
          ((float)m_dirty / (m_config.m_nset * m_config.m_assoc)) * 100;
      // If the cacheline is from a load op (not modified),
      // or the total dirty cacheline is above a specific value,
      // Then this cacheline is eligible to be considered for replacement
      // candidate i.e. Only evict clean cachelines until total dirty cachelines
      // reach the limit.
      if (!line->is_modified_line() ||
          dirty_line_percentage >= m_config.m_wr_percent) {
        if (line->is_invalid_line()) {
          invalid_line = index;
        } else {
          // valid line : keep track of most appropriate replacement candidate
          if (m_config.m_replacement_policy == LRU) {
            if (line->get_last_access_time() < valid_timestamp) {
              valid_timestamp = line->get_last_access_time();
              valid_line = index;
            }
          } else if (m_config.m_replacement_policy == FIFO) {
            if (line->get_alloc_time() < valid_timestamp) {
              valid_timestamp = line->get_alloc_time();
              valid_line = index;
            }
          }
        }
      } else if ((m_config.m_replacement_policy == LRU &&
                  line->get_last_access_time() < dirty_timestamp) ||
                 (m_config.m_replacement_policy == FIFO &&
                  line->get_alloc_time() < dirty_timestamp)) {
        // Dirty-ratio selection is a preference, never an availability rule.
        // If this set has no clean candidate, retain its oldest dirty line as
        // the conventional replacement fallback.
        dirty_timestamp = (m_config.m_replacement_policy == LRU)
                              ? line->get_last_access_time()
                              : line->get_alloc_time();
        dirty_line = index;
      }
    }
  }
  if (all_reserved) {
    assert(m_config.m_alloc_policy == ON_MISS);
    return RESERVATION_FAIL;  // miss and not enough space in cache to allocate
                              // on miss
  }

  if (invalid_line != (unsigned)-1) {
    idx = invalid_line;
  } else if (valid_line != (unsigned)-1) {
    idx = valid_line;
  } else if (dirty_line != (unsigned)-1) {
    idx = dirty_line;
  } else
    abort();  // if an unreserved block exists, it is either invalid or
              // replaceable

  return MISS;
}

enum cache_request_status tag_array::access(new_addr_type addr, unsigned time,
                                            unsigned &idx, mem_fetch *mf) {
  bool wb = false;
  evicted_block_info evicted;
  enum cache_request_status result = access(addr, time, idx, wb, evicted, mf);
  assert(!wb);
  return result;
}

enum cache_request_status tag_array::access(new_addr_type addr, unsigned time,
                                            unsigned &idx, bool &wb,
                                            evicted_block_info &evicted,
                                            mem_fetch *mf) {
  m_access++;
  is_used = true;
  shader_cache_access_log(m_core_id, m_type_id, 0);  // log accesses to cache
  enum cache_request_status status = probe(addr, idx, mf, mf->is_write());
  switch (status) {
    case HIT_RESERVED:
      m_pending_hit++;
    case HIT:
      m_lines[idx]->set_last_access_time(time, mf->get_access_sector_mask());
      break;
    case MISS:
      m_miss++;
      shader_cache_access_log(m_core_id, m_type_id, 1);  // log cache misses
      if (m_config.m_alloc_policy == ON_MISS) {
        if (m_lines[idx]->is_modified_line()) {
          wb = true;
          // m_lines[idx]->set_byte_mask(mf);
          evicted.set_info(m_lines[idx]->m_block_addr,
                           m_lines[idx]->get_modified_size(),
                           m_lines[idx]->get_dirty_byte_mask(),
                           m_lines[idx]->get_dirty_sector_mask());
          m_dirty--;
        }
        m_lines[idx]->allocate(m_config.tag(addr), m_config.block_addr(addr),
                               time, mf->get_access_sector_mask());
      }
      break;
    case SECTOR_MISS:
      assert(m_config.m_cache_type == SECTOR);
      m_sector_miss++;
      shader_cache_access_log(m_core_id, m_type_id, 1);  // log cache misses
      if (m_config.m_alloc_policy == ON_MISS) {
        bool before = m_lines[idx]->is_modified_line();
        ((sector_cache_block *)m_lines[idx])
            ->allocate_sector(time, mf->get_access_sector_mask());
        if (before && !m_lines[idx]->is_modified_line()) {
          m_dirty--;
        }
      }
      break;
    case RESERVATION_FAIL:
      m_res_fail++;
      shader_cache_access_log(m_core_id, m_type_id, 1);  // log cache misses
      break;
    default:
      fprintf(stderr,
              "tag_array::access - Error: Unknown"
              "cache_request_status %d\n",
              status);
      abort();
  }
  if (status != RESERVATION_FAIL) l2_char_tracking_refresh_line(idx);
  return status;
}

void tag_array::fill(new_addr_type addr, unsigned time, mem_fetch *mf,
                     bool is_write) {
  fill(addr, time, mf->get_access_sector_mask(), mf->get_access_byte_mask(),
       is_write);
}

void tag_array::fill(new_addr_type addr, unsigned time,
                     mem_access_sector_mask_t mask,
                     mem_access_byte_mask_t byte_mask, bool is_write) {
  // assert( m_config.m_alloc_policy == ON_FILL );
  unsigned idx;
  enum cache_request_status status = probe(addr, idx, mask, is_write);

  if (status == RESERVATION_FAIL) {
    return;
  }

  bool before = m_lines[idx]->is_modified_line();
  // assert(status==MISS||status==SECTOR_MISS); // MSHR should have prevented
  // redundant memory request
  if (status == MISS) {
    m_lines[idx]->allocate(m_config.tag(addr), m_config.block_addr(addr), time,
                           mask);
  } else if (status == SECTOR_MISS) {
    assert(m_config.m_cache_type == SECTOR);
    ((sector_cache_block *)m_lines[idx])->allocate_sector(time, mask);
  }
  if (before && !m_lines[idx]->is_modified_line()) {
    m_dirty--;
  }
  before = m_lines[idx]->is_modified_line();
  m_lines[idx]->fill(time, mask, byte_mask);
  if (m_lines[idx]->is_modified_line() && !before) {
    m_dirty++;
  }
  l2_char_tracking_refresh_line(idx);
}

void tag_array::fill(unsigned index, unsigned time, mem_fetch *mf) {
  assert(m_config.m_alloc_policy == ON_MISS);
  bool before = m_lines[index]->is_modified_line();
  m_lines[index]->fill(time, mf->get_access_sector_mask(),
                       mf->get_access_byte_mask());
  if (m_lines[index]->is_modified_line() && !before) {
    m_dirty++;
  }
  l2_char_tracking_refresh_line(index);
}

// TODO: we need write back the flushed data to the upper level
void tag_array::flush() {
  if (!is_used) return;

  for (unsigned i = 0; i < m_config.get_num_lines(); i++)
    if (m_lines[i]->is_modified_line()) {
      for (unsigned j = 0; j < SECTOR_CHUNCK_SIZE; j++) {
        m_lines[i]->set_status(INVALID, mem_access_sector_mask_t().set(j));
      }
    }

  m_dirty = 0;
  for (unsigned i = 0; i < m_config.get_num_lines(); ++i)
    l2_char_tracking_refresh_line(i);
  is_used = false;
}

void tag_array::invalidate() {
  if (!is_used) return;

  for (unsigned i = 0; i < m_config.get_num_lines(); i++)
    for (unsigned j = 0; j < SECTOR_CHUNCK_SIZE; j++)
      m_lines[i]->set_status(INVALID, mem_access_sector_mask_t().set(j));

  m_dirty = 0;
  for (unsigned i = 0; i < m_config.get_num_lines(); ++i)
    l2_char_tracking_refresh_line(i);
  is_used = false;
}

float tag_array::windowed_miss_rate() const {
  return l2_windowed_miss_rate(m_access, m_prev_snapshot_access, m_miss,
                               m_sector_miss, m_prev_snapshot_miss);
}

void tag_array::new_window() {
  m_prev_snapshot_access = m_access;
  m_prev_snapshot_miss = m_miss + m_sector_miss;
  m_prev_snapshot_pending_hit = m_pending_hit;
}

void tag_array::print(FILE *stream, unsigned &total_access,
                      unsigned &total_misses) const {
  m_config.print(stream);
  fprintf(stream,
          "\t\tAccess = %d, Miss = %d, Sector_Miss = %d, Total_Miss = %d "
          "(%.3g), PendingHit = %d (%.3g)\n",
          m_access, m_miss, m_sector_miss, (m_miss + m_sector_miss),
          (float)(m_miss + m_sector_miss) / m_access, m_pending_hit,
          (float)m_pending_hit / m_access);
  total_misses += (m_miss + m_sector_miss);
  total_access += m_access;
}

void tag_array::get_stats(unsigned &total_access, unsigned &total_misses,
                          unsigned &total_hit_res,
                          unsigned &total_res_fail) const {
  // Update statistics from the tag array
  total_access = m_access;
  total_misses = (m_miss + m_sector_miss);
  total_hit_res = m_pending_hit;
  total_res_fail = m_res_fail;
}

bool was_write_sent(const std::list<cache_event> &events) {
  for (std::list<cache_event>::const_iterator e = events.begin();
       e != events.end(); e++) {
    if ((*e).m_cache_event_type == WRITE_REQUEST_SENT) return true;
  }
  return false;
}

bool was_writeback_sent(const std::list<cache_event> &events,
                        cache_event &wb_event) {
  for (std::list<cache_event>::const_iterator e = events.begin();
       e != events.end(); e++) {
    if ((*e).m_cache_event_type == WRITE_BACK_REQUEST_SENT) {
      wb_event = *e;
      return true;
    }
  }
  return false;
}

bool was_read_sent(const std::list<cache_event> &events) {
  for (std::list<cache_event>::const_iterator e = events.begin();
       e != events.end(); e++) {
    if ((*e).m_cache_event_type == READ_REQUEST_SENT) return true;
  }
  return false;
}

bool was_writeallocate_sent(const std::list<cache_event> &events) {
  for (std::list<cache_event>::const_iterator e = events.begin();
       e != events.end(); e++) {
    if ((*e).m_cache_event_type == WRITE_ALLOCATE_SENT) return true;
  }
  return false;
}
/****************************************************************** MSHR
 * ******************************************************************/

/// Checks if there is a pending request to the lower memory level already
bool mshr_table::probe(new_addr_type block_addr) const {
  table::const_iterator a = m_data.find(block_addr);
  return a != m_data.end();
}

/// Checks if there is space for tracking a new memory access
bool mshr_table::full(new_addr_type block_addr) const {
  return full_reason(block_addr) != EP_L2_BLOCK_NONE;
}

mshr_table::ep_l2_block_reason mshr_table::full_reason(
    new_addr_type block_addr) const {
  table::const_iterator i = m_data.find(block_addr);
  if (i != m_data.end()) {
    const unsigned per_line_limit =
        m_descriptor_pool_size ? m_descriptor_per_line_cap : m_max_merged;
    if (i->second.m_list.size() >= per_line_limit)
      return m_descriptor_pool_size ? EP_L2_BLOCK_PER_ADDRESS_CAP
                                    : EP_L2_BLOCK_LINE_MSHR_FULL;
  } else if (m_data.size() >= m_num_entries) {
    return EP_L2_BLOCK_LINE_MSHR_FULL;
  }
  if (m_descriptor_pool_size && m_free_descriptor_ids.empty())
    return EP_L2_BLOCK_DESCRIPTOR_POOL_FULL;
  return EP_L2_BLOCK_NONE;
}

bool mshr_table::needs_lower_read(
    new_addr_type block_addr, const mem_access_sector_mask_t &sectors) const {
  if (!m_descriptor_pool_size) return !probe(block_addr);
  table::const_iterator entry = m_data.find(block_addr);
  if (entry == m_data.end()) return true;
  return (sectors & ~entry->second.m_issued_sectors).any();
}

/// Add or merge this access
void mshr_table::add(new_addr_type block_addr, mem_fetch *mf) {
  mshr_entry &entry = m_data[block_addr];
  entry.m_list.push_back(mf);
  if (m_descriptor_pool_size) {
    assert(!m_free_descriptor_ids.empty());
    unsigned id = m_free_descriptor_ids.back();
    m_free_descriptor_ids.pop_back();
    ep_l2_descriptor &descriptor = m_descriptor_pool[id];
    descriptor.m_mf = mf;
    descriptor.m_sectors = mf->get_access_sector_mask();
    descriptor.m_response_queued = false;
    entry.m_descriptor_ids.push_back(id);
    entry.m_pending_sectors |= descriptor.m_sectors;
    entry.m_issued_sectors |= descriptor.m_sectors;
  }
  assert(m_data.size() <= m_num_entries);
  assert(entry.m_list.size() <=
         (m_descriptor_pool_size ? m_descriptor_per_line_cap : m_max_merged));
  // indicate that this MSHR entry contains an atomic operation
  if (mf->isatomic()) {
    entry.m_has_atomic = true;
  }
}

void mshr_table::add_for_test(new_addr_type block_addr, mem_fetch *mf,
                              const mem_access_sector_mask_t &sectors) {
  assert(m_descriptor_pool_size != 0);
  assert(full_reason(block_addr) == EP_L2_BLOCK_NONE);
  mshr_entry &entry = m_data[block_addr];
  entry.m_list.push_back(mf);
  unsigned id = m_free_descriptor_ids.back();
  m_free_descriptor_ids.pop_back();
  ep_l2_descriptor &descriptor = m_descriptor_pool[id];
  descriptor.m_mf = mf;
  descriptor.m_sectors = sectors;
  descriptor.m_response_queued = false;
  entry.m_descriptor_ids.push_back(id);
  entry.m_pending_sectors |= sectors;
  entry.m_issued_sectors |= sectors;
}

/// check is_read_after_write_pending
bool mshr_table::is_read_after_write_pending(new_addr_type block_addr) {
  std::list<mem_fetch *> my_list = m_data[block_addr].m_list;
  bool write_found = false;
  for (std::list<mem_fetch *>::iterator it = my_list.begin();
       it != my_list.end(); ++it) {
    if ((*it)->is_write())  // Pending Write Request
      write_found = true;
    else if (write_found)  // Pending Read Request and we found previous Write
      return true;
  }

  return false;
}

unsigned mshr_table::num_targets_used() const {
  unsigned total = 0;
  for (table::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
    total += it->second.m_list.size();
  }
  return total;
}

bool mshr_table::response_ready(new_addr_type block_addr) const {
  for (std::list<new_addr_type>::const_iterator it =
           m_current_response.begin();
       it != m_current_response.end(); ++it) {
    if (*it == block_addr) return true;
  }
  for (std::list<ready_descriptor>::const_iterator it =
           m_current_descriptor_response.begin();
       it != m_current_descriptor_response.end(); ++it) {
    if (it->m_block_addr == block_addr) return true;
  }
  return false;
}

unsigned mshr_table::num_response_ready_targets() const {
  unsigned total = 0;
  for (std::list<new_addr_type>::const_iterator ready =
           m_current_response.begin();
       ready != m_current_response.end(); ++ready) {
    table::const_iterator entry = m_data.find(*ready);
    assert(entry != m_data.end());
    total += entry->second.m_list.size();
  }
  total += m_current_descriptor_response.size();
  return total;
}

unsigned mshr_table::max_targets_on_one_entry() const {
  unsigned maximum = 0;
  for (table::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
    if (it->second.m_list.size() > maximum)
      maximum = it->second.m_list.size();
  }
  return maximum;
}

void mshr_table::l2_char_states(std::vector<new_addr_type> &addresses,
                                std::vector<unsigned> &targets,
                                std::vector<bool> &ready) const {
  addresses.clear();
  targets.clear();
  ready.clear();
  for (table::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
    addresses.push_back(it->first);
    targets.push_back(it->second.m_list.size());
    ready.push_back(response_ready(it->first));
  }
}

void mshr_table::descriptor_chain_snapshot(
    unsigned &active_entries, unsigned &target_sum, unsigned &maximum,
    unsigned long long *histogram, unsigned histogram_size) const {
  assert(histogram && histogram_size);
  active_entries = target_sum = maximum = 0;
  for (unsigned i = 0; i < histogram_size; ++i) histogram[i] = 0;
  for (table::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
    const unsigned depth = it->second.m_list.size();
    if (!depth) continue;
    ++active_entries;
    target_sum += depth;
    maximum = std::max(maximum, depth);
    ++histogram[std::min<unsigned>(depth, histogram_size - 1)];
  }
}

/// Accept a new cache fill response: mark entry ready for processing
void mshr_table::mark_ready(new_addr_type block_addr, bool &has_atomic,
                            const mem_access_sector_mask_t &sectors) {
  assert(!busy());
  table::iterator a = m_data.find(block_addr);
  assert(a != m_data.end());
  has_atomic = a->second.m_has_atomic;
  if (!m_descriptor_pool_size) {
    m_current_response.push_back(block_addr);
    assert(m_current_response.size() <= m_data.size());
    return;
  }

  mshr_entry &entry = a->second;
  entry.m_ready_sectors |= sectors;
  entry.m_pending_sectors &= ~sectors;
  for (std::list<unsigned>::const_iterator id = entry.m_descriptor_ids.begin();
       id != entry.m_descriptor_ids.end(); ++id) {
    ep_l2_descriptor &descriptor = m_descriptor_pool[*id];
    if (!descriptor.m_response_queued &&
        (descriptor.m_sectors & ~entry.m_ready_sectors).none()) {
      descriptor.m_response_queued = true;
      m_current_descriptor_response.push_back(ready_descriptor(block_addr, *id));
    }
  }
}

mem_fetch *mshr_table::peek_next_access() const {
  assert(access_ready());
  if (!m_current_descriptor_response.empty()) {
    return m_descriptor_pool[m_current_descriptor_response.front().m_descriptor_id]
        .m_mf;
  }
  new_addr_type block_addr = m_current_response.front();
  table::const_iterator entry = m_data.find(block_addr);
  assert(entry != m_data.end() && !entry->second.m_list.empty());
  return entry->second.m_list.front();
}

void mshr_table::commit_next_access() {
  assert(access_ready());
  if (!m_current_descriptor_response.empty()) {
    ready_descriptor ready = m_current_descriptor_response.front();
    table::iterator entry = m_data.find(ready.m_block_addr);
    assert(entry != m_data.end());
    ep_l2_descriptor &descriptor = m_descriptor_pool[ready.m_descriptor_id];
    std::list<unsigned>::iterator id = std::find(
        entry->second.m_descriptor_ids.begin(), entry->second.m_descriptor_ids.end(),
        ready.m_descriptor_id);
    assert(id != entry->second.m_descriptor_ids.end());
    std::list<mem_fetch *>::iterator mf = std::find(
        entry->second.m_list.begin(), entry->second.m_list.end(), descriptor.m_mf);
    assert(mf != entry->second.m_list.end());
    entry->second.m_descriptor_ids.erase(id);
    entry->second.m_list.erase(mf);
    descriptor = ep_l2_descriptor();
    m_free_descriptor_ids.push_back(ready.m_descriptor_id);
    m_current_descriptor_response.pop_front();
    if (entry->second.m_list.empty()) m_data.erase(entry);
    return;
  }

  new_addr_type block_addr = m_current_response.front();
  table::iterator entry = m_data.find(block_addr);
  assert(entry != m_data.end() && !entry->second.m_list.empty());
  entry->second.m_list.pop_front();
  if (entry->second.m_list.empty()) {
    // release entry
    m_data.erase(entry);
    m_current_response.pop_front();
  }
}

/// Returns next ready access
mem_fetch *mshr_table::next_access() {
  mem_fetch *result = peek_next_access();
  commit_next_access();
  return result;
}

void mshr_table::sector_masks(new_addr_type block_addr,
                              mem_access_sector_mask_t &pending,
                              mem_access_sector_mask_t &issued,
                              mem_access_sector_mask_t &ready) const {
  table::const_iterator entry = m_data.find(block_addr);
  assert(entry != m_data.end());
  pending = entry->second.m_pending_sectors;
  issued = entry->second.m_issued_sectors;
  ready = entry->second.m_ready_sectors;
}

void mshr_table::display(FILE *fp) const {
  fprintf(fp, "MSHR contents\n");
  for (table::const_iterator e = m_data.begin(); e != m_data.end(); ++e) {
    unsigned block_addr = e->first;
    fprintf(fp, "MSHR: tag=0x%06x, atomic=%d %zu entries : ", block_addr,
            e->second.m_has_atomic, e->second.m_list.size());
    if (!e->second.m_list.empty()) {
      mem_fetch *mf = e->second.m_list.front();
      fprintf(fp, "%p :", mf);
      mf->print(fp);
    } else {
      fprintf(fp, " no memory requests???\n");
    }
  }
}
/***************************************************************** Caches
 * *****************************************************************/
cache_stats::cache_stats() {
  m_cache_port_available_cycles = 0;
  m_cache_data_port_busy_cycles = 0;
  m_cache_fill_port_busy_cycles = 0;
}

void cache_stats::clear() {
  ///
  /// Zero out all current cache statistics
  ///
  m_stats.clear();
  m_stats_pw.clear();
  m_fail_stats.clear();

  m_cache_port_available_cycles = 0;
  m_cache_data_port_busy_cycles = 0;
  m_cache_fill_port_busy_cycles = 0;
}

void cache_stats::clear_pw() {
  ///
  /// Zero out per-window cache statistics
  ///
  m_stats_pw.clear();
}

void cache_stats::inc_stats(int access_type, int access_outcome,
                            unsigned long long streamID) {
  ///
  /// Increment the stat corresponding to (access_type, access_outcome) by 1.
  ///
  if (!check_valid(access_type, access_outcome))
    assert(0 && "Unknown cache access type or access outcome");

  if (m_stats.find(streamID) == m_stats.end()) {
    std::vector<std::vector<unsigned long long>> new_val;
    new_val.resize(NUM_MEM_ACCESS_TYPE);
    for (unsigned j = 0; j < NUM_MEM_ACCESS_TYPE; ++j) {
      new_val[j].resize(NUM_CACHE_REQUEST_STATUS, 0);
    }
    m_stats.insert(std::pair<unsigned long long,
                             std::vector<std::vector<unsigned long long>>>(
        streamID, new_val));
  }
  m_stats.at(streamID)[access_type][access_outcome]++;
}

void cache_stats::inc_stats_pw(int access_type, int access_outcome,
                               unsigned long long streamID) {
  ///
  /// Increment the corresponding per-window cache stat
  ///
  if (!check_valid(access_type, access_outcome))
    assert(0 && "Unknown cache access type or access outcome");

  if (m_stats_pw.find(streamID) == m_stats_pw.end()) {
    std::vector<std::vector<unsigned long long>> new_val;
    new_val.resize(NUM_MEM_ACCESS_TYPE);
    for (unsigned j = 0; j < NUM_MEM_ACCESS_TYPE; ++j) {
      new_val[j].resize(NUM_CACHE_REQUEST_STATUS, 0);
    }
    m_stats_pw.insert(std::pair<unsigned long long,
                                std::vector<std::vector<unsigned long long>>>(
        streamID, new_val));
  }
  m_stats_pw.at(streamID)[access_type][access_outcome]++;
}

void cache_stats::inc_fail_stats(int access_type, int fail_outcome,
                                 unsigned long long streamID) {
  if (!check_fail_valid(access_type, fail_outcome))
    assert(0 && "Unknown cache access type or access fail");

  if (m_fail_stats.find(streamID) == m_fail_stats.end()) {
    std::vector<std::vector<unsigned long long>> new_val;
    new_val.resize(NUM_MEM_ACCESS_TYPE);
    for (unsigned j = 0; j < NUM_MEM_ACCESS_TYPE; ++j) {
      new_val[j].resize(NUM_CACHE_RESERVATION_FAIL_STATUS, 0);
    }
    m_fail_stats.insert(std::pair<unsigned long long,
                                  std::vector<std::vector<unsigned long long>>>(
        streamID, new_val));
  }
  m_fail_stats.at(streamID)[access_type][fail_outcome]++;
}

enum cache_request_status cache_stats::select_stats_status(
    enum cache_request_status probe, enum cache_request_status access) const {
  ///
  /// This function selects how the cache access outcome should be counted.
  /// HIT_RESERVED is considered as a MISS in the cores, however, it should be
  /// counted as a HIT_RESERVED in the caches.
  ///
  if (probe == HIT_RESERVED && access != RESERVATION_FAIL)
    return probe;
  else if (probe == SECTOR_MISS && access == MISS)
    return probe;
  else
    return access;
}

unsigned long long &cache_stats::operator()(int access_type, int access_outcome,
                                            bool fail_outcome,
                                            unsigned long long streamID) {
  ///
  /// Simple method to read/modify the stat corresponding to (access_type,
  /// access_outcome) Used overloaded () to avoid the need for separate
  /// read/write member functions
  ///
  if (fail_outcome) {
    if (!check_fail_valid(access_type, access_outcome))
      assert(0 && "Unknown cache access type or fail outcome");

    return m_fail_stats.at(streamID)[access_type][access_outcome];
  } else {
    if (!check_valid(access_type, access_outcome))
      assert(0 && "Unknown cache access type or access outcome");

    return m_stats.at(streamID)[access_type][access_outcome];
  }
}

unsigned long long cache_stats::operator()(int access_type, int access_outcome,
                                           bool fail_outcome,
                                           unsigned long long streamID) const {
  ///
  /// Const accessor into m_stats.
  ///
  if (fail_outcome) {
    if (!check_fail_valid(access_type, access_outcome))
      assert(0 && "Unknown cache access type or fail outcome");

    return m_fail_stats.at(streamID)[access_type][access_outcome];
  } else {
    if (!check_valid(access_type, access_outcome))
      assert(0 && "Unknown cache access type or access outcome");

    return m_stats.at(streamID)[access_type][access_outcome];
  }
}

unsigned long long cache_stats::total_fail_reason(
    enum cache_reservation_fail_reason reason) const {
  unsigned long long total = 0;
  for (std::map<unsigned long long,
                std::vector<std::vector<unsigned long long>>>::const_iterator
           stream = m_fail_stats.begin(); stream != m_fail_stats.end(); ++stream)
    for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type)
      total += stream->second[type][reason];
  return total;
}

cache_stats cache_stats::operator+(const cache_stats &cs) {
  ///
  /// Overloaded + operator to allow for simple stat accumulation
  ///
  cache_stats ret;
  for (auto iter = m_stats.begin(); iter != m_stats.end(); ++iter) {
    unsigned long long streamID = iter->first;
    ret.m_stats.insert(std::pair<unsigned long long,
                                 std::vector<std::vector<unsigned long long>>>(
        streamID, m_stats.at(streamID)));
  }
  for (auto iter = m_stats_pw.begin(); iter != m_stats_pw.end(); ++iter) {
    unsigned long long streamID = iter->first;
    ret.m_stats_pw.insert(
        std::pair<unsigned long long,
                  std::vector<std::vector<unsigned long long>>>(
            streamID, m_stats_pw.at(streamID)));
  }
  for (auto iter = m_fail_stats.begin(); iter != m_fail_stats.end(); ++iter) {
    unsigned long long streamID = iter->first;
    ret.m_fail_stats.insert(
        std::pair<unsigned long long,
                  std::vector<std::vector<unsigned long long>>>(
            streamID, m_fail_stats.at(streamID)));
  }
  for (auto iter = cs.m_stats.begin(); iter != cs.m_stats.end(); ++iter) {
    unsigned long long streamID = iter->first;
    if (ret.m_stats.find(streamID) == ret.m_stats.end()) {
      ret.m_stats.insert(
          std::pair<unsigned long long,
                    std::vector<std::vector<unsigned long long>>>(
              streamID, cs.m_stats.at(streamID)));
    } else {
      for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
        for (unsigned status = 0; status < NUM_CACHE_REQUEST_STATUS; ++status) {
          ret.m_stats.at(streamID)[type][status] +=
              cs(type, status, false, streamID);
        }
      }
    }
  }
  for (auto iter = cs.m_stats_pw.begin(); iter != cs.m_stats_pw.end(); ++iter) {
    unsigned long long streamID = iter->first;
    if (ret.m_stats_pw.find(streamID) == ret.m_stats_pw.end()) {
      ret.m_stats_pw.insert(
          std::pair<unsigned long long,
                    std::vector<std::vector<unsigned long long>>>(
              streamID, cs.m_stats_pw.at(streamID)));
    } else {
      for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
        for (unsigned status = 0; status < NUM_CACHE_REQUEST_STATUS; ++status) {
          ret.m_stats_pw.at(streamID)[type][status] +=
              cs(type, status, false, streamID);
        }
      }
    }
  }
  for (auto iter = cs.m_fail_stats.begin(); iter != cs.m_fail_stats.end();
       ++iter) {
    unsigned long long streamID = iter->first;
    if (ret.m_fail_stats.find(streamID) == ret.m_fail_stats.end()) {
      ret.m_fail_stats.insert(
          std::pair<unsigned long long,
                    std::vector<std::vector<unsigned long long>>>(
              streamID, cs.m_fail_stats.at(streamID)));
    } else {
      for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
        for (unsigned status = 0; status < NUM_CACHE_RESERVATION_FAIL_STATUS;
             ++status) {
          ret.m_fail_stats.at(streamID)[type][status] +=
              cs(type, status, true, streamID);
        }
      }
    }
  }
  ret.m_cache_port_available_cycles =
      m_cache_port_available_cycles + cs.m_cache_port_available_cycles;
  ret.m_cache_data_port_busy_cycles =
      m_cache_data_port_busy_cycles + cs.m_cache_data_port_busy_cycles;
  ret.m_cache_fill_port_busy_cycles =
      m_cache_fill_port_busy_cycles + cs.m_cache_fill_port_busy_cycles;
  return ret;
}

cache_stats &cache_stats::operator+=(const cache_stats &cs) {
  ///
  /// Overloaded += operator to allow for simple stat accumulation
  ///
  for (auto iter = cs.m_stats.begin(); iter != cs.m_stats.end(); ++iter) {
    unsigned long long streamID = iter->first;
    if (m_stats.find(streamID) == m_stats.end()) {
      m_stats.insert(std::pair<unsigned long long,
                               std::vector<std::vector<unsigned long long>>>(
          streamID, cs.m_stats.at(streamID)));
    } else {
      for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
        for (unsigned status = 0; status < NUM_CACHE_REQUEST_STATUS; ++status) {
          m_stats.at(streamID)[type][status] +=
              cs(type, status, false, streamID);
        }
      }
    }
  }
  for (auto iter = cs.m_stats_pw.begin(); iter != cs.m_stats_pw.end(); ++iter) {
    unsigned long long streamID = iter->first;
    if (m_stats_pw.find(streamID) == m_stats_pw.end()) {
      m_stats_pw.insert(std::pair<unsigned long long,
                                  std::vector<std::vector<unsigned long long>>>(
          streamID, cs.m_stats_pw.at(streamID)));
    } else {
      for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
        for (unsigned status = 0; status < NUM_CACHE_REQUEST_STATUS; ++status) {
          m_stats_pw.at(streamID)[type][status] +=
              cs(type, status, false, streamID);
        }
      }
    }
  }
  for (auto iter = cs.m_fail_stats.begin(); iter != cs.m_fail_stats.end();
       ++iter) {
    unsigned long long streamID = iter->first;
    if (m_fail_stats.find(streamID) == m_fail_stats.end()) {
      m_fail_stats.insert(
          std::pair<unsigned long long,
                    std::vector<std::vector<unsigned long long>>>(
              streamID, cs.m_fail_stats.at(streamID)));
    } else {
      for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
        for (unsigned status = 0; status < NUM_CACHE_RESERVATION_FAIL_STATUS;
             ++status) {
          m_fail_stats.at(streamID)[type][status] +=
              cs(type, status, true, streamID);
        }
      }
    }
  }
  m_cache_port_available_cycles += cs.m_cache_port_available_cycles;
  m_cache_data_port_busy_cycles += cs.m_cache_data_port_busy_cycles;
  m_cache_fill_port_busy_cycles += cs.m_cache_fill_port_busy_cycles;
  return *this;
}

void cache_stats::print_stats(FILE *fout, unsigned long long streamID,
                              const char *cache_name) const {
  ///
  /// For a given CUDA stream, print out each non-zero cache statistic for every
  /// memory access type and status "cache_name" defaults to "Cache_stats" when
  /// no argument is provided, otherwise the provided name is used. The printed
  /// format is
  /// "<cache_name>[<request_type>][<request_status>] = <stat_value>"
  /// Specify streamID to be -1 to print every stream.

  std::vector<unsigned> total_access;
  std::string m_cache_name = cache_name;
  for (auto iter = m_stats.begin(); iter != m_stats.end(); ++iter) {
    unsigned long long streamid = iter->first;
    // when streamID is specified, skip stats for all other streams, otherwise,
    // print stats from all streams
    if ((streamID != -1) && (streamid != streamID)) continue;
    total_access.clear();
    total_access.resize(NUM_MEM_ACCESS_TYPE, 0);
    for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
      for (unsigned status = 0; status < NUM_CACHE_REQUEST_STATUS; ++status) {
        fprintf(fout, "\t%s[%s][%s] = %llu\n", m_cache_name.c_str(),
                mem_access_type_str((enum mem_access_type)type),
                cache_request_status_str((enum cache_request_status)status),
                m_stats.at(streamid)[type][status]);

        if (status != RESERVATION_FAIL && status != MSHR_HIT &&
            status != WRITE_ALLOCATED)
          // MSHR_HIT is a special type of SECTOR_MISS
          // so its already included in the SECTOR_MISS.
          // WRITE_ALLOCATE is a supplementary bucket (the access is also
          // counted as MISS), so it must NOT be added to TOTAL_ACCESS.
          total_access[type] += m_stats.at(streamid)[type][status];
      }
    }
    for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
      if (total_access[type] > 0)
        fprintf(fout, "\t%s[%s][%s] = %u\n", m_cache_name.c_str(),
                mem_access_type_str((enum mem_access_type)type), "TOTAL_ACCESS",
                total_access[type]);
    }
  }
}

void cache_stats::print_fail_stats(FILE *fout, unsigned long long streamID,
                                   const char *cache_name) const {
  std::string m_cache_name = cache_name;
  for (auto iter = m_fail_stats.begin(); iter != m_fail_stats.end(); ++iter) {
    unsigned long long streamid = iter->first;
    // when streamID is specified, skip stats for all other streams, otherwise,
    // print stats from all streams
    if ((streamID != -1) && (streamid != streamID)) continue;
    for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
      for (unsigned fail = 0; fail < NUM_CACHE_RESERVATION_FAIL_STATUS;
           ++fail) {
        if (m_fail_stats.at(streamid)[type][fail] > 0) {
          fprintf(
              fout, "\t%s[%s][%s] = %llu\n", m_cache_name.c_str(),
              mem_access_type_str((enum mem_access_type)type),
              cache_fail_status_str((enum cache_reservation_fail_reason)fail),
              m_fail_stats.at(streamid)[type][fail]);
        }
      }
    }
  }
}

void cache_sub_stats::print_port_stats(FILE *fout,
                                       const char *cache_name) const {
  float data_port_util = 0.0f;
  if (port_available_cycles > 0) {
    data_port_util = (float)data_port_busy_cycles / port_available_cycles;
  }
  fprintf(fout, "%s_data_port_util = %.3f\n", cache_name, data_port_util);
  float fill_port_util = 0.0f;
  if (port_available_cycles > 0) {
    fill_port_util = (float)fill_port_busy_cycles / port_available_cycles;
  }
  fprintf(fout, "%s_fill_port_util = %.3f\n", cache_name, fill_port_util);
}

unsigned long long cache_stats::get_stats(
    enum mem_access_type *access_type, unsigned num_access_type,
    enum cache_request_status *access_status,
    unsigned num_access_status) const {
  ///
  /// Returns a sum of the stats corresponding to each "access_type" and
  /// "access_status" pair. "access_type" is an array of "num_access_type"
  /// mem_access_types. "access_status" is an array of "num_access_status"
  /// cache_request_statuses.
  ///
  unsigned long long total = 0;
  for (auto iter = m_stats.begin(); iter != m_stats.end(); ++iter) {
    unsigned long long streamID = iter->first;
    for (unsigned type = 0; type < num_access_type; ++type) {
      for (unsigned status = 0; status < num_access_status; ++status) {
        if (!check_valid((int)access_type[type], (int)access_status[status]))
          assert(0 && "Unknown cache access type or access outcome");
        total += m_stats.at(streamID)[access_type[type]][access_status[status]];
      }
    }
  }
  return total;
}

void cache_stats::get_sub_stats(struct cache_sub_stats &css) const {
  ///
  /// Overwrites "css" with the appropriate statistics from this cache.
  ///
  struct cache_sub_stats t_css;
  t_css.clear();

  for (auto iter = m_stats.begin(); iter != m_stats.end(); ++iter) {
    unsigned long long streamID = iter->first;
    for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
      for (unsigned status = 0; status < NUM_CACHE_REQUEST_STATUS; ++status) {
        if (status == HIT || status == MISS || status == SECTOR_MISS ||
            status == HIT_RESERVED)
          t_css.accesses += m_stats.at(streamID)[type][status];

        if (status == MISS || status == SECTOR_MISS)
          t_css.misses += m_stats.at(streamID)[type][status];

        if (status == HIT_RESERVED)
          t_css.pending_hits += m_stats.at(streamID)[type][status];

        if (status == RESERVATION_FAIL)
          t_css.res_fails += m_stats.at(streamID)[type][status];
      }
    }
  }

  t_css.port_available_cycles = m_cache_port_available_cycles;
  t_css.data_port_busy_cycles = m_cache_data_port_busy_cycles;
  t_css.fill_port_busy_cycles = m_cache_fill_port_busy_cycles;

  css = t_css;
}

void cache_stats::get_sub_stats_pw(struct cache_sub_stats_pw &css) const {
  ///
  /// Overwrites "css" with the appropriate statistics from this cache.
  ///
  struct cache_sub_stats_pw t_css;
  t_css.clear();

  for (auto iter = m_stats_pw.begin(); iter != m_stats_pw.end(); ++iter) {
    unsigned long long streamID = iter->first;
    for (unsigned type = 0; type < NUM_MEM_ACCESS_TYPE; ++type) {
      for (unsigned status = 0; status < NUM_CACHE_REQUEST_STATUS; ++status) {
        if (status == HIT || status == MISS || status == SECTOR_MISS ||
            status == HIT_RESERVED)
          t_css.accesses += m_stats_pw.at(streamID)[type][status];

        if (status == HIT) {
          if (type == GLOBAL_ACC_R || type == CONST_ACC_R ||
              type == INST_ACC_R) {
            t_css.read_hits += m_stats_pw.at(streamID)[type][status];
          } else if (type == GLOBAL_ACC_W) {
            t_css.write_hits += m_stats_pw.at(streamID)[type][status];
          }
        }

        if (status == MISS || status == SECTOR_MISS) {
          if (type == GLOBAL_ACC_R || type == CONST_ACC_R ||
              type == INST_ACC_R) {
            t_css.read_misses += m_stats_pw.at(streamID)[type][status];
          } else if (type == GLOBAL_ACC_W) {
            t_css.write_misses += m_stats_pw.at(streamID)[type][status];
          }
        }

        if (status == HIT_RESERVED) {
          if (type == GLOBAL_ACC_R || type == CONST_ACC_R ||
              type == INST_ACC_R) {
            t_css.read_pending_hits += m_stats_pw.at(streamID)[type][status];
          } else if (type == GLOBAL_ACC_W) {
            t_css.write_pending_hits += m_stats_pw.at(streamID)[type][status];
          }
        }

        if (status == RESERVATION_FAIL) {
          if (type == GLOBAL_ACC_R || type == CONST_ACC_R ||
              type == INST_ACC_R) {
            t_css.read_res_fails += m_stats_pw.at(streamID)[type][status];
          } else if (type == GLOBAL_ACC_W) {
            t_css.write_res_fails += m_stats_pw.at(streamID)[type][status];
          }
        }
      }
    }
  }

  css = t_css;
}

bool cache_stats::check_valid(int type, int status) const {
  ///
  /// Verify a valid access_type/access_status
  ///
  if ((type >= 0) && (type < NUM_MEM_ACCESS_TYPE) && (status >= 0) &&
      (status < NUM_CACHE_REQUEST_STATUS))
    return true;
  else
    return false;
}

bool cache_stats::check_fail_valid(int type, int fail) const {
  ///
  /// Verify a valid access_type/access_status
  ///
  if ((type >= 0) && (type < NUM_MEM_ACCESS_TYPE) && (fail >= 0) &&
      (fail < NUM_CACHE_RESERVATION_FAIL_STATUS))
    return true;
  else
    return false;
}

void cache_stats::sample_cache_port_utility(bool data_port_busy,
                                            bool fill_port_busy) {
  m_cache_port_available_cycles += 1;
  if (data_port_busy) {
    m_cache_data_port_busy_cycles += 1;
  }
  if (fill_port_busy) {
    m_cache_fill_port_busy_cycles += 1;
  }
}

baseline_cache::bandwidth_management::bandwidth_management(cache_config &config)
    : m_config(config) {
  m_data_port_occupied_cycles = 0;
  m_fill_port_occupied_cycles = 0;
}

/// use the data port based on the outcome and events generated by the mem_fetch
/// request
void baseline_cache::bandwidth_management::use_data_port(
    mem_fetch *mf, enum cache_request_status outcome,
    const std::list<cache_event> &events) {
  unsigned data_size = mf->get_data_size();
  unsigned port_width = m_config.m_data_port_width;
  switch (outcome) {
    case HIT: {
      unsigned data_cycles =
          data_size / port_width + ((data_size % port_width > 0) ? 1 : 0);
      m_data_port_occupied_cycles += data_cycles;
    } break;
    case HIT_RESERVED:
    case MISS: {
      // the data array is accessed to read out the entire line for write-back
      // in case of sector cache we need to write bank only the modified sectors
      cache_event ev(WRITE_BACK_REQUEST_SENT);
      if (was_writeback_sent(events, ev)) {
        unsigned data_cycles = ev.m_evicted_block.m_modified_size / port_width;
        m_data_port_occupied_cycles += data_cycles;
      }
    } break;
    case SECTOR_MISS:
    case RESERVATION_FAIL:
      // Does not consume any port bandwidth
      break;
    default:
      assert(0);
      break;
  }
}

/// use the fill port
void baseline_cache::bandwidth_management::use_fill_port(mem_fetch *mf) {
  // assume filling the entire line with the returned request
  unsigned fill_cycles = m_config.get_atom_sz() / m_config.m_data_port_width;
  m_fill_port_occupied_cycles += fill_cycles;
}

/// called every cache cycle to free up the ports
void baseline_cache::bandwidth_management::replenish_port_bandwidth() {
  if (m_data_port_occupied_cycles > 0) {
    m_data_port_occupied_cycles -= 1;
  }
  assert(m_data_port_occupied_cycles >= 0);

  if (m_fill_port_occupied_cycles > 0) {
    m_fill_port_occupied_cycles -= 1;
  }
  assert(m_fill_port_occupied_cycles >= 0);
}

/// query for data port availability
bool baseline_cache::bandwidth_management::data_port_free() const {
  return (m_data_port_occupied_cycles == 0);
}

/// query for fill port availability
bool baseline_cache::bandwidth_management::fill_port_free() const {
  return (m_fill_port_occupied_cycles == 0);
}

/// Sends next request to lower level of memory
void baseline_cache::cycle() {
  if (!m_miss_queue.empty()) {
    mem_fetch *mf = m_miss_queue.front();
    if (!m_memport->full(mf->size(), mf->get_is_write())) {
      m_miss_queue.pop_front();
      m_memport->push(mf);
    }
  }
  bool data_port_busy = !m_bandwidth_management.data_port_free();
  bool fill_port_busy = !m_bandwidth_management.fill_port_free();
  // Keep the characterization observation aligned with the native statistics.
  // This is intentionally before replenish_port_bandwidth(); it must not affect
  // availability, admission, or fill semantics later in this cache cycle.
  m_l2_char_data_port_busy_snapshot = data_port_busy;
  m_l2_char_fill_port_busy_snapshot = fill_port_busy;
  m_stats.sample_cache_port_utility(data_port_busy, fill_port_busy);
  m_bandwidth_management.replenish_port_bandwidth();
}

void baseline_cache::miss_queue_class_counts(unsigned &demand,
                                             unsigned &writeback,
                                             unsigned &other_write) const {
  demand = 0;
  writeback = 0;
  other_write = 0;
  for (std::list<mem_fetch *>::const_iterator it = m_miss_queue.begin();
       it != m_miss_queue.end(); ++it) {
    mem_fetch *mf = *it;
    if (mf->get_access_type() == L1_WRBK_ACC ||
        mf->get_access_type() == L2_WRBK_ACC) {
      writeback++;
    } else if (mf->get_is_write()) {
      other_write++;
    } else {
      demand++;
    }
  }
}

/// Interface for response from lower memory level (model bandwidth restictions
/// in caller)
void baseline_cache::fill(mem_fetch *mf, unsigned time) {
  if (m_config.m_mshr_type == SECTOR_ASSOC) {
    assert(mf->get_original_mf());
    extra_mf_fields_lookup::iterator e =
        m_extra_mf_fields.find(mf->get_original_mf());
    assert(e != m_extra_mf_fields.end());
    e->second.pending_read--;

    if (e->second.pending_read > 0) {
      // wait for the other requests to come back
      delete mf;
      return;
    } else {
      mem_fetch *temp = mf;
      mf = mf->get_original_mf();
      delete temp;
    }
  }

  extra_mf_fields_lookup::iterator e = m_extra_mf_fields.find(mf);
  assert(e != m_extra_mf_fields.end());
  assert(e->second.m_valid);
  mf->set_data_size(e->second.m_data_size);
  mf->set_addr(e->second.m_addr);
  if (m_config.m_alloc_policy == ON_MISS)
    m_tag_array->fill(e->second.m_cache_index, time, mf);
  else if (m_config.m_alloc_policy == ON_FILL) {
    m_tag_array->fill(e->second.m_block_addr, time, mf, mf->is_write());
  } else
    abort();
  bool has_atomic = false;
  m_mshrs.mark_ready(e->second.m_block_addr, has_atomic,
                      mf->get_access_sector_mask());
  if (has_atomic) {
    assert(m_config.m_alloc_policy == ON_MISS);
    cache_block_t *block = m_tag_array->get_block(e->second.m_cache_index);
    if (!block->is_modified_line()) {
      m_tag_array->inc_dirty();
    }
    block->set_status(MODIFIED,
                      mf->get_access_sector_mask());  // mark line as dirty for
                                                      // atomic operation
    block->set_byte_mask(mf);
  }
  m_tag_array->l2_char_tracking_refresh_line(e->second.m_cache_index);
  m_extra_mf_fields.erase(mf);
  // EP-L2 payload modes admit returned data through l2_cache's target RAM.
  // Do not create a second legacy FillPort bottleneck for the same fill.
  if (!m_config.m_ep_l2_payload_mode)
    m_bandwidth_management.use_fill_port(mf);
}

/// Checks if mf is waiting to be filled by lower memory level
bool baseline_cache::waiting_for_fill(mem_fetch *mf) {
  extra_mf_fields_lookup::iterator e = m_extra_mf_fields.find(mf);
  return e != m_extra_mf_fields.end();
}

void baseline_cache::print(FILE *fp, unsigned &accesses,
                           unsigned &misses) const {
  fprintf(fp, "Cache %s:\t", m_name.c_str());
  m_tag_array->print(fp, accesses, misses);
}

void baseline_cache::display_state(FILE *fp) const {
  fprintf(fp, "Cache %s:\n", m_name.c_str());
  m_mshrs.display(fp);
  fprintf(fp, "\n");
}

void baseline_cache::inc_aggregated_stats(cache_request_status status,
                                          cache_request_status cache_status,
                                          mem_fetch *mf,
                                          enum cache_gpu_level level) {
  if (level == L1_GPU_CACHE) {
    m_gpu->aggregated_l1_stats.inc_stats(
        mf->get_streamID(), mf->get_access_type(),
        m_gpu->aggregated_l1_stats.select_stats_status(status, cache_status));
  } else if (level == L2_GPU_CACHE) {
    m_gpu->aggregated_l2_stats.inc_stats(
        mf->get_streamID(), mf->get_access_type(),
        m_gpu->aggregated_l2_stats.select_stats_status(status, cache_status));
  }
}

void baseline_cache::inc_aggregated_fail_stats(
    cache_request_status status, cache_request_status cache_status,
    mem_fetch *mf, enum cache_gpu_level level) {
  if (level == L1_GPU_CACHE) {
    m_gpu->aggregated_l1_stats.inc_fail_stats(
        mf->get_streamID(), mf->get_access_type(),
        m_gpu->aggregated_l1_stats.select_stats_status(status, cache_status));
  } else if (level == L2_GPU_CACHE) {
    m_gpu->aggregated_l2_stats.inc_fail_stats(
        mf->get_streamID(), mf->get_access_type(),
        m_gpu->aggregated_l2_stats.select_stats_status(status, cache_status));
  }
}

void baseline_cache::inc_aggregated_stats_pw(cache_request_status status,
                                             cache_request_status cache_status,
                                             mem_fetch *mf,
                                             enum cache_gpu_level level) {
  if (level == L1_GPU_CACHE) {
    m_gpu->aggregated_l1_stats.inc_stats_pw(
        mf->get_streamID(), mf->get_access_type(),
        m_gpu->aggregated_l1_stats.select_stats_status(status, cache_status));
  } else if (level == L2_GPU_CACHE) {
    m_gpu->aggregated_l2_stats.inc_stats_pw(
        mf->get_streamID(), mf->get_access_type(),
        m_gpu->aggregated_l2_stats.select_stats_status(status, cache_status));
  }
}

/// Read miss handler without writeback
void baseline_cache::send_read_request(new_addr_type addr,
                                       new_addr_type block_addr,
                                       unsigned cache_index, mem_fetch *mf,
                                       unsigned time, bool &do_miss,
                                       std::list<cache_event> &events,
                                       bool read_only, bool wa) {
  bool wb = false;
  evicted_block_info e;
  send_read_request(addr, block_addr, cache_index, mf, time, do_miss, wb, e,
                    events, read_only, wa);
}

/// Read miss handler. Check MSHR hit or MSHR available
void baseline_cache::send_read_request(new_addr_type addr,
                                       new_addr_type block_addr,
                                       unsigned cache_index, mem_fetch *mf,
                                       unsigned time, bool &do_miss, bool &wb,
                                       evicted_block_info &evicted,
                                       std::list<cache_event> &events,
                                       bool read_only, bool wa) {
  new_addr_type mshr_addr = m_config.mshr_addr(mf->get_addr());
  bool mshr_hit = m_mshrs.probe(mshr_addr);
  bool mshr_avail = !m_mshrs.full(mshr_addr);
  bool needs_lower_read =
      m_mshrs.needs_lower_read(mshr_addr, mf->get_access_sector_mask());
  bool lower_issue_slot = m_miss_queue.size() < m_config.m_miss_queue_size;
  if (mshr_hit && mshr_avail && (!needs_lower_read || lower_issue_slot)) {
    if (read_only)
      m_tag_array->access(block_addr, time, cache_index, mf);
    else
      m_tag_array->access(block_addr, time, cache_index, wb, evicted, mf);

    m_mshrs.add(mshr_addr, mf);
    if (needs_lower_read) {
      m_extra_mf_fields[mf] = extra_mf_fields(
          mshr_addr, mf->get_addr(), cache_index, mf->get_data_size(), m_config);
      mf->set_data_size(m_config.get_atom_sz());
      // In EP-L2 descriptor mode, the MSHR key is the line but each lower
      // transaction remains a 32-byte sector request.
      if (!m_config.ep_l2_descriptor_mode()) mf->set_addr(mshr_addr);
      m_miss_queue.push_back(mf);
      mf->set_status(m_miss_queue_status, time);
      if (!wa) events.push_back(cache_event(READ_REQUEST_SENT));
    }
    m_stats.inc_stats(mf->get_access_type(), MSHR_HIT, mf->get_streamID());
    do_miss = true;

  } else if (!mshr_hit && mshr_avail && lower_issue_slot) {
    if (read_only)
      m_tag_array->access(block_addr, time, cache_index, mf);
    else
      m_tag_array->access(block_addr, time, cache_index, wb, evicted, mf);

    m_mshrs.add(mshr_addr, mf);
    m_extra_mf_fields[mf] = extra_mf_fields(
        mshr_addr, mf->get_addr(), cache_index, mf->get_data_size(), m_config);
    mf->set_data_size(m_config.get_atom_sz());
    if (!m_config.ep_l2_descriptor_mode()) mf->set_addr(mshr_addr);
    m_miss_queue.push_back(mf);
    mf->set_status(m_miss_queue_status, time);
    if (!wa) events.push_back(cache_event(READ_REQUEST_SENT));

    do_miss = true;
  } else if ((mshr_hit && needs_lower_read && !lower_issue_slot) ||
             (!mshr_hit && !lower_issue_slot))
    m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                           mf->get_streamID());
  else if (mshr_hit && !mshr_avail)
    m_stats.inc_fail_stats(mf->get_access_type(), MSHR_MERGE_ENRTY_FAIL,
                           mf->get_streamID());
  else if (!mshr_hit && !mshr_avail)
    m_stats.inc_fail_stats(mf->get_access_type(), MSHR_ENRTY_FAIL,
                           mf->get_streamID());
  else
    assert(0);
}

/// Sends write request to lower level memory (write or writeback)
void data_cache::send_write_request(mem_fetch *mf, cache_event request,
                                    unsigned time,
                                    std::list<cache_event> &events) {
  events.push_back(request);
  m_miss_queue.push_back(mf);
  mf->set_status(m_miss_queue_status, time);
}

void data_cache::update_m_readable(mem_fetch *mf, unsigned cache_index) {
  cache_block_t *block = m_tag_array->get_block(cache_index);
  for (unsigned i = 0; i < SECTOR_CHUNCK_SIZE; i++) {
    if (mf->get_access_sector_mask().test(i)) {
      bool all_set = true;
      for (unsigned k = i * SECTOR_SIZE; k < (i + 1) * SECTOR_SIZE; k++) {
        // If any bit in the byte mask (within the sector) is not set,
        // the sector is unreadble
        if (!block->get_dirty_byte_mask().test(k)) {
          all_set = false;
          break;
        }
      }
      if (all_set) block->set_m_readable(true, mf->get_access_sector_mask());
    }
  }
}

/****** Write-hit functions (Set by config file) ******/

/// Write-back hit: Mark block as modified
cache_request_status data_cache::wr_hit_wb(new_addr_type addr,
                                           unsigned cache_index, mem_fetch *mf,
                                           unsigned time,
                                           std::list<cache_event> &events,
                                           enum cache_request_status status) {
  new_addr_type block_addr = m_config.block_addr(addr);
  m_tag_array->access(block_addr, time, cache_index, mf);  // update LRU state
  cache_block_t *block = m_tag_array->get_block(cache_index);
  if (!block->is_modified_line()) {
    m_tag_array->inc_dirty();
  }
  block->set_status(MODIFIED, mf->get_access_sector_mask());
  block->set_byte_mask(mf);
  update_m_readable(mf, cache_index);

  return HIT;
}

/// Write-through hit: Directly send request to lower level memory
cache_request_status data_cache::wr_hit_wt(new_addr_type addr,
                                           unsigned cache_index, mem_fetch *mf,
                                           unsigned time,
                                           std::list<cache_event> &events,
                                           enum cache_request_status status) {
  if (miss_queue_full(0)) {
    m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                           mf->get_streamID());
    return RESERVATION_FAIL;  // cannot handle request this cycle
  }

  new_addr_type block_addr = m_config.block_addr(addr);
  m_tag_array->access(block_addr, time, cache_index, mf);  // update LRU state
  cache_block_t *block = m_tag_array->get_block(cache_index);
  if (!block->is_modified_line()) {
    m_tag_array->inc_dirty();
  }
  block->set_status(MODIFIED, mf->get_access_sector_mask());
  block->set_byte_mask(mf);
  update_m_readable(mf, cache_index);

  // generate a write-through
  send_write_request(mf, cache_event(WRITE_REQUEST_SENT), time, events);

  return HIT;
}

/// Write-evict hit: Send request to lower level memory and invalidate
/// corresponding block
cache_request_status data_cache::wr_hit_we(new_addr_type addr,
                                           unsigned cache_index, mem_fetch *mf,
                                           unsigned time,
                                           std::list<cache_event> &events,
                                           enum cache_request_status status) {
  if (miss_queue_full(0)) {
    m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                           mf->get_streamID());
    return RESERVATION_FAIL;  // cannot handle request this cycle
  }

  // generate a write-through/evict
  cache_block_t *block = m_tag_array->get_block(cache_index);
  send_write_request(mf, cache_event(WRITE_REQUEST_SENT), time, events);

  // Invalidate block
  block->set_status(INVALID, mf->get_access_sector_mask());

  return HIT;
}

/// Global write-evict, local write-back: Useful for private caches
enum cache_request_status data_cache::wr_hit_global_we_local_wb(
    new_addr_type addr, unsigned cache_index, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events, enum cache_request_status status) {
  bool evict = (mf->get_access_type() ==
                GLOBAL_ACC_W);  // evict a line that hits on global memory write
  if (evict)
    return wr_hit_we(addr, cache_index, mf, time, events,
                     status);  // Write-evict
  else
    return wr_hit_wb(addr, cache_index, mf, time, events,
                     status);  // Write-back
}

/****** Write-miss functions (Set by config file) ******/

/// Write-allocate miss: Send write request to lower level memory
// and send a read request for the same block
enum cache_request_status data_cache::wr_miss_wa_naive(
    new_addr_type addr, unsigned cache_index, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events, enum cache_request_status status) {
  new_addr_type block_addr = m_config.block_addr(addr);
  new_addr_type mshr_addr = m_config.mshr_addr(mf->get_addr());

  // Write allocate, maximum 3 requests (write miss, read request, write back
  // request) Conservatively ensure the worst-case request can be handled this
  // cycle
  bool mshr_hit = m_mshrs.probe(mshr_addr);
  bool mshr_avail = !m_mshrs.full(mshr_addr);
  if (miss_queue_full(2) ||
      (!(mshr_hit && mshr_avail) &&
       !(!mshr_hit && mshr_avail &&
         (m_miss_queue.size() < m_config.m_miss_queue_size)))) {
    // check what is the exactly the failure reason
    if (miss_queue_full(2))
      m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                             mf->get_streamID());
    else if (mshr_hit && !mshr_avail)
      m_stats.inc_fail_stats(mf->get_access_type(), MSHR_MERGE_ENRTY_FAIL,
                             mf->get_streamID());
    else if (!mshr_hit && !mshr_avail)
      m_stats.inc_fail_stats(mf->get_access_type(), MSHR_ENRTY_FAIL,
                             mf->get_streamID());
    else
      assert(0);

    return RESERVATION_FAIL;
  }

  send_write_request(mf, cache_event(WRITE_REQUEST_SENT), time, events);
  // Tries to send write allocate request, returns true on success and false on
  // failure
  // if(!send_write_allocate(mf, addr, block_addr, cache_index, time, events))
  //    return RESERVATION_FAIL;

  const mem_access_t *ma =
      new mem_access_t(m_wr_alloc_type, mf->get_addr(), m_config.get_atom_sz(),
                       false,  // Now performing a read
                       mf->get_access_warp_mask(), mf->get_access_byte_mask(),
                       mf->get_access_sector_mask(), m_gpu->gpgpu_ctx);

  mem_fetch *n_mf = new mem_fetch(
      *ma, NULL, mf->get_streamID(), mf->get_ctrl_size(), mf->get_wid(),
      mf->get_sid(), mf->get_tpc(), mf->get_mem_config(),
      m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle);

  bool do_miss = false;
  bool wb = false;
  evicted_block_info evicted;

  // Send read request resulting from write miss
  send_read_request(addr, block_addr, cache_index, n_mf, time, do_miss, wb,
                    evicted, events, false, true);

  events.push_back(cache_event(WRITE_ALLOCATE_SENT));

  if (do_miss) {
    // If evicted block is modified and not a write-through
    // (already modified lower level)
    if (wb && (m_config.m_write_policy != WRITE_THROUGH)) {
      assert(status ==
             MISS);  // SECTOR_MISS and HIT_RESERVED should not send write back
      mem_fetch *wb = m_memfetch_creator->alloc(
          evicted.m_block_addr, m_wrbk_type, mf->get_access_warp_mask(),
          evicted.m_byte_mask, evicted.m_sector_mask, evicted.m_modified_size,
          true, m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle, -1, -1, -1,
          NULL, mf->get_streamID());
      // the evicted block may have wrong chip id when advanced L2 hashing  is
      // used, so set the right chip address from the original mf
      wb->set_chip(mf->get_tlx_addr().chip);
      wb->set_partition(mf->get_tlx_addr().sub_partition);
      send_write_request(wb, cache_event(WRITE_BACK_REQUEST_SENT, evicted),
                         time, events);
    }
    return MISS;
  }

  return RESERVATION_FAIL;
}

enum cache_request_status data_cache::wr_miss_wa_fetch_on_write(
    new_addr_type addr, unsigned cache_index, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events, enum cache_request_status status) {
  new_addr_type block_addr = m_config.block_addr(addr);
  new_addr_type mshr_addr = m_config.mshr_addr(mf->get_addr());

  if (mf->get_access_byte_mask().count() == m_config.get_atom_sz()) {
    // if the request writes to the whole cache line/sector, then, write and set
    // cache line Modified. and no need to send read request to memory or
    // reserve mshr

    if (miss_queue_full(0)) {
      m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                             mf->get_streamID());
      return RESERVATION_FAIL;  // cannot handle request this cycle
    }

    bool wb = false;
    evicted_block_info evicted;

    cache_request_status status =
        m_tag_array->access(block_addr, time, cache_index, wb, evicted, mf);
    assert(status != HIT);
    cache_block_t *block = m_tag_array->get_block(cache_index);
    if (!block->is_modified_line()) {
      m_tag_array->inc_dirty();
    }
    block->set_status(MODIFIED, mf->get_access_sector_mask());
    block->set_byte_mask(mf);
    if (status == HIT_RESERVED)
      block->set_ignore_on_fill(true, mf->get_access_sector_mask());

    if (status != RESERVATION_FAIL) {
      // If evicted block is modified and not a write-through
      // (already modified lower level)
      if (wb && (m_config.m_write_policy != WRITE_THROUGH)) {
        mem_fetch *wb = m_memfetch_creator->alloc(
            evicted.m_block_addr, m_wrbk_type, mf->get_access_warp_mask(),
            evicted.m_byte_mask, evicted.m_sector_mask, evicted.m_modified_size,
            true, m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle, -1, -1, -1,
            NULL, mf->get_streamID());
        // the evicted block may have wrong chip id when advanced L2 hashing  is
        // used, so set the right chip address from the original mf
        wb->set_chip(mf->get_tlx_addr().chip);
        wb->set_partition(mf->get_tlx_addr().sub_partition);
        send_write_request(wb, cache_event(WRITE_BACK_REQUEST_SENT, evicted),
                           time, events);
      }
      return MISS;
    }
    return RESERVATION_FAIL;
  } else {
    bool mshr_hit = m_mshrs.probe(mshr_addr);
    bool mshr_avail = !m_mshrs.full(mshr_addr);
    if (miss_queue_full(1) ||
        (!(mshr_hit && mshr_avail) &&
         !(!mshr_hit && mshr_avail &&
           (m_miss_queue.size() < m_config.m_miss_queue_size)))) {
      // check what is the exactly the failure reason
      if (miss_queue_full(1))
        m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                               mf->get_streamID());
      else if (mshr_hit && !mshr_avail)
        m_stats.inc_fail_stats(mf->get_access_type(), MSHR_MERGE_ENRTY_FAIL,
                               mf->get_streamID());
      else if (!mshr_hit && !mshr_avail)
        m_stats.inc_fail_stats(mf->get_access_type(), MSHR_ENRTY_FAIL,
                               mf->get_streamID());
      else
        assert(0);

      return RESERVATION_FAIL;
    }

    // prevent Write - Read - Write in pending mshr
    // allowing another write will override the value of the first write, and
    // the pending read request will read incorrect result from the second write
    if (m_mshrs.probe(mshr_addr) &&
        m_mshrs.is_read_after_write_pending(mshr_addr) && mf->is_write()) {
      // assert(0);
      m_stats.inc_fail_stats(mf->get_access_type(), MSHR_RW_PENDING,
                             mf->get_streamID());
      return RESERVATION_FAIL;
    }

    const mem_access_t *ma = new mem_access_t(
        m_wr_alloc_type, mf->get_addr(), m_config.get_atom_sz(),
        false,  // Now performing a read
        mf->get_access_warp_mask(), mf->get_access_byte_mask(),
        mf->get_access_sector_mask(), m_gpu->gpgpu_ctx);

    mem_fetch *n_mf = new mem_fetch(
        *ma, NULL, mf->get_streamID(), mf->get_ctrl_size(), mf->get_wid(),
        mf->get_sid(), mf->get_tpc(), mf->get_mem_config(),
        m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle, NULL, mf);

    new_addr_type block_addr = m_config.block_addr(addr);
    bool do_miss = false;
    bool wb = false;
    evicted_block_info evicted;
    send_read_request(addr, block_addr, cache_index, n_mf, time, do_miss, wb,
                      evicted, events, false, true);

    cache_block_t *block = m_tag_array->get_block(cache_index);
    block->set_modified_on_fill(true, mf->get_access_sector_mask());
    block->set_byte_mask_on_fill(true);

    events.push_back(cache_event(WRITE_ALLOCATE_SENT));

    if (do_miss) {
      // If evicted block is modified and not a write-through
      // (already modified lower level)
      if (wb && (m_config.m_write_policy != WRITE_THROUGH)) {
        mem_fetch *wb = m_memfetch_creator->alloc(
            evicted.m_block_addr, m_wrbk_type, mf->get_access_warp_mask(),
            evicted.m_byte_mask, evicted.m_sector_mask, evicted.m_modified_size,
            true, m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle, -1, -1, -1,
            NULL, mf->get_streamID());
        // the evicted block may have wrong chip id when advanced L2 hashing  is
        // used, so set the right chip address from the original mf
        wb->set_chip(mf->get_tlx_addr().chip);
        wb->set_partition(mf->get_tlx_addr().sub_partition);
        send_write_request(wb, cache_event(WRITE_BACK_REQUEST_SENT, evicted),
                           time, events);
      }
      return MISS;
    }
    return RESERVATION_FAIL;
  }
}

enum cache_request_status data_cache::wr_miss_wa_lazy_fetch_on_read(
    new_addr_type addr, unsigned cache_index, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events, enum cache_request_status status) {
  new_addr_type block_addr = m_config.block_addr(addr);

  // if the request writes to the whole cache line/sector, then, write and set
  // cache line Modified. and no need to send read request to memory or reserve
  // mshr

  if (exact_l2_admission()) {
    // QV100 lazy-fetch writes are locally absorbed.  They require shared
    // lower-queue space only for a real dirty victim writeback, not merely
    // because the queue happens to be full.
    unsigned n_new_entries =
        (status == MISS && m_tag_array->block_is_modified(cache_index)) ? 1 : 0;
    if (!miss_queue_has_slots(n_new_entries)) {
      m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                             mf->get_streamID());
      return RESERVATION_FAIL;
    }
  } else if (miss_queue_full(0)) {
    m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                           mf->get_streamID());
    return RESERVATION_FAIL;  // cannot handle request this cycle
  }

  if (m_config.m_write_policy == WRITE_THROUGH) {
    send_write_request(mf, cache_event(WRITE_REQUEST_SENT), time, events);
  }

  bool wb = false;
  evicted_block_info evicted;

  cache_request_status m_status =
      m_tag_array->access(block_addr, time, cache_index, wb, evicted, mf);
  assert(m_status != HIT);
  cache_block_t *block = m_tag_array->get_block(cache_index);
  if (!block->is_modified_line()) {
    m_tag_array->inc_dirty();
  }
  block->set_status(MODIFIED, mf->get_access_sector_mask());
  block->set_byte_mask(mf);
  if (m_status == HIT_RESERVED) {
    block->set_ignore_on_fill(true, mf->get_access_sector_mask());
    block->set_modified_on_fill(true, mf->get_access_sector_mask());
    block->set_byte_mask_on_fill(true);
  }

  if (mf->get_access_byte_mask().count() == m_config.get_atom_sz()) {
    block->set_m_readable(true, mf->get_access_sector_mask());
  } else {
    block->set_m_readable(false, mf->get_access_sector_mask());
    if (m_status == HIT_RESERVED)
      block->set_readable_on_fill(true, mf->get_access_sector_mask());
  }
  update_m_readable(mf, cache_index);

  if (m_status != RESERVATION_FAIL) {
    // If evicted block is modified and not a write-through
    // (already modified lower level)
    if (wb && (m_config.m_write_policy != WRITE_THROUGH)) {
      mem_fetch *wb = m_memfetch_creator->alloc(
          evicted.m_block_addr, m_wrbk_type, mf->get_access_warp_mask(),
          evicted.m_byte_mask, evicted.m_sector_mask, evicted.m_modified_size,
          true, m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle, -1, -1, -1,
          NULL, mf->get_streamID());
      // the evicted block may have wrong chip id when advanced L2 hashing  is
      // used, so set the right chip address from the original mf
      wb->set_chip(mf->get_tlx_addr().chip);
      wb->set_partition(mf->get_tlx_addr().sub_partition);
      send_write_request(wb, cache_event(WRITE_BACK_REQUEST_SENT, evicted),
                         time, events);
    }
    return MISS;
  }
  return RESERVATION_FAIL;
}

/// No write-allocate miss: Simply send write request to lower level memory
enum cache_request_status data_cache::wr_miss_no_wa(
    new_addr_type addr, unsigned cache_index, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events, enum cache_request_status status) {
  if (miss_queue_full(0)) {
    m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                           mf->get_streamID());
    return RESERVATION_FAIL;  // cannot handle request this cycle
  }

  // on miss, generate write through (no write buffering -- too many threads for
  // that)
  send_write_request(mf, cache_event(WRITE_REQUEST_SENT), time, events);

  return MISS;
}

/****** Read hit functions (Set by config file) ******/

/// Baseline read hit: Update LRU status of block.
// Special case for atomic instructions -> Mark block as modified
enum cache_request_status data_cache::rd_hit_base(
    new_addr_type addr, unsigned cache_index, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events, enum cache_request_status status) {
  new_addr_type block_addr = m_config.block_addr(addr);
  m_tag_array->access(block_addr, time, cache_index, mf);
  // Atomics treated as global read/write requests - Perform read, mark line as
  // MODIFIED
  if (mf->isatomic()) {
    assert(mf->get_access_type() == GLOBAL_ACC_R);
    cache_block_t *block = m_tag_array->get_block(cache_index);
    if (!block->is_modified_line()) {
      m_tag_array->inc_dirty();
    }
    block->set_status(MODIFIED,
                      mf->get_access_sector_mask());  // mark line as
    block->set_byte_mask(mf);
  }
  return HIT;
}

/****** Read miss functions (Set by config file) ******/

/// Baseline read miss: Send read request to lower level memory,
// perform write-back as necessary
enum cache_request_status data_cache::rd_miss_base(
    new_addr_type addr, unsigned cache_index, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events, enum cache_request_status status) {
  if (exact_l2_admission()) {
    new_addr_type mshr_addr = m_config.mshr_addr(mf->get_addr());
    bool mshr_hit = m_mshrs.probe(mshr_addr);
    bool mshr_avail = !m_mshrs.full(mshr_addr);
    if (!mshr_avail) {
      m_stats.inc_fail_stats(
          mf->get_access_type(),
          mshr_hit ? MSHR_MERGE_ENRTY_FAIL : MSHR_ENRTY_FAIL,
          mf->get_streamID());
      return RESERVATION_FAIL;
    }
    bool needs_lower_read =
        m_mshrs.needs_lower_read(mshr_addr, mf->get_access_sector_mask());
    unsigned n_new_entries = needs_lower_read ? 1 : 0;
    if (!mshr_hit && status == MISS &&
        m_tag_array->block_is_modified(cache_index)) {
      // The allocation will emit demand-read plus victim writeback.
      n_new_entries++;
    }
    if (!miss_queue_has_slots(n_new_entries)) {
      m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                             mf->get_streamID());
      return RESERVATION_FAIL;
    }
  } else if (miss_queue_full(1)) {
    // Historical shared-cache behavior for non-characterization policies.
    m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                           mf->get_streamID());
    return RESERVATION_FAIL;
  }

  new_addr_type block_addr = m_config.block_addr(addr);
  bool do_miss = false;
  bool wb = false;
  evicted_block_info evicted;
  send_read_request(addr, block_addr, cache_index, mf, time, do_miss, wb,
                    evicted, events, false, false);

  if (do_miss) {
    // If evicted block is modified and not a write-through
    // (already modified lower level)
    if (wb && (m_config.m_write_policy != WRITE_THROUGH)) {
      mem_fetch *wb = m_memfetch_creator->alloc(
          evicted.m_block_addr, m_wrbk_type, mf->get_access_warp_mask(),
          evicted.m_byte_mask, evicted.m_sector_mask, evicted.m_modified_size,
          true, m_gpu->gpu_tot_sim_cycle + m_gpu->gpu_sim_cycle, -1, -1, -1,
          NULL, mf->get_streamID());
      // the evicted block may have wrong chip id when advanced L2 hashing  is
      // used, so set the right chip address from the original mf
      wb->set_chip(mf->get_tlx_addr().chip);
      wb->set_partition(mf->get_tlx_addr().sub_partition);
      send_write_request(wb, WRITE_BACK_REQUEST_SENT, time, events);
    }
    return MISS;
  }
  return RESERVATION_FAIL;
}

/// Access cache for read_only_cache: returns RESERVATION_FAIL if
// request could not be accepted (for any reason)
enum cache_request_status read_only_cache::access(
    new_addr_type addr, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events) {
  assert(mf->get_data_size() <= m_config.get_atom_sz());
  assert(m_config.m_write_policy == READ_ONLY);
  assert(!mf->get_is_write());
  new_addr_type block_addr = m_config.block_addr(addr);
  unsigned cache_index = (unsigned)-1;
  enum cache_request_status status =
      m_tag_array->probe(block_addr, cache_index, mf, mf->is_write());
  enum cache_request_status cache_status = RESERVATION_FAIL;

  if (status == HIT) {
    cache_status = m_tag_array->access(block_addr, time, cache_index,
                                       mf);  // update LRU state
  } else if (status != RESERVATION_FAIL) {
    if (!miss_queue_full(0)) {
      bool do_miss = false;
      send_read_request(addr, block_addr, cache_index, mf, time, do_miss,
                        events, true, false);
      if (do_miss)
        cache_status = MISS;
      else
        cache_status = RESERVATION_FAIL;
    } else {
      cache_status = RESERVATION_FAIL;
      m_stats.inc_fail_stats(mf->get_access_type(), MISS_QUEUE_FULL,
                             mf->get_streamID());
    }
  } else {
    m_stats.inc_fail_stats(mf->get_access_type(), LINE_ALLOC_FAIL,
                           mf->get_streamID());
  }

  m_stats.inc_stats(mf->get_access_type(),
                    m_stats.select_stats_status(status, cache_status),
                    mf->get_streamID());
  m_stats.inc_stats_pw(mf->get_access_type(),
                       m_stats.select_stats_status(status, cache_status),
                       mf->get_streamID());
  return cache_status;
}

//! A general function that takes the result of a tag_array probe
//  and performs the correspding functions based on the cache configuration
//  The access fucntion calls this function
enum cache_request_status data_cache::process_tag_probe(
    bool wr, enum cache_request_status probe_status, new_addr_type addr,
    unsigned cache_index, mem_fetch *mf, unsigned time,
    std::list<cache_event> &events) {
  // Each function pointer ( m_[rd/wr]_[hit/miss] ) is set in the
  // data_cache constructor to reflect the corresponding cache configuration
  // options. Function pointers were used to avoid many long conditional
  // branches resulting from many cache configuration options.
  cache_request_status access_status = probe_status;
  if (wr) {  // Write
    if (probe_status == HIT) {
      access_status =
          (this->*m_wr_hit)(addr, cache_index, mf, time, events, probe_status);
    } else if ((probe_status != RESERVATION_FAIL) ||
               (probe_status == RESERVATION_FAIL &&
                m_config.m_write_alloc_policy == NO_WRITE_ALLOCATE)) {
      access_status =
          (this->*m_wr_miss)(addr, cache_index, mf, time, events, probe_status);
      // NVIDIA-comparable accounting: a write miss that ALLOCATES a line (any
      // write-allocate policy) is absorbed into this cache, which NVIDIA's L2
      // reports as a write "hit". Record it in a separate WRITE_ALLOCATE
      // bucket. This does NOT change HIT/MISS/TOTAL_ACCESS; comparable write
      // hits are computed as HIT + WRITE_ALLOCATE in correl_mappings.py.
      if (m_config.m_write_alloc_policy != NO_WRITE_ALLOCATE &&
          access_status == MISS) {
        m_stats.inc_stats(mf->get_access_type(), WRITE_ALLOCATED,
                          mf->get_streamID());
        m_stats.inc_stats_pw(mf->get_access_type(), WRITE_ALLOCATED,
                             mf->get_streamID());
      }
    } else {
      // the only reason for reservation fail here is LINE_ALLOC_FAIL (i.e all
      // lines are reserved)
      m_stats.inc_fail_stats(mf->get_access_type(), LINE_ALLOC_FAIL,
                             mf->get_streamID());
    }
  } else {  // Read
    if (probe_status == HIT) {
      access_status =
          (this->*m_rd_hit)(addr, cache_index, mf, time, events, probe_status);
    } else if (probe_status != RESERVATION_FAIL) {
      access_status =
          (this->*m_rd_miss)(addr, cache_index, mf, time, events, probe_status);
    } else {
      // the only reason for reservation fail here is LINE_ALLOC_FAIL (i.e all
      // lines are reserved)
      m_stats.inc_fail_stats(mf->get_access_type(), LINE_ALLOC_FAIL,
                             mf->get_streamID());
    }
  }

  // In EP-L2 payload modes the target payload RAM, rather than the historical
  // DataPort, is the authoritative data-operation timing resource.
  if (!m_config.m_ep_l2_payload_mode)
    m_bandwidth_management.use_data_port(mf, access_status, events);
  return access_status;
}

// Both the L1 and L2 currently use the same access function.
// Differentiation between the two caches is done through configuration
// of caching policies.
// Both the L1 and L2 override this function to provide a means of
// performing actions specific to each cache when such actions are implemnted.
enum cache_request_status data_cache::access(new_addr_type addr, mem_fetch *mf,
                                             unsigned time,
                                             std::list<cache_event> &events) {
  assert(mf->get_data_size() <= m_config.get_atom_sz());
  bool wr = mf->get_is_write();
  new_addr_type block_addr = m_config.block_addr(addr);
  unsigned cache_index = (unsigned)-1;
  enum cache_request_status probe_status =
      m_tag_array->probe(block_addr, cache_index, mf, mf->is_write(), true);
  enum cache_request_status access_status =
      process_tag_probe(wr, probe_status, addr, cache_index, mf, time, events);
  m_stats.inc_stats(mf->get_access_type(),
                    m_stats.select_stats_status(probe_status, access_status),
                    mf->get_streamID());
  m_stats.inc_stats_pw(mf->get_access_type(),
                       m_stats.select_stats_status(probe_status, access_status),
                       mf->get_streamID());
  return access_status;
}

/// This is meant to model the first level data cache in Fermi.
/// It is write-evict (global) or write-back (local) at the
/// granularity of individual blocks (Set by GPGPU-Sim configuration file)
/// (the policy used in fermi according to the CUDA manual)
enum cache_request_status l1_cache::access(new_addr_type addr, mem_fetch *mf,
                                           unsigned time,
                                           std::list<cache_event> &events) {
  return data_cache::access(addr, mf, time, events);
}

// The l2 cache access function calls the base data_cache access
// implementation.  When the L2 needs to diverge from L1, L2 specific
// changes should be made here.
enum cache_request_status l2_cache::access(new_addr_type addr, mem_fetch *mf,
                                           unsigned time,
                                           std::list<cache_event> &events) {
  m_ep_l2_last_payload_request_result = ep_l2_payload_store::GRANTED;
  const new_addr_type m0b_mshr_addr = m_config.mshr_addr(mf->get_addr());
  const bool m0b_mshr_present_before = m_mshrs.probe(m0b_mshr_addr);
  unsigned payload_index = (unsigned)-1;
  bool payload_reserved = false;
  ep_l2_payload_store::slot payload_saved;
  ep_l2_payload_store::payload_handle sidecar_saved;
  const new_addr_type payload_owner = m_config.block_addr(addr);
  if (m_ep_l2_payload.enabled()) {
    const enum cache_request_status payload_probe = m_tag_array->probe(
        payload_owner, payload_index, mf, mf->is_write(), true);
    // Hits consume a target-RAM operation. A miss only reserves its static
    // landing; the physical 128B write occurs when its lower response lands.
    if (payload_probe == HIT) {
      assert(payload_index < ep_l2_payload_store::k_resident_slots);
      const ep_l2_payload_store::payload_handle handle =
          tag_payload_handle(payload_index);
      assert(handle.valid() && handle.payload_id == payload_index &&
             m_ep_l2_payload.handle_owner_matches(handle, payload_owner));
      m_ep_l2_last_payload_request_result = m_ep_l2_payload.request(
          handle,
          mf->get_is_write(), time, (unsigned long long)-1,
          mf->get_is_write() ? ep_l2_payload_store::RESIDENT_WRITE
                             : ep_l2_payload_store::RESIDENT_HIT_READ);
      if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED)
        return RESERVATION_FAIL;
    } else if (payload_probe == MISS) {
      assert(payload_index < ep_l2_payload_store::k_resident_slots);
      payload_saved = m_ep_l2_payload.resident(payload_index);
      sidecar_saved = tag_payload_handle(payload_index);
      // Identity exists before data_cache::access can enqueue the lower read.
      m_ep_l2_payload.reserve_resident(payload_index, payload_owner, mf);
      const ep_l2_payload_store::payload_handle handle =
          m_ep_l2_payload.resident_handle(payload_index);
      assert(handle.payload_id == payload_index);
      set_tag_payload_handle(payload_index, handle);
      reconcile_tag_payload_handles();
      payload_reserved = true;
    } else if (payload_probe != RESERVATION_FAIL) {
      // Sector/MSHR requests to a reserved line share the original static
      // landing. The status is HIT_RESERVED in some cache organizations and
      // SECTOR_MISS in others, so state—not the status spelling—defines this.
      assert(payload_index < ep_l2_payload_store::k_resident_slots);
      // A sector miss may target a payload that is already valid/dirty for
      // another sector.  Identity is line-scoped and must be carried in that
      // case too; only the pending-sector mask is fill-scoped.
      const ep_l2_payload_store::payload_handle handle =
          tag_payload_handle(payload_index);
      assert(handle.valid() && handle.payload_id == payload_index &&
             m_ep_l2_payload.handle_owner_matches(handle, payload_owner));
      m_ep_l2_payload.attach_resident_identity(payload_index, mf);
    }
  }
  bool wad_reserved = false;
  new_addr_type wad_addr = 0;
  if (ep_l2_wad_enabled()) {
    const new_addr_type requested_block = m_config.block_addr(addr);
    // Reads of an address with an outstanding dirty writeback are ordered
    // behind that writeback.  The tag may already be reused, so this must be
    // checked independently of the normal tag probe.
    if (!mf->get_is_write() && ep_l2_wad_contains(requested_block)) {
      ++m_ep_l2_wad_same_address_wait_count;
      if (payload_reserved) {
        m_ep_l2_payload.restore_resident(payload_index, payload_saved);
        set_tag_payload_handle(payload_index, sidecar_saved);
      }
      return RESERVATION_FAIL;
    }

    unsigned victim_index = (unsigned)-1;
    const enum cache_request_status victim_status = m_tag_array->probe(
        requested_block, victim_index, mf, mf->is_write(), true);
    if (victim_status == MISS &&
        m_tag_array->block_is_modified(victim_index)) {
      wad_addr = m_tag_array->get_block(victim_index)->m_block_addr;
      if (m_ep_l2_wad_entries_live.size() >= m_config.m_ep_l2_wad_entries) {
        ++m_ep_l2_wad_full_block_count;
        if (payload_reserved) {
          m_ep_l2_payload.restore_resident(payload_index, payload_saved);
          set_tag_payload_handle(payload_index, sidecar_saved);
        }
        return RESERVATION_FAIL;
      }
      const bool inserted = m_ep_l2_wad_entries_live.insert(wad_addr).second;
      // A dirty victim must have no already-pending WAD.  A duplicate would
      // mean an earlier destructive eviction was not serialized correctly.
      assert(inserted);
      const bool timestamp_inserted =
          m_ep_l2_wad_birth_cycle.insert(std::make_pair(wad_addr, time)).second;
      assert(timestamp_inserted);
      wad_reserved = true;
    }
  }

  const enum cache_request_status result =
      data_cache::access(addr, mf, time, events);
  if (wad_reserved) {
    if (result == RESERVATION_FAIL) {
      m_ep_l2_wad_entries_live.erase(wad_addr);
      const size_t timestamp_erased = m_ep_l2_wad_birth_cycle.erase(wad_addr);
      assert(timestamp_erased == 1);
    } else {
      cache_event writeback(WRITE_BACK_REQUEST_SENT);
      // Allocation precedes tag mutation, and every reserved WAD must bind to
      // the real writeback that data_cache just created.
      assert(was_writeback_sent(events, writeback));
      if (m_config.ep_l2_m0b_stats_enabled())
        ep_l2_m0b_record_wad_payload(sidecar_saved);
    }
  }
  const bool lower_read = was_read_sent(events);
  if (m_config.ep_l2_m0b_stats_enabled() && !m0b_mshr_present_before &&
      result != RESERVATION_FAIL && lower_read)
    ep_l2_m0b_record_allocation(m0b_mshr_addr, mf, time);
  if (m_config.ep_l2_m0b_stats_enabled() && payload_reserved &&
      result != RESERVATION_FAIL)
    ++m_ep_l2_m0b.resident_allocations;
  if (payload_reserved && result == RESERVATION_FAIL) {
    m_ep_l2_payload.restore_resident(payload_index, payload_saved);
    set_tag_payload_handle(payload_index, sidecar_saved);
  } else if (m_ep_l2_payload.enabled() && result == HIT &&
             mf->get_is_write() && payload_index != (unsigned)-1) {
    m_ep_l2_payload.mark_resident_dirty(payload_index);
  }
  // Lower-read generation, not the cache API's HIT/MISS spelling, determines
  // whether the static landing remains pending.  This also covers a sector
  // miss on an otherwise valid/dirty 128B line.
  if (m_ep_l2_payload.enabled() && payload_index != (unsigned)-1 && lower_read)
    m_ep_l2_payload.note_resident_lower_read(
        payload_index, payload_owner, mf->get_ep_l2_payload_generation(),
        mf->get_access_sector_mask());
  else if (payload_reserved && result != RESERVATION_FAIL) {
    // A newly allocated, locally absorbed lazy-fetch write has no lower fill.
    m_ep_l2_payload.complete_resident_no_fill(
        payload_index, payload_owner, mf->get_ep_l2_payload_generation(),
        mf->get_is_write());
  }
  // data_cache policies may modify the selected block after tag_array::access
  // (for example a write hit).  Refresh only the selected L2 line.
  unsigned cache_index = (unsigned)-1;
  const new_addr_type block_addr = m_config.block_addr(addr);
  const enum cache_request_status status =
      m_tag_array->probe(block_addr, cache_index, mf, mf->is_write(), true);
  if (status != RESERVATION_FAIL)
    m_tag_array->l2_char_tracking_refresh_line(cache_index);
  return result;
}

void l2_cache::ep_l2_wad_complete(new_addr_type block_addr,
                                   unsigned long long cycle) {
  if (!ep_l2_wad_enabled()) return;
  const new_addr_type normalized = m_config.block_addr(block_addr);
  std::map<new_addr_type, unsigned long long>::iterator birth =
      m_ep_l2_wad_birth_cycle.find(normalized);
  assert(birth != m_ep_l2_wad_birth_cycle.end());
  // CUDA memcpy traffic can carry a local dispatch offset whereas set_done()
  // uses the global cycle.  Saturate that instrumentation-only corner rather
  // than allowing an unsigned underflow to corrupt the lifetime histogram.
  const unsigned long long lifetime =
      cycle >= birth->second ? cycle - birth->second : 0;
  m_ep_l2_wad_lifetime_sum += lifetime;
  ++m_ep_l2_wad_lifetime_count;
  m_ep_l2_wad_lifetime_max = std::max(m_ep_l2_wad_lifetime_max, lifetime);
  ++m_ep_l2_wad_lifetime_hist[lifetime];
  m_ep_l2_wad_birth_cycle.erase(birth);
  const size_t erased = m_ep_l2_wad_entries_live.erase(normalized);
  assert(erased == 1);
}

void l2_cache::fill(mem_fetch *mf, unsigned time) {
  new_addr_type m0b_mshr_addr = 0;
  bool m0b_fill_tracked = false;
  if (m_config.ep_l2_m0b_stats_enabled()) {
    extra_mf_fields_lookup::iterator m0b_extra = m_extra_mf_fields.find(mf);
    if (m0b_extra != m_extra_mf_fields.end()) {
      m0b_mshr_addr = m0b_extra->second.m_block_addr;
      m0b_fill_tracked = true;
    }
  }
  if (m_ep_l2_payload.enabled()) {
    assert(mf->has_ep_l2_payload_identity());
    const unsigned id = mf->get_ep_l2_payload_id();
    assert(id < ep_l2_payload_store::k_resident_slots);
    extra_mf_fields_lookup::iterator e = m_extra_mf_fields.find(mf);
    assert(e != m_extra_mf_fields.end());
    const ep_l2_payload_store::payload_handle handle(
        id, mf->get_ep_l2_payload_generation());
    assert(tag_payload_handle(e->second.m_cache_index).payload_id == id &&
           tag_payload_handle(e->second.m_cache_index).generation ==
               handle.generation &&
           m_ep_l2_payload.handle_owner_matches(handle, e->second.m_block_addr));
    // A stale response must never land into a reused static payload slot.
    m_ep_l2_payload.complete_resident_fill(
        id, e->second.m_block_addr, mf->get_ep_l2_payload_generation(),
        mf->get_access_sector_mask(), mf->is_write());
  }
  baseline_cache::fill(mf, time);
  if (m0b_fill_tracked) ep_l2_m0b_record_fill(m0b_mshr_addr, time);
}

void l2_cache::ep_l2_m0b_record_allocation(new_addr_type mshr_addr,
                                            mem_fetch *mf,
                                            unsigned long long cycle) {
  ep_l2_m0b_mshr_instance instance;
  instance.epoch = ++m_ep_l2_m0b.next_epoch;
  instance.allocation_cycle = cycle;
  // No source predicate proves a request is safe for RO pending state.
  if (mf->get_is_write())
    ++m_ep_l2_m0b.excluded_write;
  else if (mf->isatomic())
    ++m_ep_l2_m0b.excluded_atomic;
  else if (mf->get_access_type() == L1_WRBK_ACC ||
           mf->get_access_type() == L2_WRBK_ACC)
    ++m_ep_l2_m0b.excluded_writeback;
  else {
    instance.candidate_uncertified = true;
    ++m_ep_l2_m0b.candidate_uncertified;
  }
  ++m_ep_l2_m0b.mshr_allocations;
  // Replacing this map entry is the address-reuse boundary; every live
  // incarnation has a distinct monotonic epoch.
  m_ep_l2_m0b.instances[mshr_addr] = instance;
}

void l2_cache::ep_l2_m0b_record_lower_issue(mem_fetch *mf,
                                             unsigned long long cycle) {
  if (!m_config.ep_l2_m0b_stats_enabled() || mf->get_is_write()) return;
  const new_addr_type mshr_addr = m_config.mshr_addr(mf->get_addr());
  std::map<new_addr_type, ep_l2_m0b_mshr_instance>::iterator instance =
      m_ep_l2_m0b.instances.find(mshr_addr);
  if (instance == m_ep_l2_m0b.instances.end() ||
      !instance->second.candidate_uncertified)
    return;
  if (!instance->second.first_lower_issue_seen) {
    instance->second.first_lower_issue_seen = true;
    instance->second.first_lower_issue_cycle = cycle;
    ++m_ep_l2_m0b.first_lower_issue_count;
    m_ep_l2_m0b.allocation_to_first_lower_issue_sum +=
        cycle >= instance->second.allocation_cycle
            ? cycle - instance->second.allocation_cycle
            : 0;
  }
  instance->second.last_lower_issue_cycle = cycle;
}

void l2_cache::ep_l2_m0b_record_fill(new_addr_type mshr_addr,
                                      unsigned long long cycle) {
  std::map<new_addr_type, ep_l2_m0b_mshr_instance>::iterator instance =
      m_ep_l2_m0b.instances.find(mshr_addr);
  if (instance == m_ep_l2_m0b.instances.end() ||
      !instance->second.candidate_uncertified)
    return;
  if (!instance->second.first_fill_seen) {
    instance->second.first_fill_seen = true;
    ++m_ep_l2_m0b.first_fill_count;
    m_ep_l2_m0b.allocation_to_first_fill_sum +=
        cycle >= instance->second.allocation_cycle
            ? cycle - instance->second.allocation_cycle
            : 0;
  }
  mem_access_sector_mask_t pending, issued, ready;
  m_mshrs.sector_masks(mshr_addr, pending, issued, ready);
  if (!instance->second.all_ready_seen && pending.none()) {
    instance->second.all_ready_seen = true;
    ++m_ep_l2_m0b.all_ready_count;
    m_ep_l2_m0b.allocation_to_all_ready_sum +=
        cycle >= instance->second.allocation_cycle
            ? cycle - instance->second.allocation_cycle
            : 0;
  }
}

void l2_cache::ep_l2_m0b_record_wad_payload(
    const ep_l2_payload_store::payload_handle &old_handle) {
  ++m_ep_l2_m0b.wad_dirty_victim_events;
  if (!old_handle.valid()) return;
  ++m_ep_l2_m0b.wad_old_handle_valid;
  if (m_ep_l2_payload.handle_live(old_handle))
    ++m_ep_l2_m0b.wad_old_handle_live_after_reassign;
  else
    ++m_ep_l2_m0b.wad_old_handle_not_live_after_reassign;
}

void l2_cache::ep_l2_m0b_print(FILE *fp, unsigned slice,
                                unsigned long long completion_cycle) const {
  if (!m_config.ep_l2_m0b_stats_enabled()) return;
  const ep_l2_m0b_observation &s = m_ep_l2_m0b;
  fprintf(fp,
          "EPL2M0BV1|scope=application_cumulative|slice=%u|completion_cycle=%llu|"
          "mshr_instance_epoch=MONOTONIC_ADDRESS_REUSE_SAFE|mshr_allocations=%llu|"
          "ro_candidate_uncertified=%llu|ro_excluded_write=%llu|ro_excluded_atomic=%llu|ro_excluded_writeback=%llu|"
          "allocation_to_first_lower_issue_count=%llu|allocation_to_first_lower_issue_sum=%llu|"
          "allocation_to_last_lower_issue=NOT_EMITTED|allocation_to_first_fill_count=%llu|allocation_to_first_fill_sum=%llu|"
          "allocation_to_all_required_sectors_ready_count=%llu|allocation_to_all_required_sectors_ready_sum=%llu|"
          "allocation_to_final_retirement=NOT_EMITTED|last_lower_issue_to_final_retirement=NOT_EMITTED|all_ready_to_final_retirement=NOT_EMITTED|"
          "wad_dirty_victim_events=%llu|wad_old_handle_valid=%llu|wad_old_handle_live_after_reassign=%llu|wad_old_handle_not_live_after_reassign=%llu|"
          "resident_payload_allocations=%llu|nonresident_payload_allocations=%llu|shared_payload_opportunity=NO_REAL_CONSUMER_YET\n",
          slice, completion_cycle, s.mshr_allocations, s.candidate_uncertified,
          s.excluded_write, s.excluded_atomic, s.excluded_writeback,
          s.first_lower_issue_count, s.allocation_to_first_lower_issue_sum,
          s.first_fill_count, s.allocation_to_first_fill_sum,
          s.all_ready_count, s.allocation_to_all_ready_sum,
          s.wad_dirty_victim_events, s.wad_old_handle_valid,
          s.wad_old_handle_live_after_reassign,
          s.wad_old_handle_not_live_after_reassign,
          s.resident_allocations, s.bypass_allocations);
}

void l2_cache::preview_access(new_addr_type addr, mem_fetch *mf,
                              l2_access_plan &plan) const {
  plan = l2_access_plan();
  if (!exact_l2_admission()) return;

  plan.exact = true;
  plan.is_write = mf->get_is_write();
  plan.is_read = !plan.is_write;
  plan.l1_writeback_absorbed = mf->get_access_type() == L1_WRBK_ACC;

  new_addr_type block_addr = m_config.block_addr(addr);
  if (ep_l2_wad_enabled() && plan.is_read && ep_l2_wad_contains(block_addr)) {
    // A WAD owns this old line until real writeback completion.  Treat the
    // frontend request as non-admissible before it can reach tag mutation.
    plan.probe_status = RESERVATION_FAIL;
    plan.ep_l2_wad_same_address_hazard = true;
    return;
  }
  unsigned cache_index = (unsigned)-1;
  plan.probe_status =
      m_tag_array->probe(block_addr, cache_index, mf, mf->is_write(), true);
  plan.cache_index = cache_index;
  if (plan.probe_status == RESERVATION_FAIL) {
    plan.ep_l2_tag_set_all_reserved = true;
    return;
  }

  if (plan.probe_status == MISS) {
    plan.victim_valid = m_tag_array->block_is_valid(cache_index);
    plan.victim_dirty = m_tag_array->block_is_modified(cache_index);
    if (plan.victim_dirty && ep_l2_wad_enabled() &&
        m_ep_l2_wad_entries_live.size() >= m_config.m_ep_l2_wad_entries) {
      // The required WAD must be allocated before data_cache::access()
      // destructively replaces this victim.
      plan.probe_status = RESERVATION_FAIL;
      plan.ep_l2_wad_full = true;
      return;
    }
    if (plan.victim_dirty) {
      plan.victim_modified_bytes =
          m_tag_array->block_modified_size(cache_index);
      plan.will_send_writeback = true;
      plan.new_missq_entries++;
      plan.needs_data_port = !m_ep_l2_payload.enabled();
    }
  }

  if (plan.is_read) {
    if (plan.probe_status == HIT) {
      plan.needs_data_port = !m_ep_l2_payload.enabled();
      plan.needs_immediate_response_slot = !plan.l1_writeback_absorbed;
      return;
    }

    // Only non-hit reads consult or allocate an MSHR.  In particular, a
    // normal hit must not be burdened with a fictitious lower read or MissQ
    // requirement; doing so makes preview disagree with the real commit.
    new_addr_type mshr_addr = m_config.mshr_addr(mf->get_addr());
    plan.mshr_hit = m_mshrs.probe(mshr_addr);
    if (m_config.ep_l2_descriptor_mode())
      plan.ep_l2_mshr_block_reason = m_mshrs.full_reason(mshr_addr);
    plan.mshr_entry_available =
        !plan.mshr_hit && !m_mshrs.full(mshr_addr);
    plan.mshr_merge_available =
        plan.mshr_hit && !m_mshrs.full(mshr_addr);
    plan.needs_new_mshr = !plan.mshr_hit;
    plan.needs_mshr_merge = plan.mshr_hit;
    // Target mode retains one line MSHR but sends one lower request for every
    // previously unissued sector.  This keeps preview faithful to the real
    // send_read_request() path without changing the admission contract.
    plan.ep_l2_needs_lower_read =
        m_config.ep_l2_descriptor_mode() &&
        m_mshrs.needs_lower_read(mshr_addr, mf->get_access_sector_mask());
    if (!plan.mshr_hit || plan.ep_l2_needs_lower_read) {
      plan.will_send_lower_read = true;
      plan.new_missq_entries++;
    }
    return;
  }

  // The QV100 L2 uses write-back plus lazy-fetch-on-read.  A write miss is
  // locally absorbed; it only creates lower traffic when replacing a dirty
  // victim, which was accounted for above.
  plan.needs_immediate_response_slot = !plan.l1_writeback_absorbed;
  if (plan.probe_status == HIT)
    plan.needs_data_port = !m_ep_l2_payload.enabled();
}

/// Access function for tex_cache
/// return values: RESERVATION_FAIL if request could not be accepted
/// otherwise returns HIT_RESERVED or MISS; NOTE: *never* returns HIT
/// since unlike a normal CPU cache, a "HIT" in texture cache does not
/// mean the data is ready (still need to get through fragment fifo)
enum cache_request_status tex_cache::access(new_addr_type addr, mem_fetch *mf,
                                            unsigned time,
                                            std::list<cache_event> &events) {
  if (m_fragment_fifo.full() || m_request_fifo.full() || m_rob.full())
    return RESERVATION_FAIL;

  assert(mf->get_data_size() <= m_config.get_line_sz());

  // at this point, we will accept the request : access tags and immediately
  // allocate line
  new_addr_type block_addr = m_config.block_addr(addr);
  unsigned cache_index = (unsigned)-1;
  enum cache_request_status status =
      m_tags.access(block_addr, time, cache_index, mf);
  enum cache_request_status cache_status = RESERVATION_FAIL;
  assert(status != RESERVATION_FAIL);
  assert(status != HIT_RESERVED);  // as far as tags are concerned: HIT or MISS
  m_fragment_fifo.push(
      fragment_entry(mf, cache_index, status == MISS, mf->get_data_size()));
  if (status == MISS) {
    // we need to send a memory request...
    unsigned rob_index = m_rob.push(rob_entry(cache_index, mf, block_addr));
    m_extra_mf_fields[mf] = extra_mf_fields(rob_index, m_config);
    mf->set_data_size(m_config.get_line_sz());
    m_tags.fill(cache_index, time, mf);  // mark block as valid
    m_request_fifo.push(mf);
    mf->set_status(m_request_queue_status, time);
    events.push_back(cache_event(READ_REQUEST_SENT));
    cache_status = MISS;
  } else {
    // the value *will* *be* in the cache already
    cache_status = HIT_RESERVED;
  }
  m_stats.inc_stats(mf->get_access_type(),
                    m_stats.select_stats_status(status, cache_status),
                    mf->get_streamID());
  m_stats.inc_stats_pw(mf->get_access_type(),
                       m_stats.select_stats_status(status, cache_status),
                       mf->get_streamID());
  return cache_status;
}

void tex_cache::cycle() {
  // send next request to lower level of memory
  // TODO: Use different full() for sst_mem_interface?
  if (!m_request_fifo.empty()) {
    mem_fetch *mf = m_request_fifo.peek();
    if (!m_memport->full(mf->get_ctrl_size(), false)) {
      m_request_fifo.pop();
      m_memport->push(mf);
    }
  }
  // read ready lines from cache
  if (!m_fragment_fifo.empty() && !m_result_fifo.full()) {
    const fragment_entry &e = m_fragment_fifo.peek();
    if (e.m_miss) {
      // check head of reorder buffer to see if data is back from memory
      unsigned rob_index = m_rob.next_pop_index();
      const rob_entry &r = m_rob.peek(rob_index);
      assert(r.m_request == e.m_request);
      // assert( r.m_block_addr == m_config.block_addr(e.m_request->get_addr())
      // );
      if (r.m_ready) {
        assert(r.m_index == e.m_cache_index);
        m_cache[r.m_index].m_valid = true;
        m_cache[r.m_index].m_block_addr = r.m_block_addr;
        m_result_fifo.push(e.m_request);
        m_rob.pop();
        m_fragment_fifo.pop();
      }
    } else {
      // hit:
      assert(m_cache[e.m_cache_index].m_valid);
      assert(m_cache[e.m_cache_index].m_block_addr ==
             m_config.block_addr(e.m_request->get_addr()));
      m_result_fifo.push(e.m_request);
      m_fragment_fifo.pop();
    }
  }
}

/// Place returning cache block into reorder buffer
void tex_cache::fill(mem_fetch *mf, unsigned time) {
  if (m_config.m_mshr_type == SECTOR_TEX_FIFO) {
    assert(mf->get_original_mf());
    extra_mf_fields_lookup::iterator e =
        m_extra_mf_fields.find(mf->get_original_mf());
    assert(e != m_extra_mf_fields.end());
    e->second.pending_read--;

    if (e->second.pending_read > 0) {
      // wait for the other requests to come back
      delete mf;
      return;
    } else {
      mem_fetch *temp = mf;
      mf = mf->get_original_mf();
      delete temp;
    }
  }

  extra_mf_fields_lookup::iterator e = m_extra_mf_fields.find(mf);
  assert(e != m_extra_mf_fields.end());
  assert(e->second.m_valid);
  assert(!m_rob.empty());
  mf->set_status(m_rob_status, time);

  unsigned rob_index = e->second.m_rob_index;
  rob_entry &r = m_rob.peek(rob_index);
  assert(!r.m_ready);
  r.m_ready = true;
  r.m_time = time;
  assert(r.m_block_addr == m_config.block_addr(mf->get_addr()));
}

void tex_cache::display_state(FILE *fp) const {
  fprintf(fp, "%s (texture cache) state:\n", m_name.c_str());
  fprintf(fp, "fragment fifo entries  = %u / %u\n", m_fragment_fifo.size(),
          m_fragment_fifo.capacity());
  fprintf(fp, "reorder buffer entries = %u / %u\n", m_rob.size(),
          m_rob.capacity());
  fprintf(fp, "request fifo entries   = %u / %u\n", m_request_fifo.size(),
          m_request_fifo.capacity());
  if (!m_rob.empty()) fprintf(fp, "reorder buffer contents:\n");
  for (int n = m_rob.size() - 1; n >= 0; n--) {
    unsigned index = (m_rob.next_pop_index() + n) % m_rob.capacity();
    const rob_entry &r = m_rob.peek(index);
    fprintf(fp, "tex rob[%3d] : %s ", index,
            (r.m_ready ? "ready  " : "pending"));
    if (r.m_ready)
      fprintf(fp, "@%6u", r.m_time);
    else
      fprintf(fp, "       ");
    fprintf(fp, "[idx=%4u]", r.m_index);
    r.m_request->print(fp, false);
  }
  if (!m_fragment_fifo.empty()) {
    fprintf(fp, "fragment fifo (oldest) :");
    fragment_entry &f = m_fragment_fifo.peek();
    fprintf(fp, "%s:          ", f.m_miss ? "miss" : "hit ");
    f.m_request->print(fp, false);
  }
}
/******************************************************************************************************************************************/
