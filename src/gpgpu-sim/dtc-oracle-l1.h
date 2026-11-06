// Controlled counterfactual L1 data-availability ownership. No mem_fetch,
// ordinary lower credits, or fabricated responses are created by this queue.
#ifndef DTC_ORACLE_L1_H
#define DTC_ORACLE_L1_H
#include <cassert>
#include <cstdint>
#include <deque>
#include "dtc-l1-common.h"
namespace dtc_l1 {
class oracle_l1_events {
 public:
  struct event {
    uint64_t uid;
    physical_identity physical;
    uint64_t line_address;
    unsigned owner_kernel_uid;
    uint64_t admitted_cycle;
    uint64_t ready_cycle;
  };
  explicit oracle_l1_events(unsigned capacity = 640) : m_capacity(capacity) {
    assert(capacity);
  }
  void enqueue(uint64_t cycle, physical_identity identity, uint64_t address,
               unsigned owner_kernel_uid) {
    assert(identity.generation && !(address & (kLogicalLineBytes - 1)));
    assert(cycle != UINT64_MAX && m_pending.size() < m_capacity);
    for (const auto &other : m_pending)
      assert((other.physical.id != identity.id ||
              other.physical.generation != identity.generation) &&
             "one virtual event per physical generation");
    assert(m_pending.empty() || m_pending.back().admitted_cycle <= cycle);
    m_pending.push_back({++m_enqueued, identity, address, owner_kernel_uid,
                         cycle, cycle + 1});
    assert_conservation();
  }
  // Created only by the existing finite Tag/allocation pipeline. All due
  // generations can become available together; original complete() still
  // applies finite notifications. No extra refill/return service is claimed.
  template <class Frontend>
  void deliver(uint64_t cycle, Frontend &frontend) {
    if (cycle == m_last_delivery_cycle) return;
    assert(m_last_delivery_cycle == UINT64_MAX || cycle > m_last_delivery_cycle);
    m_last_delivery_cycle = cycle;
    while (!m_pending.empty() && m_pending.front().ready_cycle <= cycle) {
      const event current = m_pending.front();
      assert(cycle > current.admitted_cycle);
      assert(frontend.fill_identity_matches(current.physical) &&
             "virtual event must match original live physical generation");
      frontend.complete(current.physical);
      m_pending.pop_front();
      ++m_delivered;
      assert_conservation();
    }
  }
  bool pending(unsigned kernel_uid = 0) const {
    if (!kernel_uid) return !m_pending.empty();
    for (const auto &current : m_pending)
      if (current.owner_kernel_uid == kernel_uid) return true;
    return false;
  }
  size_t size() const { return m_pending.size(); }
  uint64_t enqueued() const { return m_enqueued; }
  uint64_t delivered() const { return m_delivered; }
  void assert_conservation() const {
    assert(m_enqueued == m_delivered + m_pending.size());
  }
  void assert_drained() const {
    assert_conservation();
    assert(m_pending.empty() && m_enqueued == m_delivered);
  }
 private:
  unsigned m_capacity;
  std::deque<event> m_pending;
  uint64_t m_enqueued = 0;
  uint64_t m_delivered = 0;
  uint64_t m_last_delivery_cycle = UINT64_MAX;
};
}  // namespace dtc_l1
#endif
