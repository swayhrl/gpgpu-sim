#ifndef EP_L2_RO_TRANSACTIONS_H
#define EP_L2_RO_TRANSACTIONS_H

#include <stdint.h>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include "gpu-cache.h"
#include "ep-l2-ro-oracle.h"

// Lane 3 independent production candidate. There is deliberately no allocator
// here: the caller atomically reserves a landing through Lane 1's interface.
// Every requester uses the existing mshr_table's descriptor free-list.
namespace ep_l2_ro {

typedef ep_l2::PayloadHandle identity;

struct tag_binding {
  unsigned index;
  uint64_t generation;
  tag_binding(unsigned i = ~0U, uint64_t g = 0) : index(i), generation(g) {}
};

// Adapter obligations: all bank operations arbitrate with normal fills/hits,
// WB and TVD; callbacks cannot allocate free data or change the physical budget.
class payload_port {
 public:
  virtual ~payload_port() {}
  virtual bool live(identity h, new_addr_type line) const = 0;
  virtual bool pin(identity h) = 0;
  virtual void unpin(identity h) = 0;
  virtual bool fill_write(identity h, unsigned sectors, uint64_t cycle) = 0;
  virtual void lower_accepted(identity, unsigned) {}
  virtual bool response_read(identity h, unsigned sectors, uint64_t cycle) = 0;
  virtual bool tag_matches(tag_binding tag, identity h,
                           new_addr_type line) const = 0;
  virtual void tag_ready(tag_binding tag, identity h,
                          new_addr_type line, unsigned ready_sectors,
                          bool all_required_ready) = 0;
  virtual void tag_detach(tag_binding tag, identity h) = 0;
  virtual void release(identity h) = 0;
};

enum request_kind { GLOBAL_DEMAND_READ, SYNTHETIC_WRITE_ALLOCATE_READ,
                    STORE, ATOMIC, WRITEBACK, UNKNOWN_REQUEST };
enum admission { ACCEPTED, NORMAL_PATH, DESCRIPTOR_OR_ADDRESS_FULL,
                 TRANSACTION_SLOT_BUSY, TAG_BINDING_INVALID, LINE_MSHR_FULL };

struct switches {
  bool tag_detach, line_mshr_bypass;
  switches(bool d = false, bool b = false) : tag_detach(d), line_mshr_bypass(b) {}
};

struct response {
  mem_fetch *request;
  unsigned sectors;
  identity transaction;
};

class transactions {
 public:
  struct counters {
    uint64_t admits, merges, detached, lower_sectors, duplicate_sectors;
    uint64_t responses, rejected_oracle, response_backpressure;
    counters() : admits(0), merges(0), detached(0), lower_sectors(0),
      duplicate_sectors(0), responses(0), rejected_oracle(0),
      response_backpressure(0) {}
  } stats;

  transactions(mshr_table &shared, payload_port &port, unsigned payload_budget,
                const oracle_certificate &certificate,
                switches features = switches())
      : m_shared(shared), m_port(port), m_slots(payload_budget),
        m_features(features), m_oracle(certificate), m_epoch(0), m_epoch_open(false) {
    if (shared.descriptor_pool_capacity() != 512 ||
        (payload_budget != 1168 && payload_budget != 1160))
      throw std::invalid_argument("RO requires shared D512 and C16/C8 payload budget");
  }

  // The binding layer must have verified every input and oracle artifact hash.
  // This is a closed-world experimental oracle, not an inferred hardware bit.
  void begin_epoch(uint64_t epoch, const std::string &verified_manifest_sha256) {
    if (m_epoch_open || !drained() ||
        verified_manifest_sha256 != m_oracle.manifest_sha256())
      throw std::logic_error("epoch start requires verified manifest and drain");
    m_epoch = epoch; m_epoch_open = true;
  }
  bool end_epoch() {
    if (!drained()) return false;
    m_epoch_open = false;
    return true;
  }
  bool drained() const {
    for (unsigned i = 0; i < m_slots.size(); ++i)
      if (m_slots[i].live) return false;
    return true;
  }
  unsigned live_transactions() const {
    unsigned n = 0;
    for (unsigned i = 0; i < m_slots.size(); ++i) n += m_slots[i].live;
    return n;
  }

  admission admit(uint64_t epoch, new_addr_type line, identity h,
                  tag_binding tag, mem_fetch *mf, unsigned mask,
                  bool certified_line, request_kind kind,
                  unsigned initial_ready_sectors = 0) {
    check_mask(mask);
    if (initial_ready_sectors & ~15U)
      throw std::invalid_argument("invalid resident ready sectors");
    if (!m_epoch_open || epoch != m_epoch || !certified_line ||
        !m_oracle.contains(line) || kind != GLOBAL_DEMAND_READ) {
      ++stats.rejected_oracle;
      return NORMAL_PATH;
    }
    if (!m_features.line_mshr_bypass && !m_features.tag_detach) return NORMAL_PATH;
    if (line % 128 || h.payload_id >= m_slots.size() || !m_port.live(h, line) ||
        !m_port.tag_matches(tag, h, line)) return TAG_BINDING_INVALID;
    entry &e = m_slots[h.payload_id];
    if (e.live) return TRANSACTION_SLOT_BUSY;
    if (!m_features.line_mshr_bypass && !m_shared.reserve_ro_line(line))
      return LINE_MSHR_FULL;
    mshr_table::ro_descriptor_handle d;
    if (!m_shared.reserve_ro_descriptor(line, mf, sector_mask(mask), d)) {
      if (!m_features.line_mshr_bypass) m_shared.release_ro_line(line);
      return DESCRIPTOR_OR_ADDRESS_FULL;
    }
    if (!m_port.pin(h)) {
      m_shared.release_ro_descriptor(d);
      if (!m_features.line_mshr_bypass) m_shared.release_ro_line(line);
      return TRANSACTION_SLOT_BUSY;
    }
    e = entry(); e.live = e.attached = true; e.h = h; e.tag = tag;
    e.line = line; e.epoch = epoch; e.required = mask;
    e.ready = initial_ready_sectors;
    e.waiters.push_back(waiter(d, mf, mask));
    if(!m_features.line_mshr_bypass)m_shared.bind_ro_line(line,h);
    ++stats.admits;
    return ACCEPTED;
  }

  // Tag lookup supplies the attached match. Only detach-with-MSHR mode may
  // discover a detached transaction by the conventional line index.
  bool merge_target(new_addr_type line, identity &result) const {
    return !m_features.line_mshr_bypass && m_shared.lookup_ro_line(line,result);
  }
  bool live(identity h) const {
    return h.payload_id<m_slots.size() && m_slots[h.payload_id].live && m_slots[h.payload_id].h==h;
  }
  unsigned scheduled(identity h) const { const entry &e=get(h);return e.required|e.ready; }
  bool attached(identity h) const { return get(h).attached; }
  bool ready_response(identity &h, unsigned start=0) const {
    for(unsigned n=0;n<m_slots.size();++n) {
      const entry &e=m_slots[(n+start)%m_slots.size()];
      if(!e.live)continue;
      for(const auto &w:e.waiters)if(!(w.sectors&~e.ready)){h=e.h;return true;}
    }
    return false;
  }

  admission merge(identity h, mem_fetch *mf, unsigned mask,
                  uint64_t epoch, bool certified_line, request_kind kind) {
    check_mask(mask);
    entry &e = get(h);
    if (epoch != e.epoch || !certified_line || !m_oracle.contains(e.line) ||
        kind != GLOBAL_DEMAND_READ) {
      ++stats.rejected_oracle;
      return NORMAL_PATH;
    }
    if ((e.attached && !m_port.tag_matches(e.tag, h, e.line)) ||
        (!e.attached && m_features.line_mshr_bypass))
      return NORMAL_PATH;
    mshr_table::ro_descriptor_handle d;
    if (!m_shared.reserve_ro_descriptor(e.line, mf, sector_mask(mask), d))
      return DESCRIPTOR_OR_ADDRESS_FULL;
    e.waiters.push_back(waiter(d, mf, mask));
    e.required |= mask; e.reuse_seen = true;
    ++stats.merges;
    return ACCEPTED;
  }

  bool can_detach(identity h) const {
    const entry &e=get(h);
    return m_features.tag_detach && e.attached && !e.reuse_seen && (e.required&~e.ready);
  }
  bool detach(identity h) {
    entry &e = get(h);
    // Conservative waiter-aware protection persists after a merged waiter
    // retires. No future reuse oracle is consumed by this policy.
    if (!m_features.tag_detach || !e.attached || e.reuse_seen ||
        !(e.required & ~e.ready)) return false;
    if (!m_port.tag_matches(e.tag, h, e.line))
      throw std::logic_error("tag reuse without detach handshake");
    m_port.tag_detach(e.tag, h); e.attached = false;
    ++stats.detached;
    return true;
  }

  unsigned lower_needed(identity h) const {
    const entry &e = get(h);
    return e.required & ~(e.issued | e.ready);
  }
  // Called only after real lower-queue acceptance; no resource is freed here.
  void lower_accepted(identity h, unsigned mask) {
    check_mask(mask);
    entry &e = get(h);
    if ((mask & ~e.required) || (mask & (e.issued | e.ready)))
      throw std::logic_error("unrequested or repeated sector issue");
    unsigned duplicates = 0;
    for (unsigned i = 0; i < m_slots.size(); ++i)
      if (m_slots[i].live && !(m_slots[i].h == h) &&
          m_slots[i].line == e.line) duplicates |= m_slots[i].issued & mask;
    stats.duplicate_sectors += bits(duplicates);
    stats.lower_sectors += bits(mask);
    m_port.lower_accepted(h, mask);
    e.issued |= mask;
    if (!m_features.line_mshr_bypass)
      m_shared.ro_line_issue(e.line, sector_mask(mask));
  }
  bool fill(identity h, unsigned mask, uint64_t cycle) {
    check_mask(mask);
    entry &e = get(h);
    if ((mask & ~e.issued) || (mask & e.ready))
      throw std::logic_error("unexpected or repeated fill");
    if (e.attached && !m_port.tag_matches(e.tag, h, e.line))
      throw std::logic_error("stale attached fill tag generation");
    if (!m_port.fill_write(h, mask, cycle)) return false;
    e.ready |= mask;
    if (!m_features.line_mshr_bypass)
      m_shared.ro_line_ready(e.line, sector_mask(mask));
    if (e.attached) {
      m_port.tag_ready(e.tag, h, e.line, e.ready,
                       (e.required & ~e.ready) == 0);
    }
    retire_if_done(e);
    return true;
  }
  // A read grant is cycle-local: a rejected enqueue must reread on retry.
  // No hidden per-transaction response data buffer is provided.
  bool prepare_response(identity h, uint64_t cycle, response &out) {
    entry &e = get(h);
    if (e.prepared && e.prepared_cycle != cycle) e.prepared = false;
    if (!e.prepared) {
      for (unsigned i = 0; i < e.waiters.size(); ++i) {
        if ((e.waiters[i].sectors & ~e.ready) == 0) {
          if (!m_port.response_read(h, e.waiters[i].sectors, cycle)) return false;
          e.prepared = true; e.prepared_index = i; e.prepared_cycle = cycle;
          break;
        }
      }
    }
    if (!e.prepared) return false;
    waiter &w = e.waiters[e.prepared_index];
    out.request = w.mf; out.sectors = w.sectors; out.transaction = h;
    return true;
  }
  void commit_response(identity h, bool fifo_accepted, uint64_t cycle) {
    entry &e = get(h);
    if (!e.prepared) throw std::logic_error("response commit without read");
    if (fifo_accepted && e.prepared_cycle != cycle)
      throw std::logic_error("response enqueue outside granted read cycle");
    if (!fifo_accepted) {
      ++stats.response_backpressure; e.prepared = false; return;
    }
    m_shared.release_ro_descriptor(e.waiters[e.prepared_index].descriptor);
    e.waiters.erase(e.waiters.begin() + e.prepared_index);
    e.prepared = false; ++stats.responses;
    retire_if_done(e);
  }
  unsigned pending(identity h) const {
    const entry &e = get(h); return e.issued & ~e.ready;
  }
  void assert_no_epoch_write(uint64_t epoch, new_addr_type line) const {
    if (epoch == m_epoch && m_epoch_open && m_oracle.contains(line))
      throw std::logic_error("oracle violation: writer to certified epoch line");
  }

 private:
  struct waiter {
    mshr_table::ro_descriptor_handle descriptor;
    mem_fetch *mf;
    unsigned sectors;
    waiter(mshr_table::ro_descriptor_handle d, mem_fetch *m, unsigned s)
      : descriptor(d), mf(m), sectors(s) {}
  };
  struct entry {
    bool live, attached, reuse_seen, prepared;
    unsigned required, issued, ready, prepared_index;
    identity h; tag_binding tag; new_addr_type line; uint64_t epoch, prepared_cycle;
    std::vector<waiter> waiters;
    entry() : live(false), attached(false), reuse_seen(false), prepared(false),
      required(0), issued(0), ready(0), prepared_index(0), line(0), epoch(0), prepared_cycle(0) {}
  };
  static void check_mask(unsigned mask) {
    if (!mask || (mask & ~15U)) throw std::invalid_argument("invalid sector mask");
  }
  static unsigned bits(unsigned mask) {
    return (mask & 1) + ((mask >> 1) & 1) + ((mask >> 2) & 1) + ((mask >> 3) & 1);
  }
  static mem_access_sector_mask_t sector_mask(unsigned mask) {
    return mem_access_sector_mask_t(mask);
  }
  entry &get(identity h) {
    if (h.payload_id >= m_slots.size() || !m_slots[h.payload_id].live ||
        !(m_slots[h.payload_id].h == h) || !m_port.live(h, m_slots[h.payload_id].line))
      throw std::logic_error("stale payload transaction generation");
    return m_slots[h.payload_id];
  }
  const entry &get(identity h) const {
    return const_cast<transactions *>(this)->get(h);
  }
  void retire_if_done(entry &e) {
    if (!e.waiters.empty() || e.prepared || (e.required & ~e.ready) ||
        (e.issued & ~e.ready)) return;
    m_port.unpin(e.h);
    if (!e.attached) m_port.release(e.h);
    if (!m_features.line_mshr_bypass) m_shared.release_ro_line(e.line);
    e.live = false;
  }
  mshr_table &m_shared;
  payload_port &m_port;
  std::vector<entry> m_slots;  // fixed bound: one completion per physical slot
  switches m_features;
  const oracle_certificate &m_oracle;
  uint64_t m_epoch;
  bool m_epoch_open;
};
}  // namespace ep_l2_ro
#endif
