// Corrected conventional L2 admission rules.
//
// This header is deliberately state-free so the same resource predicate is
// used by the memory-subpartition controller and the deterministic regression
// suite.  It does not allocate, schedule, or mutate cache state.

#ifndef L2_ADMISSION_RULES_H
#define L2_ADMISSION_RULES_H

#include <stdint.h>

enum l2_block_reason {
  L2_BLOCK_LINE_ALLOC = 0,
  L2_BLOCK_MSHR_NEW,
  L2_BLOCK_MSHR_MERGE,
  L2_BLOCK_MISSQ,
  L2_BLOCK_DATA_PORT,
  L2_BLOCK_RESPQ,
  L2_BLOCK_OTHER,
  NUM_L2_BLOCK_REASONS
};

struct l2_admission_inputs {
  l2_admission_inputs()
      : line_available(false), needs_new_mshr(false), new_mshr_available(false),
        needs_mshr_merge(false), mshr_merge_available(false),
        missq_available(false), needs_data_port(false), data_port_available(false),
        needs_response_slot(false), response_slot_available(false) {}

  bool line_available;
  bool needs_new_mshr;
  bool new_mshr_available;
  bool needs_mshr_merge;
  bool mshr_merge_available;
  bool missq_available;
  bool needs_data_port;
  bool data_port_available;
  bool needs_response_slot;
  bool response_slot_available;
};

inline unsigned long long l2_admission_blockers(
    const l2_admission_inputs &in) {
  unsigned long long blockers = 0;
  if (!in.line_available) blockers |= 1ULL << L2_BLOCK_LINE_ALLOC;
  if (in.needs_new_mshr && !in.new_mshr_available)
    blockers |= 1ULL << L2_BLOCK_MSHR_NEW;
  if (in.needs_mshr_merge && !in.mshr_merge_available)
    blockers |= 1ULL << L2_BLOCK_MSHR_MERGE;
  if (!in.missq_available) blockers |= 1ULL << L2_BLOCK_MISSQ;
  if (in.needs_data_port && !in.data_port_available)
    blockers |= 1ULL << L2_BLOCK_DATA_PORT;
  if (in.needs_response_slot && !in.response_slot_available)
    blockers |= 1ULL << L2_BLOCK_RESPQ;
  return blockers;
}

inline bool l2_admission_allowed(const l2_admission_inputs &in) {
  return l2_admission_blockers(in) == 0;
}

// This is the production preview/commit contract used by the L2 controller
// immediately after cache::access().  Keep it here, rather than reproducing
// the comparison in a test, so regression tests exercise the same predicate
// as the simulator's release build.
inline bool l2_preview_commit_matches(bool expected_lower_read,
                                      bool expected_lower_write,
                                      bool expected_writeback,
                                      unsigned expected_missq_entries,
                                      bool reservation_fail,
                                      bool lower_read_sent,
                                      bool lower_write_sent,
                                      bool writeback_sent,
                                      unsigned actual_missq_entries) {
  return !reservation_fail &&
         expected_lower_read == lower_read_sent &&
         expected_lower_write == lower_write_sent &&
         expected_writeback == writeback_sent &&
         expected_missq_entries == actual_missq_entries;
}

inline bool dram_issue_allowed(bool needs_return, bool return_path_full,
                               bool general_credit_available,
                               bool wb_progress_credit_available) {
  if (needs_return && return_path_full) return false;
  return general_credit_available ||
         (!needs_return && wb_progress_credit_available);
}

inline float l2_windowed_miss_rate(unsigned total_access,
                                   unsigned previous_access,
                                   unsigned total_miss,
                                   unsigned total_sector_miss,
                                   unsigned previous_miss) {
  unsigned n_access = total_access - previous_access;
  unsigned n_miss = total_miss + total_sector_miss - previous_miss;
  return n_access ? (float)n_miss / n_access : 0.0f;
}

#endif
