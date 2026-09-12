// Functional VM translation structures shared by the M2/M3 pipeline.
#ifndef GPGPU_SIM_VM_TRANSLATION_H
#define GPGPU_SIM_VM_TRANSLATION_H

#include <stdint.h>
#include <stdio.h>

#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "vm_core.h"

namespace vm_translation {

struct translation_key {
  unsigned asid;
  uint64_t vpn;
  uint64_t page_size;
  translation_key(unsigned a = 0, uint64_t v = 0, uint64_t p = 0)
      : asid(a), vpn(v), page_size(p) {}
  bool operator==(const translation_key &other) const {
    return asid == other.asid && vpn == other.vpn &&
           page_size == other.page_size;
  }
  bool operator<(const translation_key &other) const {
    if (asid != other.asid) return asid < other.asid;
    if (vpn != other.vpn) return vpn < other.vpn;
    return page_size < other.page_size;
  }
};

// v0 keeps the identity-like data mapping from M1. The key allows later
// backends to change PPN policy without changing request or TLB contracts.
class present_page_mapper {
 public:
  uint64_t translate(const translation_key &key) const;
};

// M3's PTE addresses are a simulator modeling contract.  The generic defaults
// reserve a configuration-derived range above the identity-like application
// range; they are neither a hardware claim nor a Segmentation-paper detail.
// The current generic trace baseline is 56 bits.  A 49-bit configuration
// remains supported for later paper-specific work.  M3 requires 64KB and 2MB
// translation keys, so the generic v0 backend rejects other PTE classes.
struct page_table_config {
  unsigned levels;
  unsigned virtual_address_bits;
  uint64_t application_physical_limit;
  uint64_t pte_physical_base;
  uint64_t pte_physical_bytes;
  page_table_config(unsigned level_count = 4, unsigned va_bits = 56,
                    uint64_t app_limit = 1ULL << 56,
                    uint64_t pte_base = 1ULL << 56,
                    uint64_t pte_bytes = 1ULL << 46)
      : levels(level_count), virtual_address_bits(va_bits),
        application_physical_limit(app_limit), pte_physical_base(pte_base),
        pte_physical_bytes(pte_bytes) {}
  bool valid() const;
};

// A PTE request has a physical address at construction time.  The explicit
// flags form the G3-1 no-recursion contract that G3-2 will carry on a real
// mem_fetch through L2/DRAM.
struct pte_request {
  translation_key key;
  unsigned level;
  uint64_t physical_address;
  uint64_t request_id;
  bool is_physical;
  bool bypass_translation;
  pte_request(const translation_key &k = translation_key(), unsigned l = 0,
              uint64_t pa = 0, uint64_t id = 0)
      : key(k), level(l), physical_address(pa), request_id(id),
        is_physical(true), bypass_translation(true) {}
};

// Replaceable backend boundary.  M2 keeps the identity-like present mapping;
// G3-2 will consume pte_request objects through the real memory hierarchy.
class page_table_backend {
 public:
  virtual ~page_table_backend() {}
  virtual bool valid() const = 0;
  virtual unsigned levels() const = 0;
  virtual uint64_t resolve_ppn(const translation_key &key) const = 0;
  virtual pte_request make_pte_request(const translation_key &key,
                                       unsigned level,
                                       uint64_t request_id) const = 0;
  virtual bool owns_pte_physical_address(uint64_t pa) const = 0;
  // Intermediate-cache clients need the logical radix identity, never a
  // synthetic physical PTE address.  Backends that do not expose a radix
  // hierarchy deliberately return false; the generic M3 backend overrides
  // this contract below.
  virtual bool pte_prefix_identity(const translation_key &key, unsigned level,
                                   unsigned *page_class,
                                   uint64_t *prefix) const {
    (void)key;
    (void)level;
    (void)page_class;
    (void)prefix;
    return false;
  }
};

class radix_page_table_backend : public page_table_backend {
 public:
  explicit radix_page_table_backend(
      const page_table_config &config = page_table_config());
  bool valid() const;
  unsigned levels() const { return m_config.levels; }
  uint64_t resolve_ppn(const translation_key &key) const;
  pte_request make_pte_request(const translation_key &key, unsigned level,
                               uint64_t request_id) const;
  bool owns_pte_physical_address(uint64_t pa) const;
  // A false result is an explicit configuration-width rejection.  The real
  // PTE path asserts this predicate rather than rewriting a raw SimVA.
  bool supports_key(const translation_key &key) const;
  const page_table_config &config() const { return m_config; }
  // These accessors make the frozen radix decomposition directly testable.
  // Prefixes are the high prefix_bits_for_level() bits of the configured VPN.
  unsigned level_width(uint64_t page_size, unsigned level) const;
  unsigned prefix_bits_for_level(uint64_t page_size, unsigned level) const;
  uint64_t vpn_prefix(const translation_key &key, unsigned level) const;
  uint64_t pte_namespace_offset(uint64_t page_size, unsigned level) const;
  bool pte_prefix_identity(const translation_key &key, unsigned level,
                           unsigned *page_class,
                           uint64_t *prefix) const;

 private:
  unsigned page_size_class(uint64_t page_size) const;
  unsigned vpn_bits(uint64_t page_size) const;
  uint64_t pte_address(const translation_key &key, unsigned level) const;
  page_table_config m_config;
};

struct tlb_config {
  unsigned entries;
  unsigned assoc;
  unsigned ports_per_cycle;
  tlb_config(unsigned e = 32, unsigned a = 32, unsigned p = 1)
      : entries(e), assoc(a), ports_per_cycle(p) {}
  bool valid() const;
  unsigned sets() const;
};

// The accepted M1--M4C path is the exact-page TLB below.  M4B deliberately
// makes the sub-entry organization an opt-in L2 candidate so that enabling no
// new option retains the existing object, tag, replacement, and timing path.
enum l2_tlb_mode {
  L2_TLB_STANDARD = 0,
  L2_TLB_SUBENTRY_16 = 1
};

// M4C object labels are observability metadata.  They are intentionally not
// part of translation_key, so they cannot affect a TLB match, replacement, or
// timing decision.
enum object_class {
  OBJECT_UNKNOWN = 0,
  OBJECT_WEIGHT = 1,
  OBJECT_KV_CACHE = 2,
  OBJECT_CLASS_COUNT = 3
};

// Observational completion provenance for cross-layer M4C telemetry.  These
// values mirror the generic mem_access carrier but remain a VM-local API so
// the accepted controller is independently testable.
enum translation_source {
  TRANSLATION_SOURCE_UNOBSERVED = 0,
  TRANSLATION_SOURCE_DISABLED,
  TRANSLATION_SOURCE_IDEAL_IDENTITY,
  TRANSLATION_SOURCE_L1_TLB_HIT,
  TRANSLATION_SOURCE_L2_TLB_HIT,
  TRANSLATION_SOURCE_PTW,
  TRANSLATION_SOURCE_SEGMENT_HIT
};

// C10-A carries request access intent into the candidate path.  Existing
// callers retain READ as the source-compatible default; simulator call-sites
// must pass their real access class before any replay is authorized.
enum translation_access {
  TRANSLATION_ACCESS_READ = 0,
  TRANSLATION_ACCESS_WRITE = 1,
  TRANSLATION_ACCESS_ATOMIC = 2
};

// A V2 registration may be architecturally rejected without being a malformed
// simulator configuration.  The result is machine-readable and, critically,
// has no live descriptor image.  It is distinct from a missing/unreadable
// artifact, which remains a project configuration error.
enum segment_registration_status {
  SEGMENT_REGISTRATION_DISABLED = 0,
  SEGMENT_REGISTRATION_ACCEPTED_V2,
  SEGMENT_REGISTRATION_ACCEPTED_HISTORICAL_V1,
  SEGMENT_REGISTRATION_REJECTED_EMPTY,
  SEGMENT_REGISTRATION_REJECTED_CAPACITY,
  SEGMENT_REGISTRATION_REJECTED_OVERLAP,
  SEGMENT_REGISTRATION_REJECTED_UNSORTED,
  SEGMENT_REGISTRATION_REJECTED_ASID_EPOCH,
  SEGMENT_REGISTRATION_REJECTED_RIGHTS,
  SEGMENT_REGISTRATION_REJECTED_MAPPING_CLASS,
  SEGMENT_REGISTRATION_REJECTED_EXTENT
};

const char *segment_registration_status_name(segment_registration_status status);

enum segment_lifecycle_state {
  SEGMENT_LIFECYCLE_INACTIVE = 0,
  SEGMENT_LIFECYCLE_INSTALLING,
  SEGMENT_LIFECYCLE_ACTIVE,
  SEGMENT_LIFECYCLE_REVOKING
};

const char *segment_lifecycle_state_name(segment_lifecycle_state state);

const char *object_class_name(object_class object);

// A frozen line-oriented schema replaces per-run ad-hoc metadata parsing.  A
// non-empty map path must parse and validate in full before simulation starts.
class object_range_map {
 public:
  explicit object_range_map(const std::string &path = "");
  bool enabled() const { return m_enabled; }
  object_class classify(uint64_t start, uint64_t bytes) const;

 private:
  struct range {
    uint64_t start;
    uint64_t end;
    object_class object;
    range(uint64_t s = 0, uint64_t e = 0,
          object_class c = OBJECT_UNKNOWN)
        : start(s), end(e), object(c) {}
  };
  bool m_enabled;
  std::vector<range> m_ranges;
};

// Immutable descriptors for the C10-A Weight Segment candidate.  V2 records
// a privileged ASID-scoped VA extent and its real PA extent; it is not an
// identity mapping and is not derived from the telemetry object map.  V1 is
// retained only to reproduce the frozen historical C3 artifact.
class weight_segment_map {
 public:
  enum fallback_reason {
    SEGMENT_FALLBACK_NONE = 0,
    SEGMENT_FALLBACK_NO_DESCRIPTOR,
    SEGMENT_FALLBACK_ASID,
    SEGMENT_FALLBACK_EPOCH,
    SEGMENT_FALLBACK_RIGHTS,
    SEGMENT_FALLBACK_BOUNDARY
  };
  explicit weight_segment_map(const std::string &path = "");
  bool enabled() const { return m_enabled; }
  bool registered_v2() const { return m_registered_v2; }
  bool registration_accepted() const {
    return m_registration_status == SEGMENT_REGISTRATION_ACCEPTED_V2 ||
           m_registration_status == SEGMENT_REGISTRATION_ACCEPTED_HISTORICAL_V1;
  }
  segment_registration_status registration_status() const {
    return m_registration_status;
  }
  unsigned size() const { return m_ranges.size(); }
  unsigned active_asid() const { return m_active_asid; }
  unsigned active_epoch() const { return m_active_epoch; }
  bool translate(uint64_t start, uint64_t bytes, uint64_t page_size,
                 uint64_t *ppn) const;
  bool translate(const translation_key &key, uint64_t start, uint64_t bytes,
                 bool is_read, uint64_t *ppn,
                 fallback_reason *reason = 0) const;
  // Registered pages must resolve identically through ordinary paging and
  // Segment lookup. Returns false for unregistered pages.
  bool registered_ppn(const translation_key &key, uint64_t *ppn) const;

 private:
  struct range {
    unsigned asid;
    unsigned epoch;
    uint64_t va_base_vpn;
    uint64_t va_limit_vpn;
    uint64_t pa_base_ppn;
    bool read_only;
    unsigned mapping_class;
    range(unsigned a = 0, unsigned e = 0, uint64_t vb = 0,
          uint64_t vl = 0, uint64_t pb = 0, bool ro = false,
          unsigned mc = 0)
        : asid(a), epoch(e), va_base_vpn(vb), va_limit_vpn(vl),
          pa_base_ppn(pb), read_only(ro), mapping_class(mc) {}
  };
  bool m_enabled;
  bool m_registered_v2;
  segment_registration_status m_registration_status;
  unsigned m_active_asid;
  unsigned m_active_epoch;
  std::vector<range> m_ranges;
};

struct segment_config {
  bool enabled;
  unsigned entries;
  unsigned lookup_latency;
  std::string map_path;
  segment_config(bool enable = false, unsigned entry_count = 0,
                 unsigned service_latency = 0,
                 const std::string &segment_map = "")
      : enabled(enable), entries(entry_count), lookup_latency(service_latency),
        map_path(segment_map) {}
  bool valid() const {
    // A disabled Segment engine may still carry a V2 driver registration as
    // the ordinary-page-table PA backend.  This is required for fair arms:
    // disabling the optional bypass must not silently change the registered
    // VA->PA mapping seen by the conventional hierarchy.
    if (!enabled) return entries == 0 && lookup_latency == 0;
    return entries != 0 && lookup_latency != 0 && !map_path.empty();
  }
};

// Official C9 comparison arms are selected as a configuration contract, not
// inferred from arbitrary entry counts. H0 is deliberately represented only
// so the selector can reject it permanently.
enum fair_arm_id {
  FAIR_ARM_MANUAL = 0,
  FAIR_ARM_F0_BASELINE_EXACT,
  FAIR_ARM_F1_SUBENTRY_G96,
  FAIR_ARM_F2_EXACT_E688,
  FAIR_ARM_F3_EXACT_SWEEP,
  FAIR_ARM_F4_EXACT_E1536,
  FAIR_ARM_F5_PHYSICAL_PWC,
  FAIR_ARM_F6_EXACT_2M_E848,
  FAIR_ARM_F7_SEGMENT_EXACT_E320,
  FAIR_ARM_F8_SEGMENT_SUBENTRY_G32,
  FAIR_ARM_F9_EXACT_E656,
  FAIR_ARM_H0_HISTORICAL_UNFAIR
};

const char *fair_arm_name(unsigned arm);

enum pwc_mode {
  PWC_OFF = 0,
  PWC_FINITE = 1,
  PWC_IDEAL = 2,
  // C9 F5 only: three 40-entry physical-pointer banks. This is not an
  // alternative spelling for the generic finite vector.
  PWC_PHYSICAL_F5 = 3
};

// This is a generic M3 modeling decision.  FINITE defaults to the 128-entry
// organization used for the project baseline; it is not a target-paper or
// hardware reconstruction. FINITE has sufficient logical bandwidth for
// active walkers. PWC_PHYSICAL_F5 is a distinct C9 implementation with an
// explicit one-port queue and must have exactly 120 entries.
struct pwc_config {
  unsigned mode;
  unsigned entries;
  unsigned lookup_latency;
  pwc_config(unsigned cache_mode = PWC_FINITE, unsigned entry_count = 128,
             unsigned service_latency = 1)
      : mode(cache_mode), entries(entry_count), lookup_latency(service_latency) {}
  bool valid() const;
};

struct tlb_stats {
  uint64_t accesses;
  uint64_t hits;
  uint64_t misses;
  uint64_t evictions;
  uint64_t port_stalls;
  tlb_stats()
      : accesses(0), hits(0), misses(0), evictions(0), port_stalls(0) {}
};

struct tlb_fill_result {
  bool evicted;
  object_class victim_object;
  tlb_fill_result(bool did_evict = false,
                  object_class victim = OBJECT_UNKNOWN)
      : evicted(did_evict), victim_object(victim) {}
};

class set_associative_tlb {
 public:
  explicit set_associative_tlb(const tlb_config &config = tlb_config());
  bool probe(const translation_key &key, uint64_t cycle, uint64_t *ppn);
  tlb_fill_result fill(const translation_key &key, uint64_t ppn,
                       uint64_t cycle,
                       object_class object = OBJECT_UNKNOWN);
  bool invalidate(const translation_key &key);
  void flush_asid(unsigned asid);
  void flush_all();
  bool try_consume_port(uint64_t cycle);
  unsigned occupancy() const;
  const tlb_stats &stats() const { return m_stats; }

 private:
  struct entry {
    bool valid;
    translation_key key;
    uint64_t ppn;
    uint64_t last_touch;
    object_class object;
    entry()
        : valid(false), key(), ppn(0), last_touch(0),
          object(OBJECT_UNKNOWN) {}
  };
  unsigned set_for(const translation_key &key) const;
  tlb_config m_config;
  std::vector<entry> m_entries;
  tlb_stats m_stats;
  uint64_t m_port_cycle;
  unsigned m_ports_used;
  uint64_t m_touch_clock;
};

// C1 selection: REFERENCE_APPROX_SUBENTRY_16.  One L2 entry is a base-VPN
// group with exactly sixteen independently valid base-page translations.  The
// group is the unit of set placement/replacement/LRU; a selected invalid leaf
// is a TLB miss even when the base tag matches.  This is intentionally a
// separate candidate type rather than a mutation of set_associative_tlb.
struct subentry_tlb_stats {
  uint64_t base_tag_hits;
  uint64_t base_tag_misses;
  uint64_t selected_subentry_hits;
  uint64_t selected_subentry_misses;
  uint64_t group_fills;
  uint64_t existing_group_fills;
  uint64_t group_evictions;
  uint64_t valid_subentries_evicted;
  uint64_t valid_subentries_by_object[OBJECT_CLASS_COUNT];
  uint64_t eviction_matrix[OBJECT_CLASS_COUNT][OBJECT_CLASS_COUNT];
  subentry_tlb_stats()
      : base_tag_hits(0), base_tag_misses(0), selected_subentry_hits(0),
        selected_subentry_misses(0), group_fills(0),
        existing_group_fills(0), group_evictions(0),
        valid_subentries_evicted(0), valid_subentries_by_object(),
        eviction_matrix() {}
};

class subentry_tlb {
 public:
  enum { kSubentriesPerGroup = 16 };
  explicit subentry_tlb(const tlb_config &config = tlb_config());
  bool probe(const translation_key &key, uint64_t cycle, uint64_t *ppn);
  tlb_fill_result fill(const translation_key &key, uint64_t ppn,
                       uint64_t cycle,
                       object_class object = OBJECT_UNKNOWN);
  // C10A fair profiles require leaf-granular invalidation. Empty groups free
  // their way immediately; this API is intentionally base-page only.
  bool invalidate(const translation_key &key);
  void flush_asid(unsigned asid);
  void flush_all();
  bool try_consume_port(uint64_t cycle);
  // Exposed for focused fair-profile validation.  The 96- and 32-group
  // profiles must use the configuration-derived set count, never the
  // historical 768-group geometry.
  unsigned sets() const { return m_config.sets(); }
  unsigned occupancy() const;
  unsigned valid_subentries() const;
  const tlb_stats &stats() const { return m_stats; }
  const subentry_tlb_stats &subentry_stats() const { return m_subentry_stats; }

 private:
  struct subentry {
    bool valid;
    uint64_t ppn;
    object_class object;
    subentry() : valid(false), ppn(0), object(OBJECT_UNKNOWN) {}
  };
  struct group_entry {
    bool valid;
    unsigned asid;
    uint64_t base_vpn;
    uint64_t page_size;
    uint64_t last_touch;
    // This is a group-level compatibility label for the existing M4C
    // group-eviction matrix only.  Per-subentry object occupancy/evictions
    // remain separately recorded in subentry_tlb_stats.
    object_class object;
    subentry leaves[kSubentriesPerGroup];
    group_entry()
        : valid(false), asid(0), base_vpn(0), page_size(0), last_touch(0),
          object(OBJECT_UNKNOWN), leaves() {}
  };
  unsigned set_for(unsigned asid, uint64_t base_vpn,
                   uint64_t page_size) const;
  unsigned leaf_for(const translation_key &key) const;
  group_entry *find_group(const translation_key &key);
  const group_entry *find_group(const translation_key &key) const;
  void clear_group(group_entry *group, object_class incoming);
  tlb_config m_config;
  std::vector<group_entry> m_entries;
  tlb_stats m_stats;
  subentry_tlb_stats m_subentry_stats;
  uint64_t m_port_cycle;
  unsigned m_ports_used;
  uint64_t m_touch_clock;
};

struct translation_config {
  unsigned num_sms;
  uint64_t page_size;
  tlb_config l1;
  tlb_config l2;
  unsigned mshr_entries;
  unsigned pwq_entries;
  unsigned walkers;
  unsigned walk_latency;
  // Zero preserves accepted M2 functional diagnostics.  The simulator-facing
  // generic M3 configuration supplies explicit non-zero lookup latencies.
  unsigned l1_lookup_latency;
  unsigned l2_lookup_latency;
  // 0 retains the accepted M2 fixed-latency diagnostic path.  1 makes each
  // active walker wait for a real, physical PTE_ACC_R response from G3-2.
  unsigned ptw_mode;
  page_table_config page_table;
  pwc_config pwc;
  std::string object_map_path;
  unsigned l2_mode;
  segment_config segment;
  unsigned fair_arm;
  // C14 Path N: default-off, counter-only translation-exposure telemetry.
  // It controls no lookup, arbitration, or completion state.
  bool c14_criticality_telemetry;
  translation_config(unsigned sms = 1,
                     uint64_t page = vm_core::kDefaultBasePageSize,
                     const tlb_config &l1_config = tlb_config(),
                     const tlb_config &l2_config = tlb_config(768, 16, 1),
                     unsigned mshr_count = 32, unsigned pwq_count = 32,
                     unsigned walker_count = 16, unsigned latency = 100,
                     const page_table_config &page_table_config_value =
                         page_table_config(),
                     unsigned page_table_walk_mode = 0,
                     const pwc_config &pwc_config_value = pwc_config(),
                     unsigned l1_latency = 0, unsigned l2_latency = 0,
                     const std::string &object_map = "",
                     unsigned l2_tlb_mode_value = L2_TLB_STANDARD,
                     const segment_config &segment_config_value =
                         segment_config(),
                     unsigned fair_arm_value = FAIR_ARM_MANUAL,
                     bool c14_criticality_telemetry_value = false)
      : num_sms(sms), page_size(page), l1(l1_config), l2(l2_config),
        mshr_entries(mshr_count), pwq_entries(pwq_count), walkers(walker_count),
        walk_latency(latency), l1_lookup_latency(l1_latency),
        l2_lookup_latency(l2_latency), ptw_mode(page_table_walk_mode),
        page_table(page_table_config_value), pwc(pwc_config_value),
        object_map_path(object_map), l2_mode(l2_tlb_mode_value),
        segment(segment_config_value), fair_arm(fair_arm_value),
        c14_criticality_telemetry(c14_criticality_telemetry_value) {}
  bool valid() const;
};

// Overwrites only the arm-controlled fields with the frozen C9 realization.
// F5 and H0 return false: neither is an executable official profile.
bool configure_fair_arm(translation_config *config, unsigned arm);
uint64_t fair_arm_charged_bits(const translation_config &config);

enum lookup_result {
  READY,
  L1_PORT_STALL,
  L2_PORT_STALL,
  TRANSLATION_PENDING,
  MSHR_FULL
  , PWQ_FULL
};

struct translation_stats {
  // A request that is not already registered in an active MSHR and therefore
  // proceeds to normal finite-resource lookup arbitration.  This is distinct
  // from TLB probe counters and from pending-waiter retries below.
  uint64_t lookup_requests;
  // Same (key, UID) retry found before either TLB port/probe.  These retries
  // intentionally do not contribute to L1/L2 TLB access or miss counters.
  uint64_t pending_waiter_bypasses;
  uint64_t lookup_inflight_bypasses;
  uint64_t l1_lookup_launches;
  uint64_t l1_lookup_completions;
  uint64_t l1_lookup_service_cycles;
  uint64_t l2_lookup_launches;
  uint64_t l2_lookup_completions;
  uint64_t l2_lookup_service_cycles;
  uint64_t segment_lookup_attempts;
  uint64_t segment_lookup_accepts;
  uint64_t segment_port_denials;
  // C3 reports the L1 probe that runs in parallel with a segment lookup as a
  // raw observation.  The effective counters exclude raw L1 outcomes whose
  // translation result is suppressed by a Weight Segment hit.
  uint64_t segment_lookup_launches;
  uint64_t segment_lookup_completions;
  uint64_t segment_hits;
  uint64_t segment_misses;
  uint64_t segment_raw_l1_completions;
  uint64_t segment_raw_l1_hits;
  uint64_t segment_raw_l1_misses;
  uint64_t segment_effective_l1_hits;
  uint64_t segment_effective_l1_misses;
  uint64_t segment_l2_suppressed;
  uint64_t segment_mshr_suppressed;
  uint64_t segment_pwq_suppressed;
  uint64_t segment_walker_suppressed;
  uint64_t segment_pwc_suppressed;
  uint64_t segment_pte_suppressed;
  uint64_t segment_l1_fill_suppressed;
  uint64_t segment_fallback_by_reason[6];
  uint64_t segment_l1_first_owners;
  uint64_t segment_first_owners;
  uint64_t segment_both_miss;
  uint64_t segment_miss_join_wait_cycles;
  uint64_t segment_late_result_discards;
  uint64_t segment_mapping_mismatch_faults;
  uint64_t segment_install_attempts;
  uint64_t segment_install_acks;
  uint64_t segment_install_rejections;
  uint64_t segment_revoke_attempts;
  uint64_t segment_revoke_acks;
  uint64_t segment_install_replica_acks;
  uint64_t segment_revoke_replica_acks;
  uint64_t segment_epoch_wrap_quiesces;
  uint64_t translation_generation_advances;
  uint64_t translation_generation_wrap_quiesces;
  uint64_t translation_stale_fills_discarded;
  uint64_t translation_stale_ready_discards;
  uint64_t translation_stale_waiters_discarded;
  // Per-requester, critical-path state intervals.  These are deliberately
  // not summed into a fabricated global latency: merged requesters share a
  // single MSHR/walk while retaining their own entry and wakeup times.
  uint64_t requester_completions;
  uint64_t requester_latency_cycles_total;
  uint64_t requester_latency_cycles_max;
  uint64_t requester_l1_queue_cycles_total;
  uint64_t requester_l1_queue_cycles_max;
  uint64_t requester_l1_service_cycles_total;
  uint64_t requester_l1_service_cycles_max;
  uint64_t requester_l2_queue_cycles_total;
  uint64_t requester_l2_queue_cycles_max;
  uint64_t requester_l2_service_cycles_total;
  uint64_t requester_l2_service_cycles_max;
  uint64_t requester_mshr_wait_cycles_total;
  uint64_t requester_mshr_wait_cycles_max;
  // C14 Path N gauge: distinct requesters still represented by a lookup or
  // an MSHR waiter. This is not a global execution-criticality measure.
  uint64_t c14_criticality_pending_requester_samples;
  uint64_t c14_criticality_pending_requester_total;
  uint64_t c14_criticality_pending_requester_high_watermark;
  // Per-unique-PTE-request memory interval; it is never multiplied by MSHR
  // merge depth.
  uint64_t pte_memory_wait_cycles_total;
  uint64_t pte_memory_wait_cycles_max;
  uint64_t mapper_lookups;
  uint64_t completed;
  uint64_t mshr_allocations;
  uint64_t mshr_merges;
  uint64_t mshr_full_events;
  uint64_t waiter_registrations;
  uint64_t waiter_wakeups;
  uint64_t mshr_releases;
  uint64_t pwq_full_events;
  uint64_t walk_starts;
  uint64_t walk_completions;
  uint64_t pwq_wait_cycles;
  uint64_t walk_service_cycles;
  uint64_t mshr_occupancy_high_watermark;
  uint64_t mshr_entries_completed;
  uint64_t mshr_waiter_depth_total;
  uint64_t mshr_waiter_depth_max;
  uint64_t mshr_lifetime_cycles_total;
  uint64_t mshr_lifetime_cycles_max;
  uint64_t pte_requests;
  uint64_t pte_responses;
  uint64_t pte_l2_only_responses;
  uint64_t pte_dram_responses;
  uint64_t pte_response_misassociations;
  uint64_t pwc_accesses;
  uint64_t pwc_hits;
  uint64_t pwc_misses;
  uint64_t pwc_inserts;
  uint64_t pwc_evictions;
  uint64_t pwc_occupancy;
  uint64_t pwc_occupancy_high_watermark;
  uint64_t pwc_service_cycles;
  // F5's one-port physical PWC queue is separate from generic PWC service
  // accounting. A denial leaves an active walk queued; it never re-probes.
  uint64_t pwc_port_accepts;
  uint64_t pwc_port_denials;
  uint64_t pwc_queue_wait_cycles_total;
  uint64_t pwc_queue_wait_cycles_max;
  uint64_t pwc_queue_high_watermark;
  uint64_t pwc_physical_payload_validations;
  std::vector<uint64_t> pwc_occupancy_by_level;
  std::vector<uint64_t> pwc_accesses_by_level;
  std::vector<uint64_t> pwc_hits_by_level;
  std::vector<uint64_t> pwc_misses_by_level;
  std::vector<uint64_t> pwc_pte_requests_skipped_by_level;
  struct object_stats {
    uint64_t translation_requesters;
    uint64_t l1_lookup_launches;
    uint64_t l1_hits;
    uint64_t l1_misses;
    uint64_t l2_lookup_launches;
    uint64_t l2_hits;
    uint64_t l2_misses;
    uint64_t mshr_allocations;
    uint64_t mshr_merges;
    uint64_t walk_starts;
    uint64_t completed;
    uint64_t requester_latency_cycles_total;
    uint64_t requester_latency_cycles_max;
    uint64_t requester_mshr_wait_cycles_total;
    uint64_t requester_mshr_wait_cycles_max;
    uint64_t pwc_accesses;
    uint64_t pwc_hits;
    uint64_t pwc_misses;
    uint64_t pte_requests;
    uint64_t pte_l2_only_responses;
    uint64_t pte_dram_responses;
    uint64_t pte_memory_wait_cycles_total;
    uint64_t pte_memory_wait_cycles_max;
    uint64_t l2_fills;
    object_stats()
        : translation_requesters(0), l1_lookup_launches(0), l1_hits(0),
          l1_misses(0), l2_lookup_launches(0), l2_hits(0), l2_misses(0),
          mshr_allocations(0), mshr_merges(0), walk_starts(0), completed(0),
          requester_latency_cycles_total(0), requester_latency_cycles_max(0),
          requester_mshr_wait_cycles_total(0),
          requester_mshr_wait_cycles_max(0), pwc_accesses(0), pwc_hits(0),
          pwc_misses(0), pte_requests(0), pte_l2_only_responses(0),
          pte_dram_responses(0), pte_memory_wait_cycles_total(0),
          pte_memory_wait_cycles_max(0), l2_fills(0) {}
  };
  bool object_attribution_enabled;
  object_stats object[OBJECT_CLASS_COUNT];
  uint64_t l2_replacement_matrix[OBJECT_CLASS_COUNT][OBJECT_CLASS_COUNT];
  translation_stats()
      : lookup_requests(0), pending_waiter_bypasses(0),
        lookup_inflight_bypasses(0), l1_lookup_launches(0),
        l1_lookup_completions(0), l1_lookup_service_cycles(0),
        l2_lookup_launches(0), l2_lookup_completions(0),
        l2_lookup_service_cycles(0), segment_lookup_attempts(0),
        segment_lookup_accepts(0), segment_port_denials(0),
        segment_lookup_launches(0),
        segment_lookup_completions(0), segment_hits(0), segment_misses(0),
        segment_raw_l1_completions(0), segment_raw_l1_hits(0),
        segment_raw_l1_misses(0), segment_effective_l1_hits(0),
        segment_effective_l1_misses(0), segment_l2_suppressed(0),
        segment_mshr_suppressed(0), segment_pwq_suppressed(0),
        segment_walker_suppressed(0), segment_pwc_suppressed(0),
        segment_pte_suppressed(0), segment_l1_fill_suppressed(0),
        segment_fallback_by_reason(), segment_l1_first_owners(0),
        segment_first_owners(0), segment_both_miss(0),
        segment_miss_join_wait_cycles(0), segment_late_result_discards(0),
        segment_mapping_mismatch_faults(0), segment_install_attempts(0),
        segment_install_acks(0), segment_install_rejections(0),
        segment_revoke_attempts(0), segment_revoke_acks(0),
        segment_install_replica_acks(0), segment_revoke_replica_acks(0),
        segment_epoch_wrap_quiesces(0), translation_generation_advances(0),
        translation_generation_wrap_quiesces(0),
        translation_stale_fills_discarded(0),
        translation_stale_ready_discards(0),
        translation_stale_waiters_discarded(0),
        requester_completions(0),
        requester_latency_cycles_total(0), requester_latency_cycles_max(0),
        requester_l1_queue_cycles_total(0), requester_l1_queue_cycles_max(0),
        requester_l1_service_cycles_total(0),
        requester_l1_service_cycles_max(0),
        requester_l2_queue_cycles_total(0), requester_l2_queue_cycles_max(0),
        requester_l2_service_cycles_total(0),
        requester_l2_service_cycles_max(0), requester_mshr_wait_cycles_total(0),
        requester_mshr_wait_cycles_max(0),
        c14_criticality_pending_requester_samples(0),
        c14_criticality_pending_requester_total(0),
        c14_criticality_pending_requester_high_watermark(0),
        pte_memory_wait_cycles_total(0),
        pte_memory_wait_cycles_max(0), mapper_lookups(0),
        completed(0), mshr_allocations(0), mshr_merges(0),
        mshr_full_events(0), waiter_registrations(0), waiter_wakeups(0),
        mshr_releases(0), pwq_full_events(0), walk_starts(0),
        walk_completions(0), pwq_wait_cycles(0), walk_service_cycles(0),
        mshr_occupancy_high_watermark(0), mshr_entries_completed(0),
        mshr_waiter_depth_total(0), mshr_waiter_depth_max(0),
        mshr_lifetime_cycles_total(0), mshr_lifetime_cycles_max(0),
        pte_requests(0), pte_responses(0), pte_l2_only_responses(0),
        pte_dram_responses(0), pte_response_misassociations(0),
        pwc_accesses(0), pwc_hits(0), pwc_misses(0), pwc_inserts(0),
        pwc_evictions(0), pwc_occupancy(0), pwc_occupancy_high_watermark(0),
        pwc_service_cycles(0), pwc_port_accepts(0), pwc_port_denials(0),
        pwc_queue_wait_cycles_total(0), pwc_queue_wait_cycles_max(0),
        pwc_queue_high_watermark(0), pwc_physical_payload_validations(0),
        pwc_occupancy_by_level(), pwc_accesses_by_level(), pwc_hits_by_level(),
        pwc_misses_by_level(), pwc_pte_requests_skipped_by_level(),
        object_attribution_enabled(false), object(), l2_replacement_matrix() {}
};

// G2-1 controller: hit latency is zero, but both TLBs have finite lookup
// ports. L2 misses synchronously resolve through the resident mapper. G2-2
// replaces the miss action with an MSHR and G2-3 provides walker completion.
class translation_controller {
 public:
  explicit translation_controller(const translation_config &config);
  // The caller retains an injected backend's lifetime.  This is a narrow test
  // and future-adapter seam; the normal simulator constructor uses the generic
  // radix backend above.
  translation_controller(const translation_config &config,
                         page_table_backend *backend);
  lookup_result translate(unsigned sid, unsigned asid, uint64_t sim_va,
                          uint64_t request_bytes, uint64_t cycle,
                          uint64_t waiter_uid,
                          uint64_t *sim_pa,
                          translation_source *source = 0,
                          translation_access access =
                              TRANSLATION_ACCESS_READ);
  // Retained for the accepted M1-M3 directed tests.  Simulator callers pass
  // the real coalesced transaction size through the overload above.
  lookup_result translate(unsigned sid, unsigned asid, uint64_t sim_va,
                          uint64_t cycle, uint64_t waiter_uid,
                          uint64_t *sim_pa) {
    return translate(sid, asid, sim_va, 1, cycle, waiter_uid, sim_pa, 0);
  }
  // G2-2 test hook and G2-3 walker completion entry point.
  bool complete_translation(const translation_key &key, uint64_t cycle);
  void cycle(uint64_t cycle);
  bool uses_real_memory_ptw() const { return m_config.ptw_mode == 1; }
  // A request remains available until pte_request_issued() commits it to the
  // real interconnect.  This makes injection backpressure explicit without
  // letting a stalled injection advance a walker.
  bool next_pte_request(pte_request *request) const;
  bool pte_request_issued(const pte_request &request, uint64_t cycle);
  bool complete_pte_response(uint64_t request_id, uint64_t physical_address,
                             bool reached_dram, uint64_t cycle);
  bool invariants_hold() const;
  bool quiescent_invariants_hold() const;
  unsigned active_mshrs() const { return m_mshrs.size(); }
  unsigned active_walkers() const { return m_active_walks.size(); }
  const translation_config &config() const { return m_config; }
  const set_associative_tlb &l1(unsigned sid) const;
  // Retained exact-page L2 accessor for M1--M4C directed tests.  It is the
  // active L2 only in L2_TLB_STANDARD mode.
  const set_associative_tlb &l2() const { return m_l2; }
  const subentry_tlb &l2_subentries() const { return m_l2_subentries; }
  const translation_stats &stats() const { return m_stats; }
  // Deterministic privileged-control seam for C9's pinned epoch. No Segment
  // descriptor is eligible until every local replica explicitly acknowledges
  // the staged image; revoke similarly removes every image before reuse.
  bool begin_segment_install();
  bool acknowledge_segment_install(unsigned sid);
  bool begin_segment_revoke();
  bool acknowledge_segment_revoke(unsigned sid);
  bool quiesce_segment_epoch_wrap();
  bool segment_active() const {
    return m_segment_lifecycle == SEGMENT_LIFECYCLE_ACTIVE;
  }
  segment_lifecycle_state segment_lifecycle() const {
    return m_segment_lifecycle;
  }
  unsigned segment_active_asid() const { return m_segment_active_asid; }
  unsigned segment_active_epoch() const { return m_segment_active_epoch; }
  // Conventional translation generation is intentionally independent of the
  // Segment driver epoch. A shootdown invalidates resident L1/L2 state and
  // causes in-flight old-generation fills to be discarded.
  void invalidate_translation(const translation_key &key);
  void flush_translation_asid(unsigned asid);
  void flush_translation_all();
  uint64_t translation_generation(unsigned asid) const;
  bool object_attribution_conserves() const;
  void print_stats(FILE *fout) const;

 private:
  struct waiter {
    unsigned sid;
    uint64_t uid;
    uint64_t entry_cycle;
    uint64_t l1_launch_cycle;
    uint64_t l1_complete_cycle;
    bool l2_issued;
    uint64_t l2_launch_cycle;
    uint64_t l2_complete_cycle;
    uint64_t mshr_join_cycle;
    object_class object;
    waiter(unsigned s, uint64_t u, uint64_t entry, uint64_t l1_launch,
           uint64_t l1_complete, bool l2_issued, uint64_t l2_launch,
           uint64_t l2_complete, uint64_t mshr_join,
           object_class object_classification)
        : sid(s), uid(u), entry_cycle(entry), l1_launch_cycle(l1_launch),
          l1_complete_cycle(l1_complete), l2_issued(l2_issued),
          l2_launch_cycle(l2_launch),
          l2_complete_cycle(l2_complete), mshr_join_cycle(mshr_join),
          object(object_classification) {}
  };
  struct mshr_entry {
    translation_key key;
    std::vector<waiter> waiters;
    uint64_t enqueue_cycle;
    uint64_t generation;
    object_class object;
    mshr_entry(const translation_key &k, uint64_t enqueue,
               uint64_t captured_generation,
               object_class object_classification)
        : key(k), waiters(), enqueue_cycle(enqueue),
          generation(captured_generation),
          object(object_classification) {}
    bool has_waiter(uint64_t uid) const;
  };
  mshr_entry *find_mshr(const translation_key &key);
  const mshr_entry *find_mshr(const translation_key &key) const;
  enum lookup_stage {
    LOOKUP_L1_SERVICE,
    LOOKUP_L2_LAUNCH,
    LOOKUP_L2_SERVICE,
    LOOKUP_MSHR_HANDOFF,
    LOOKUP_READY
  };
  struct lookup_operation {
    unsigned sid;
    uint64_t uid;
    translation_key key;
    lookup_stage stage;
    uint64_t entry_cycle;
    uint64_t l1_launch_cycle;
    uint64_t l1_complete_cycle;
    bool l2_issued;
    uint64_t l2_launch_cycle;
    uint64_t l2_complete_cycle;
    uint64_t ready_cycle;
    uint64_t ppn;
    uint64_t sim_va;
    uint64_t request_bytes;
    uint64_t generation;
    object_class object;
    translation_access access;
    translation_source source;
    bool segment_launched;
    uint64_t segment_ready_cycle;
    bool segment_completed;
    bool segment_hit;
    uint64_t segment_ppn;
    unsigned segment_fallback_reason;
    bool segment_join_accounted;
    bool l1_completed;
    bool l1_hit;
    uint64_t l1_ppn;
    lookup_operation(unsigned s, uint64_t u, const translation_key &k,
                     uint64_t entry, uint64_t ready,
                     uint64_t va, uint64_t bytes, uint64_t captured_generation,
                     object_class object_classification,
                     translation_access request_access, bool launch_segment,
                     uint64_t segment_ready)
        : sid(s), uid(u), key(k), stage(LOOKUP_L1_SERVICE),
          entry_cycle(entry), l1_launch_cycle(entry), l1_complete_cycle(0),
          l2_issued(false),
          l2_launch_cycle(0), l2_complete_cycle(0), ready_cycle(ready),
          ppn(0), sim_va(va), request_bytes(bytes),
          generation(captured_generation),
          object(object_classification), access(request_access),
          source(TRANSLATION_SOURCE_UNOBSERVED),
          segment_launched(launch_segment), segment_ready_cycle(segment_ready),
          segment_completed(!launch_segment), segment_hit(false),
          segment_ppn(0), segment_fallback_reason(0),
          segment_join_accounted(false), l1_completed(false), l1_hit(false),
          l1_ppn(0) {}
  };
  struct completed_outcome {
    translation_key key;
    translation_source source;
    uint64_t generation;
    completed_outcome(const translation_key &completed_key = translation_key(),
                      translation_source completed_source =
                          TRANSLATION_SOURCE_UNOBSERVED,
                      uint64_t completed_generation = 0)
        : key(completed_key), source(completed_source),
          generation(completed_generation) {}
  };
  lookup_operation *find_lookup(unsigned sid, uint64_t uid,
                                const translation_key &key);
  const lookup_operation *find_lookup(unsigned sid, uint64_t uid,
                                      const translation_key &key) const;
  struct active_walk {
    translation_key key;
    uint64_t start_cycle;
    uint64_t ready_cycle;
    unsigned next_level;
    bool pte_outstanding;
    uint64_t pte_request_id;
    uint64_t pte_issue_cycle;
    bool pwc_probe_scheduled;
    uint64_t pwc_probe_enqueue_cycle;
    uint64_t pwc_probe_ready_cycle;
    bool pwc_miss_ready;
    object_class object;
    active_walk(const translation_key &k, uint64_t start, uint64_t ready,
                object_class object_classification)
        : key(k), start_cycle(start), ready_cycle(ready), next_level(0),
          pte_outstanding(false), pte_request_id(0), pte_issue_cycle(0),
          pwc_probe_scheduled(false), pwc_probe_enqueue_cycle(~0ULL),
          pwc_probe_ready_cycle(0),
          pwc_miss_ready(false), object(object_classification) {}
  };
  struct pwc_entry {
    unsigned asid;
    unsigned page_class;
    unsigned level;
    uint64_t prefix;
    uint64_t last_touch;
    pwc_entry(unsigned a, unsigned c, unsigned l, uint64_t p, uint64_t touch)
        : asid(a), page_class(c), level(l), prefix(p), last_touch(touch) {}
  };
  // F5 models physical non-leaf PTE payload rather than the legacy logical
  // vector. The storage fields intentionally match the C9 accounting ABI:
  // valid, ASID, level, VPN prefix, 33-bit next-table PPN and two attributes.
  struct physical_f5_pwc_entry {
    bool valid;
    unsigned asid;
    unsigned level;
    uint64_t prefix;
    uint64_t next_table_ppn;
    unsigned attributes;
    physical_f5_pwc_entry()
        : valid(false), asid(0), level(0), prefix(0), next_table_ppn(0),
          attributes(0) {}
  };
  lookup_result allocate_or_merge(unsigned sid, uint64_t waiter_uid,
                                  const translation_key &key, uint64_t cycle,
                                  const lookup_operation *lookup);
  void note_mshr_occupancy();
  void note_c14_pending_requester_occupancy();
  void note_requester_completion(uint64_t entry_cycle,
                                 uint64_t l1_launch_cycle,
                                 uint64_t l1_complete_cycle,
                                 bool l2_issued,
                                 uint64_t l2_launch_cycle,
                                 uint64_t l2_complete_cycle,
                                 uint64_t mshr_join_cycle, bool used_mshr,
                                 uint64_t ready_cycle, object_class object);
  void service_lookups(uint64_t cycle);
  void advance_translation_generation(unsigned asid);
  void invalidate_resident_translation(const translation_key &key);
  void flush_resident_translation_asid(unsigned asid);
  void flush_resident_translation_all();
  bool pwc_enabled() const { return m_config.pwc.mode != PWC_OFF; }
  bool physical_f5_pwc_enabled() const {
    return m_config.pwc.mode == PWC_PHYSICAL_F5;
  }
  bool pwc_is_leaf(unsigned level) const {
    return level + 1 == m_page_table->levels();
  }
  bool pwc_lookup(const active_walk &walk);
  void pwc_insert(const translation_key &key, unsigned level);
  void service_pwc(uint64_t cycle);
  bool pwc_identity(const translation_key &key, unsigned level,
                    unsigned *page_class, uint64_t *prefix) const;
  bool physical_f5_pwc_payload(const translation_key &key, unsigned level,
                               uint64_t prefix, uint64_t *next_table_ppn,
                               unsigned *attributes) const;
  unsigned physical_f5_pwc_set(unsigned asid, uint64_t prefix) const;
  unsigned physical_f5_pwc_victim(unsigned level, unsigned set) const;
  void physical_f5_pwc_touch(unsigned level, unsigned set, unsigned way);
  void flush_physical_f5_pwc_asid(unsigned asid);
  void flush_physical_f5_pwc_all();
  unsigned physical_f5_pwc_queue_depth() const;
  void initialize_pwc_stats();
  object_class classify_key(const translation_key &key) const;
  void note_object_requester(object_class object, const translation_key &key);
  translation_config m_config;
  radix_page_table_backend m_default_page_table;
  page_table_backend *m_page_table;
  std::vector<set_associative_tlb> m_l1s;
  set_associative_tlb m_l2;
  subentry_tlb m_l2_subentries;
  std::vector<mshr_entry> m_mshrs;
  std::vector<lookup_operation> m_lookups;
  std::vector<translation_key> m_pwq;
  std::vector<active_walk> m_active_walks;
  std::vector<pwc_entry> m_pwc;
  std::vector<physical_f5_pwc_entry> m_physical_f5_pwc;
  std::vector<unsigned> m_physical_f5_pwc_plru;
  // A PTW-completed requester re-enters through the existing L1 lookup path.
  // Preserve that source label only until that normal completion is observed;
  // this map has no timing or flow-control role.
  std::map<uint64_t, completed_outcome> m_completed_outcomes;
  object_range_map m_object_map;
  weight_segment_map m_weight_segments;
  // Immutable V2 descriptors are replicated one-for-one with the translation
  // clusters (the existing per-SID L1 model).  The canonical map below is
  // used only by the normal PTE backend consistency path.
  std::vector<weight_segment_map> m_weight_segment_locals;
  std::vector<bool> m_segment_install_acks;
  std::vector<bool> m_segment_revoke_acks;
  segment_lifecycle_state m_segment_lifecycle;
  unsigned m_segment_active_asid;
  unsigned m_segment_active_epoch;
  bool m_segment_epoch_wrap_requires_quiesce;
  std::map<unsigned, uint64_t> m_translation_generations;
  std::set<translation_key> m_object_unique_keys[OBJECT_CLASS_COUNT];
  translation_stats m_stats;
  uint64_t m_next_pte_request_id;
  uint64_t m_pwc_touch_clock;
  uint64_t m_physical_f5_pwc_port_cycle;
  unsigned m_physical_f5_pwc_ports_used;
};

}  // namespace vm_translation

#endif
