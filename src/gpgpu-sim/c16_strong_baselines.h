#ifndef C16_STRONG_BASELINES_H
#define C16_STRONG_BASELINES_H

#include <stdint.h>

#include <vector>

namespace c16_strong_baselines {

enum policy {
  POLICY_NONE = 0,
  POLICY_PRIORITY_ALL = 1,
  POLICY_PRIORITY_STABLE = 2,
  POLICY_DRRIP = 3,
  POLICY_SHIP_SW = 4
};

enum leader_role { FOLLOWER_SET = 0, SRRIP_LEADER_SET, BRRIP_LEADER_SET };
enum victim_kind { VICTIM_NONE = 0, VICTIM_INVALID, VICTIM_ORDINARY,
                   VICTIM_PRIORITY, VICTIM_RRPV3 };
enum gpu_event {
  EVENT_TRUE_VALID_SECTOR_HIT = 0,
  EVENT_SAME_TAG_SECTOR_MISS,
  EVENT_NEW_LINE_ALLOCATION,
  EVENT_HIT_RESERVED_MSHR_MERGE,
  EVENT_RESERVATION_FAIL_RETRY,
  EVENT_FILL,
  EVENT_WRITEBACK_HIT
};

const char *policy_name(unsigned value);
bool policy_value_valid(unsigned value);
bool policy_needs_target_map(unsigned value);
bool ship_sw_contract_complete();
const char *ship_sw_version();
uint64_t ship_sw_page_signature(uint64_t address);
unsigned ship_sw_shct_index(uint64_t address);
bool ship_sw_predict_resident(unsigned counter);
unsigned ship_sw_train(unsigned counter, bool outcome);

const char *stable_hash_version();
uint64_t stable_hash_seed();
uint64_t stable_hash_threshold();
uint64_t stable_hash(unsigned target_class,
                     uint64_t region_relative_line_index);
bool stable_selected(unsigned target_class,
                     uint64_t region_relative_line_index);

struct candidate {
  unsigned index;
  bool invalid;
  bool priority;
  unsigned rrpv;
  uint64_t last_access;
  uint64_t allocation_time;
};

struct victim_result {
  bool found;
  unsigned index;
  victim_kind kind;
  unsigned aging_rounds;
  victim_result()
      : found(false), index(static_cast<unsigned>(-1)), kind(VICTIM_NONE),
        aging_rounds(0) {}
};

victim_result choose_priority_victim(const std::vector<candidate> &eligible,
                                     bool use_lru);
victim_result choose_drrip_victim(std::vector<candidate> *eligible);

leader_role drrip_leader_role(unsigned set_index, unsigned number_sets);
bool drrip_follower_uses_brrip(unsigned psel);
void drrip_train_psel(leader_role role, unsigned *psel);
unsigned drrip_insertion_rrpv(leader_role role, unsigned psel,
                              uint64_t *brrip_miss_counter,
                              bool *used_brrip);

bool event_promotes(gpu_event event);
bool event_inserts(gpu_event event);

struct diagnostics {
  uint64_t accesses;
  uint64_t true_hit_promotions;
  uint64_t sector_miss_no_promotion;
  uint64_t mshr_merge_no_promotion;
  uint64_t accepted_new_line_allocations;
  uint64_t priority_target_accesses;
  uint64_t priority_selected_accesses;
  uint64_t priority_allocations;
  uint64_t priority_hits;
  uint64_t ordinary_victims;
  uint64_t priority_victims;
  uint64_t invalid_victims;
  uint64_t drrip_srrip_insertions;
  uint64_t drrip_brrip_long_insertions;
  uint64_t drrip_brrip_short_insertions;
  uint64_t drrip_aging_rounds;
  uint64_t drrip_psel_increments;
  uint64_t drrip_psel_decrements;
  uint64_t writeback_hits_not_promoted;
  uint64_t ship_critical_allocations;
  uint64_t ship_regular_predictions;
  uint64_t ship_resident_predictions;
  uint64_t ship_true_hit_outcomes;
  uint64_t ship_positive_trainings;
  uint64_t ship_negative_trainings;
  uint64_t ship_noncritical_no_train;
  uint64_t priority_resident;
  uint64_t priority_pending;
  uint64_t priority_peak_resident;
  uint64_t priority_peak_resident_plus_pending;
  diagnostics();
};

}  // namespace c16_strong_baselines

#endif
