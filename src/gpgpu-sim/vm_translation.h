// Functional VM translation structures shared by the M2/M3 pipeline.
#ifndef GPGPU_SIM_VM_TRANSLATION_H
#define GPGPU_SIM_VM_TRANSLATION_H

#include <stdint.h>
#include <stdio.h>

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
  translation_config(unsigned sms = 1,
                     uint64_t page = vm_core::kDefaultBasePageSize,
                     const tlb_config &l1_config = tlb_config(),
                     const tlb_config &l2_config = tlb_config(768, 16, 1),
                     unsigned mshr_count = 32)
      : num_sms(sms), page_size(page), l1(l1_config), l2(l2_config),
        mshr_entries(mshr_count) {}
  bool valid() const;
};

enum lookup_result {
  READY,
  L1_PORT_STALL,
  L2_PORT_STALL,
  TRANSLATION_PENDING,
  MSHR_FULL
};

struct translation_stats {
  uint64_t mapper_lookups;
  uint64_t completed;
  uint64_t mshr_allocations;
  uint64_t mshr_merges;
  uint64_t mshr_full_events;
  uint64_t waiter_registrations;
  uint64_t waiter_wakeups;
  uint64_t mshr_releases;
  translation_stats()
      : mapper_lookups(0), completed(0), mshr_allocations(0),
        mshr_merges(0), mshr_full_events(0), waiter_registrations(0),
        waiter_wakeups(0), mshr_releases(0) {}
};

// G2-1 controller: hit latency is zero, but both TLBs have finite lookup
// ports. L2 misses synchronously resolve through the resident mapper. G2-2
// replaces the miss action with an MSHR and G2-3 provides walker completion.
class translation_controller {
 public:
  explicit translation_controller(const translation_config &config);
  lookup_result translate(unsigned sid, unsigned asid, uint64_t sim_va,
                          uint64_t cycle, uint64_t waiter_uid,
                          uint64_t *sim_pa);
  // G2-2 test hook and G2-3 walker completion entry point.
  bool complete_translation(const translation_key &key, uint64_t cycle);
  bool invariants_hold() const;
  bool quiescent_invariants_hold() const;
  unsigned active_mshrs() const { return m_mshrs.size(); }
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
    mshr_entry(const translation_key &k) : key(k), waiters() {}
    bool has_waiter(uint64_t uid) const;
  };
  mshr_entry *find_mshr(const translation_key &key);
  const mshr_entry *find_mshr(const translation_key &key) const;
  lookup_result allocate_or_merge(unsigned sid, uint64_t waiter_uid,
                                  const translation_key &key);
  translation_config m_config;
  present_page_mapper m_mapper;
  std::vector<set_associative_tlb> m_l1s;
  set_associative_tlb m_l2;
  std::vector<mshr_entry> m_mshrs;
  translation_stats m_stats;
};

}  // namespace vm_translation

#endif
