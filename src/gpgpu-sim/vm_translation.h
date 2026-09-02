// Functional VM translation structures shared by the M2/M3 pipeline.
#ifndef GPGPU_SIM_VM_TRANSLATION_H
#define GPGPU_SIM_VM_TRANSLATION_H

#include <stdint.h>
#include <stdio.h>

#include <memory>
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

struct tlb_stats {
  uint64_t accesses;
  uint64_t hits;
  uint64_t misses;
  uint64_t evictions;
  uint64_t port_stalls;
  tlb_stats()
      : accesses(0), hits(0), misses(0), evictions(0), port_stalls(0) {}
};

class set_associative_tlb {
 public:
  explicit set_associative_tlb(const tlb_config &config = tlb_config());
  bool probe(const translation_key &key, uint64_t cycle, uint64_t *ppn);
  void fill(const translation_key &key, uint64_t ppn, uint64_t cycle);
  bool try_consume_port(uint64_t cycle);
  unsigned occupancy() const;
  const tlb_stats &stats() const { return m_stats; }

 private:
  struct entry {
    bool valid;
    translation_key key;
    uint64_t ppn;
    uint64_t last_touch;
    entry() : valid(false), key(), ppn(0), last_touch(0) {}
  };
  unsigned set_for(const translation_key &key) const;
  tlb_config m_config;
  std::vector<entry> m_entries;
  tlb_stats m_stats;
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
  // 0 retains the accepted M2 fixed-latency diagnostic path.  1 makes each
  // active walker wait for a real, physical PTE_ACC_R response from G3-2.
  unsigned ptw_mode;
  page_table_config page_table;
  translation_config(unsigned sms = 1,
                     uint64_t page = vm_core::kDefaultBasePageSize,
                     const tlb_config &l1_config = tlb_config(),
                     const tlb_config &l2_config = tlb_config(768, 16, 1),
                     unsigned mshr_count = 32, unsigned pwq_count = 32,
                     unsigned walker_count = 16, unsigned latency = 100,
                     const page_table_config &page_table_config_value =
                         page_table_config(),
                     unsigned page_table_walk_mode = 0)
      : num_sms(sms), page_size(page), l1(l1_config), l2(l2_config),
        mshr_entries(mshr_count), pwq_entries(pwq_count), walkers(walker_count),
        walk_latency(latency), ptw_mode(page_table_walk_mode),
        page_table(page_table_config_value) {}
  bool valid() const;
};

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
  translation_stats()
      : lookup_requests(0), pending_waiter_bypasses(0), mapper_lookups(0),
        completed(0), mshr_allocations(0), mshr_merges(0),
        mshr_full_events(0), waiter_registrations(0), waiter_wakeups(0),
        mshr_releases(0), pwq_full_events(0), walk_starts(0),
        walk_completions(0), pwq_wait_cycles(0), walk_service_cycles(0),
        mshr_occupancy_high_watermark(0), mshr_entries_completed(0),
        mshr_waiter_depth_total(0), mshr_waiter_depth_max(0),
        mshr_lifetime_cycles_total(0), mshr_lifetime_cycles_max(0),
        pte_requests(0), pte_responses(0), pte_l2_only_responses(0),
        pte_dram_responses(0), pte_response_misassociations(0) {}
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
                          uint64_t cycle, uint64_t waiter_uid,
                          uint64_t *sim_pa);
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
  const set_associative_tlb &l2() const { return m_l2; }
  const translation_stats &stats() const { return m_stats; }
  void print_stats(FILE *fout) const;

 private:
  struct waiter {
    unsigned sid;
    uint64_t uid;
    waiter(unsigned s, uint64_t u) : sid(s), uid(u) {}
  };
  struct mshr_entry {
    translation_key key;
    std::vector<waiter> waiters;
    uint64_t enqueue_cycle;
    mshr_entry(const translation_key &k, uint64_t enqueue)
        : key(k), waiters(), enqueue_cycle(enqueue) {}
    bool has_waiter(uint64_t uid) const;
  };
  mshr_entry *find_mshr(const translation_key &key);
  const mshr_entry *find_mshr(const translation_key &key) const;
  struct active_walk {
    translation_key key;
    uint64_t start_cycle;
    uint64_t ready_cycle;
    unsigned next_level;
    bool pte_outstanding;
    uint64_t pte_request_id;
    active_walk(const translation_key &k, uint64_t start, uint64_t ready)
        : key(k), start_cycle(start), ready_cycle(ready), next_level(0),
          pte_outstanding(false), pte_request_id(0) {}
  };
  lookup_result allocate_or_merge(unsigned sid, uint64_t waiter_uid,
                                  const translation_key &key, uint64_t cycle);
  void note_mshr_occupancy();
  translation_config m_config;
  radix_page_table_backend m_default_page_table;
  page_table_backend *m_page_table;
  std::vector<set_associative_tlb> m_l1s;
  set_associative_tlb m_l2;
  std::vector<mshr_entry> m_mshrs;
  std::vector<translation_key> m_pwq;
  std::vector<active_walk> m_active_walks;
  translation_stats m_stats;
  uint64_t m_next_pte_request_id;
};

}  // namespace vm_translation

#endif
