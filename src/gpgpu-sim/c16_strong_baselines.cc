#include "c16_strong_baselines.h"

#include <algorithm>
#include <stdexcept>

namespace c16_strong_baselines {
namespace {

const uint64_t kSeed = 0x6a09e667f3bcc908ULL;
const uint64_t kThreshold = 0x0484baf3b723b966ULL;

unsigned oldest(const std::vector<candidate> &rows,
                const std::vector<unsigned> &positions, bool use_lru) {
  unsigned selected = positions.front();
  uint64_t timestamp = use_lru ? rows[selected].last_access
                               : rows[selected].allocation_time;
  for (size_t i = 1; i < positions.size(); ++i) {
    const unsigned position = positions[i];
    const uint64_t current = use_lru ? rows[position].last_access
                                     : rows[position].allocation_time;
    if (current < timestamp) {
      timestamp = current;
      selected = position;
    }
  }
  return selected;
}

}  // namespace

const char *policy_name(unsigned value) {
  switch (value) {
    case POLICY_NONE: return "NONE";
    case POLICY_PRIORITY_ALL: return "PRIORITY_ALL";
    case POLICY_PRIORITY_STABLE: return "PRIORITY_STABLE";
    case POLICY_DRRIP: return "DRRIP";
    case POLICY_SHIP_SW: return "SHIP_SW";
    default: return "INVALID";
  }
}

bool policy_value_valid(unsigned value) { return value <= POLICY_SHIP_SW; }

bool policy_needs_target_map(unsigned value) {
  return value == POLICY_PRIORITY_ALL || value == POLICY_PRIORITY_STABLE ||
         value == POLICY_SHIP_SW;
}

bool ship_sw_contract_complete() { return true; }

const char *ship_sw_version() { return "C16_SHIP_SW_STYLE_V1"; }

uint64_t ship_sw_page_signature(uint64_t address) { return address >> 12; }

unsigned ship_sw_shct_index(uint64_t address) {
  return static_cast<unsigned>(ship_sw_page_signature(address) & 0x3fffULL);
}

bool ship_sw_predict_resident(unsigned counter) {
  if (counter > 7) throw std::runtime_error("3-bit SHCT counter overflow");
  return counter > 0;
}

unsigned ship_sw_train(unsigned counter, bool outcome) {
  if (counter > 7) throw std::runtime_error("3-bit SHCT counter overflow");
  if (outcome) return std::min(7U, counter + 1);
  return counter > 0 ? counter - 1 : 0;
}

const char *stable_hash_version() {
  return "C16_M1F_STABLE_ADMISSION_HASH_V1";
}
uint64_t stable_hash_seed() { return kSeed; }
uint64_t stable_hash_threshold() { return kThreshold; }

uint64_t stable_hash(unsigned target_class,
                     uint64_t region_relative_line_index) {
  uint64_t value = kSeed ^ (static_cast<uint64_t>(target_class) << 32) ^
                   region_relative_line_index;
  value += 0x9e3779b97f4a7c15ULL;
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

bool stable_selected(unsigned target_class,
                     uint64_t region_relative_line_index) {
  return stable_hash(target_class, region_relative_line_index) < kThreshold;
}

victim_result choose_priority_victim(const std::vector<candidate> &eligible,
                                     bool use_lru) {
  victim_result result;
  if (eligible.empty()) return result;
  unsigned invalid = static_cast<unsigned>(-1);
  std::vector<unsigned> ordinary;
  std::vector<unsigned> priority;
  for (size_t i = 0; i < eligible.size(); ++i) {
    if (eligible[i].invalid)
      invalid = static_cast<unsigned>(i);  // exact baseline last-invalid rule
    else if (eligible[i].priority)
      priority.push_back(static_cast<unsigned>(i));
    else
      ordinary.push_back(static_cast<unsigned>(i));
  }
  unsigned position = static_cast<unsigned>(-1);
  if (invalid != static_cast<unsigned>(-1)) {
    position = invalid;
    result.kind = VICTIM_INVALID;
  } else if (!ordinary.empty()) {
    position = oldest(eligible, ordinary, use_lru);
    result.kind = VICTIM_ORDINARY;
  } else if (!priority.empty()) {
    position = oldest(eligible, priority, use_lru);
    result.kind = VICTIM_PRIORITY;
  }
  if (position != static_cast<unsigned>(-1)) {
    result.found = true;
    result.index = eligible[position].index;
  }
  return result;
}

victim_result choose_drrip_victim(std::vector<candidate> *eligible) {
  victim_result result;
  if (!eligible || eligible->empty()) return result;
  unsigned invalid = static_cast<unsigned>(-1);
  for (size_t i = 0; i < eligible->size(); ++i)
    if ((*eligible)[i].invalid) invalid = static_cast<unsigned>(i);
  if (invalid != static_cast<unsigned>(-1)) {
    result.found = true;
    result.index = (*eligible)[invalid].index;
    result.kind = VICTIM_INVALID;
    return result;
  }
  for (;;) {
    for (size_t i = 0; i < eligible->size(); ++i) {
      if ((*eligible)[i].rrpv == 3) {
        result.found = true;
        result.index = (*eligible)[i].index;
        result.kind = VICTIM_RRPV3;
        return result;
      }
    }
    for (size_t i = 0; i < eligible->size(); ++i)
      (*eligible)[i].rrpv = std::min(3U, (*eligible)[i].rrpv + 1);
    ++result.aging_rounds;
    if (result.aging_rounds > 3)
      throw std::runtime_error("2-bit DRRIP aging failed to reach RRPV=3");
  }
}

leader_role drrip_leader_role(unsigned set_index, unsigned number_sets) {
  if (number_sets < 64 || set_index >= number_sets)
    throw std::runtime_error("DRRIP requires at least 64 valid sets");
  for (unsigned slot = 0; slot < 64; ++slot) {
    const unsigned anchor = (slot * number_sets) / 64;
    if (set_index == anchor)
      return (slot & 1) ? BRRIP_LEADER_SET : SRRIP_LEADER_SET;
  }
  return FOLLOWER_SET;
}

bool drrip_follower_uses_brrip(unsigned psel) {
  if (psel > 1023) throw std::runtime_error("10-bit PSEL overflow");
  return psel >= 512;
}

void drrip_train_psel(leader_role role, unsigned *psel) {
  if (!psel || *psel > 1023) throw std::runtime_error("invalid PSEL");
  if (role == SRRIP_LEADER_SET) {
    if (*psel < 1023) ++*psel;
  } else if (role == BRRIP_LEADER_SET) {
    if (*psel > 0) --*psel;
  }
}

unsigned drrip_insertion_rrpv(leader_role role, unsigned psel,
                              uint64_t *brrip_miss_counter,
                              bool *used_brrip) {
  if (!brrip_miss_counter || !used_brrip)
    throw std::runtime_error("null DRRIP insertion state");
  const bool brrip = role == BRRIP_LEADER_SET ||
                     (role == FOLLOWER_SET &&
                      drrip_follower_uses_brrip(psel));
  *used_brrip = brrip;
  if (!brrip) return 2;
  const bool short_insertion = ((*brrip_miss_counter & 31ULL) == 0);
  ++*brrip_miss_counter;
  return short_insertion ? 2 : 3;
}

bool event_promotes(gpu_event event) {
  return event == EVENT_TRUE_VALID_SECTOR_HIT;
}

bool event_inserts(gpu_event event) {
  return event == EVENT_NEW_LINE_ALLOCATION;
}

diagnostics::diagnostics()
    : accesses(0), true_hit_promotions(0), sector_miss_no_promotion(0),
      mshr_merge_no_promotion(0), accepted_new_line_allocations(0),
      priority_target_accesses(0), priority_selected_accesses(0),
      priority_allocations(0), priority_hits(0), ordinary_victims(0),
      priority_victims(0), invalid_victims(0), drrip_srrip_insertions(0),
      drrip_brrip_long_insertions(0), drrip_brrip_short_insertions(0),
      drrip_aging_rounds(0), drrip_psel_increments(0),
      drrip_psel_decrements(0), writeback_hits_not_promoted(0),
      ship_critical_allocations(0), ship_regular_predictions(0),
      ship_resident_predictions(0), ship_true_hit_outcomes(0),
      ship_positive_trainings(0), ship_negative_trainings(0),
      ship_noncritical_no_train(0),
      priority_resident(0), priority_pending(0),
      priority_peak_resident(0), priority_peak_resident_plus_pending(0) {}

}  // namespace c16_strong_baselines
