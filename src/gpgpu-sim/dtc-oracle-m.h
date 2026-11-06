#ifndef DTC_CONTROLLED_ORACLE_M_H
#define DTC_CONTROLLED_ORACLE_M_H
#include <cassert>
#include <cstdint>
#include <deque>
#include <set>
#include <algorithm>
namespace dtc_oracle {
// Exactly one finite parent descriptor per already-admitted lower credit.
// No payload cache and no additional request admission are implemented here.
template <class Parent> class finite_read_sequencer {
 public:
  struct parent {
    Parent *request;
    unsigned uid;
    uint64_t accepted_tick;
    uint8_t required;
    uint8_t transferred = 0;
  };
  struct sector { Parent *request; unsigned index; };
  explicit finite_read_sequencer(unsigned capacity = 1) : m_capacity(capacity) {
    assert(capacity);
  }
  void set_capacity(unsigned n) { assert(n && empty()); m_capacity = n; }
  void enqueue(Parent *request, unsigned uid, uint8_t mask, uint64_t tick) {
    assert(request && mask && !(mask & ~0xFU));
    assert(m_parents.size() < m_capacity);
    assert(m_uids.insert(uid).second && "duplicate oracle parent");
    m_parents.push_back({request, uid, tick, mask, 0});
    ++parents_enqueued;
    for (unsigned s = 0; s < 4; ++s) required_sectors += !!(mask & (1U << s));
    peak_parents = std::max(peak_parents, m_parents.size());
  }
  bool ready(uint64_t tick) const {
    if (tick == m_last_transfer_tick) return false;
    for (const parent &p : m_parents) if (tick > p.accepted_tick) return true;
    return false;
  }
  sector next(uint64_t tick) {
    assert(ready(tick));
    // Rotation both selects an eligible parent and provides bounded RR fairness.
    while (!(tick > m_parents.front().accepted_tick)) rotate();
    const parent &p = m_parents.front();
    unsigned s = 0;
    while (!(p.required & (1U << s)) || (p.transferred & (1U << s))) ++s;
    assert(s < 4);
    return {p.request, s};
  }
  void commit(uint64_t tick, const sector &sent) {
    assert(ready(tick) && !m_parents.empty());
    parent &p = m_parents.front();
    assert(p.request == sent.request && sent.index < 4);
    const uint8_t bit = static_cast<uint8_t>(1U << sent.index);
    assert((p.required & bit) && !(p.transferred & bit));
    p.transferred |= bit;
    ++sectors_enqueued;
    m_last_transfer_tick = tick;
    if (p.transferred == p.required) {
      assert(m_uids.erase(p.uid) == 1);
      m_parents.pop_front();
      ++parents_transferred;
    } else {
      rotate();
    }
  }
  bool empty() const { return m_parents.empty(); }
  size_t size() const { return m_parents.size(); }
  uint64_t parents_enqueued = 0, required_sectors = 0, sectors_enqueued = 0;
  uint64_t parents_transferred = 0;
  size_t peak_parents = 0;
 private:
  void rotate() { m_parents.push_back(m_parents.front()); m_parents.pop_front(); }
  unsigned m_capacity;
  uint64_t m_last_transfer_tick = UINT64_MAX;
  std::deque<parent> m_parents;
  std::set<unsigned> m_uids;
};
}  // namespace dtc_oracle
#endif
