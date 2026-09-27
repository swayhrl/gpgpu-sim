#ifndef ORACLE_ELASTIC_RESIDENCY_H
#define ORACLE_ELASTIC_RESIDENCY_H

#include <stdint.h>

#include <string>
#include <vector>

namespace oracle_elastic_residency {

enum replacement_policy { POLICY_LRU = 0, POLICY_FIFO = 1 };

struct interval {
  uint64_t begin;
  uint64_t end;
  unsigned target_class;
  std::string allocation;
};

// Frozen before any selection-distribution result was observed.
// The seed is the first SHA-512 IV word; it was chosen as a public,
// non-searched constant rather than tuned for this workload.
const char *m1f_hash_version();
uint64_t m1f_hash_seed();
uint64_t m1f_b16_threshold();
uint64_t m1f_stable_hash(unsigned target_class,
                         uint64_t region_relative_line_index);
uint64_t m1f_integer_threshold(uint64_t numerator, uint64_t denominator);
bool m1f_stable_selected(unsigned target_class,
                         uint64_t region_relative_line_index,
                         uint64_t numerator, uint64_t denominator);

struct candidate {
  unsigned index;
  bool invalid;
  bool protected_line;
  uint64_t last_access;
  uint64_t allocation_time;
};

enum selection_status {
  SELECTION_FOUND = 0,
  SELECTION_NO_ELIGIBLE
};

enum victim_kind {
  VICTIM_INVALID = 0,
  VICTIM_NORMAL,
  VICTIM_PROTECTED,
  VICTIM_NONE
};

enum admission_denial_reason {
  ADMISSION_DENIAL_NONE = 0,
  QUOTA_FULL_BASELINE_INVALID_PRIORITY,
  QUOTA_FULL_NO_LOCAL_PROTECTED_VICTIM
};

struct selection {
  selection_status status;
  unsigned index;
  victim_kind victim;
  bool protect_fill;
  bool quota_full;
  admission_denial_reason denial_reason;
  selection()
      : status(SELECTION_NO_ELIGIBLE),
        index(static_cast<unsigned>(-1)),
        victim(VICTIM_NONE),
        protect_fill(false),
        quota_full(false),
        denial_reason(ADMISSION_DENIAL_NONE) {}
};

struct admission {
  bool protect_fill;
  admission_denial_reason denial_reason;
  admission() : protect_fill(false), denial_reason(ADMISSION_DENIAL_NONE) {}
};

struct diagnostic_counters {
  uint64_t target_accesses;
  uint64_t selected_target_accesses;
  uint64_t selection_filtered_admissions;
  uint64_t target_hits;
  uint64_t target_misses;
  uint64_t protected_fills;
  uint64_t protected_hits;
  uint64_t target_normal_victims;
  uint64_t target_protected_victims;
  uint64_t normal_normal_victims;
  uint64_t normal_fallback_protected_victims;
  uint64_t quota_full_events;
  uint64_t protected_protected_replacements;
  uint64_t ordinary_borrowing_fills;
  uint64_t ordinary_borrowing_current;
  uint64_t ordinary_borrowing_max;
  uint64_t target_protection_admission_denied;
  uint64_t denial_quota_full_invalid_priority;
  uint64_t denial_quota_full_no_local_protected;
  uint64_t occupancy_current;
  uint64_t occupancy_max;
  diagnostic_counters();
};

void account_target_access(diagnostic_counters *counters, bool oracle_target,
                           bool protection_eligible, bool hit, bool miss);
void account_selection_filtered_admission(diagnostic_counters *counters,
                                          bool oracle_target,
                                          bool protection_eligible,
                                          bool admission_attempt);

class oracle_config {
 public:
  oracle_config();

  void configure(bool enabled, uint64_t global_budget_bytes,
                 uint64_t exact_global_line_budget, unsigned line_bytes,
                 unsigned l2_instances, const char *sidecar_path,
                 const char *accepted_sha256, bool diagnostics,
                 bool m1f_stable_admission = false);

  bool enabled() const { return m_enabled; }
  bool diagnostics_enabled() const { return m_diagnostics; }
  bool m1f_stable_admission_enabled() const { return m_m1f_enabled; }
  uint64_t global_line_budget() const { return m_global_lines; }
  unsigned l2_instances() const { return m_instances; }
  uint64_t quota_for_instance(unsigned instance) const;
  bool lookup(uint64_t address, unsigned *target_class = 0) const;
  bool lookup_identity(uint64_t address, unsigned *target_class,
                       uint64_t *region_relative_line_index) const;
  bool protection_eligible(uint64_t address, unsigned *target_class = 0,
                           uint64_t *region_relative_line_index = 0,
                           uint64_t *selection_hash = 0) const;
  uint64_t m1f_threshold() const { return m_m1f_threshold; }
  uint64_t m1f_seed() const { return m_m1f_seed; }
  const std::vector<interval> &intervals() const { return m_intervals; }
  const std::string &sidecar_sha256() const { return m_sidecar_sha256; }

  static std::string sha256_file(const std::string &path);

 private:
  void load_sidecar(const std::string &path, const std::string &accepted_sha);

  bool m_enabled;
  bool m_diagnostics;
  bool m_m1f_enabled;
  unsigned m_line_bytes;
  uint64_t m_global_lines;
  uint64_t m_m1f_threshold;
  uint64_t m_m1f_seed;
  unsigned m_instances;
  std::vector<uint64_t> m_quotas;
  std::vector<interval> m_intervals;
  std::string m_sidecar_sha256;
};

selection choose_victim(const std::vector<candidate> &eligible,
                        bool incoming_target, uint64_t occupancy,
                        uint64_t quota, replacement_policy policy,
                        bool mechanism_enabled);

admission decide_fill_admission(bool incoming_target, uint64_t occupancy,
                                uint64_t quota, bool victim_invalid,
                                bool victim_protected,
                                bool mechanism_enabled);

bool target_request_eligible(unsigned access_type, bool is_write,
                             unsigned global_read_access_type);

// Opt-in address-integration observer. It is disabled unless a nonempty log
// path is configured and never participates in replacement decisions.
void configure_address_observer(const char *path, unsigned target_limit,
                                uint64_t exact_target_address,
                                const std::vector<interval> &intervals);
void observe_address(const char *stage, uint64_t address, int eligibility,
                     int access_type, bool is_write, uint64_t request_uid);

}  // namespace oracle_elastic_residency

#endif
