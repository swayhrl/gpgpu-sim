// Metadata-only TVD policy. Instantiate Handle with the public payload handle;
// this module owns neither a payload allocator nor a second WAD address table.
#ifndef EP_L2_TVD_DIRECTORY_H
#define EP_L2_TVD_DIRECTORY_H

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include "ep-l2-ft-interface.h"

template <class Handle = ep_l2::PayloadHandle> class ep_l2_tvd_directory {
 public:
  typedef std::uint64_t address_type;
  typedef std::uint64_t cycle_type;
  enum state { EMPTY, CLEAN, WB_STAGED, WB_INFLIGHT, WB_COMPLETE };
  enum operation { READ_OP, WRITE_OP, ATOMIC_OP };
  enum result {
    DISABLED, ABSENT, LOOKUP_DELAY, READ_READY, PROMOTE_REQUIRED,
    WAIT_WRITEBACK, MISSING_SECTOR, PINNED, DIRECTORY_FULL,
    INVALID_INPUT, STALE, DATA_TRANSFER_UNPROVEN, OK
  };
  struct ticket {
    unsigned index;
    std::uint64_t incarnation;
    ticket(unsigned i = 128, std::uint64_t g = 0) : index(i), incarnation(g) {}
  };
  struct entry {
    state status;
    address_type line;
    Handle payload;
    unsigned valid_sectors, dirty_sectors, readers;
    std::uint64_t incarnation, age;
    cycle_type born, lower_accepted;
    entry() : status(EMPTY), line(0), valid_sectors(0), dirty_sectors(0),
              readers(0), incarnation(0), age(0), born(0), lower_accepted(0) {}
    bool mandatory() const { return status == WB_STAGED || status == WB_INFLIGHT; }
    bool reclaimable() const {
      return (status == CLEAN || status == WB_COMPLETE) && readers == 0;
    }
  };
  struct install_plan {
    result status;
    unsigned index;
    std::uint64_t revision;
    Handle displaced;
    address_type line;
    bool writeback;
    install_plan() : status(DIRECTORY_FULL), index(128), revision(0), line(0),
                     writeback(false) {}
  };
  struct promotion_plan {
    result status;
    ticket source;
    std::uint64_t revision;
    address_type victim_line;
    Handle victim;
    unsigned valid, dirty;
    bool retain_payload;
    promotion_plan() : status(INVALID_INPUT), revision(0), victim_line(0),
                       valid(0), dirty(0), retain_payload(false) {}
  };
  struct observation {
    result status;
    ticket identity;
    Handle payload;
    bool dirty_forward;
    observation(result r = ABSENT) : status(r), dirty_forward(false) {}
  };
  // A lease lives in an existing request/descriptor, not a new request pool.
  // Move-only: response enqueue consumes it exactly once. A live lease must
  // be retired/cancelled before its owner is destroyed.
  class read_lease {
   public:
    read_lease() : owner(NULL), sectors(0), forward(false), active(false) {}
    read_lease(const read_lease &) = delete;
    read_lease &operator=(const read_lease &) = delete;
    read_lease(read_lease &&other) : owner(other.owner), identity(other.identity),
        payload(other.payload), sectors(other.sectors), forward(other.forward),
        active(other.active) { other.active = false; }
    ~read_lease() { assert(!active); }
    bool live() const { return active; }
    const Handle &handle() const { return payload; }
   private:
    friend class ep_l2_tvd_directory;
    ep_l2_tvd_directory *owner;
    ticket identity;
    Handle payload;
    unsigned sectors;
    bool forward, active;
  };
  struct counters {
    std::uint64_t clean_installs, dirty_installs, reclaimed, skipped_clean;
    std::uint64_t read_responses, dirty_forward_responses, promotions;
    std::uint64_t served_sectors, served_sector_bytes;
    // DEPRECATED compatibility aliases: these are SERVED bytes, NOT DRAM savings.
    std::uint64_t avoided_lower_sectors, avoided_lower_bytes;
    std::uint64_t cancelled_reads, lower_accepts, wb_completions;
    std::uint64_t wb_staging_slot_cycles, tvd_extra_retention_slot_cycles;
    counters() : clean_installs(0), dirty_installs(0), reclaimed(0),
        skipped_clean(0), read_responses(0), dirty_forward_responses(0),
        promotions(0), served_sectors(0), served_sector_bytes(0), avoided_lower_sectors(0), avoided_lower_bytes(0),
        cancelled_reads(0), lower_accepts(0), wb_completions(0),
        wb_staging_slot_cycles(0), tvd_extra_retention_slot_cycles(0) {}
  };

  explicit ep_l2_tvd_directory(unsigned capacity = 128, bool clean = false,
                               bool dirty = false)
      : m_capacity(capacity), m_clean(clean), m_dirty(dirty), m_revision(0),
        m_clock(0), m_age(0) {
    if (!capacity || capacity > 128) throw std::invalid_argument("WAD/TVD capacity");
  }
  ep_l2_tvd_directory(const ep_l2_tvd_directory &) = delete;
  ep_l2_tvd_directory &operator=(const ep_l2_tvd_directory &) = delete;
  unsigned capacity() const { return m_capacity; }
  unsigned occupied() const {
    unsigned n = 0;
    for (unsigned i = 0; i < m_capacity; ++i) n += m_entries[i].status != EMPTY;
    return n;
  }
  unsigned mandatory() const {
    unsigned n = 0;
    for (unsigned i = 0; i < m_capacity; ++i) n += m_entries[i].mandatory();
    return n;
  }
  const entry &at(unsigned i) const { assert(i < m_capacity); return m_entries[i]; }
  const counters &stats() const { return m_stats; }
  ticket find(address_type line) const {
    for (unsigned i = 0; i < m_capacity; ++i)
      if (m_entries[i].status != EMPTY && m_entries[i].line == line)
        return ticket(i, m_entries[i].incarnation);
    return ticket();
  }
  bool live(ticket t) const {
    return t.index < m_capacity && m_entries[t.index].status != EMPTY &&
           m_entries[t.index].incarnation == t.incarnation;
  }
  // Non-mutating preview. Mandatory WB may reclaim clean/WB-complete entries;
  // opportunistic clean retention never preempts pending/inflight WB.
  install_plan preview_install(address_type line, bool dirty) const {
    install_plan p; p.revision = m_revision; p.line = line; p.writeback = dirty;
    if ((line & 127) || live(find(line))) { p.status = INVALID_INPUT; return p; }
    if (!dirty && !m_clean) { p.status = DISABLED; return p; }
    for (unsigned i = 0; i < m_capacity; ++i)
      if (m_entries[i].status == EMPTY) {
        p.status = OK; p.index = i; return p;
      }
    for (unsigned i = 0; i < m_capacity; ++i) {
      if (!m_entries[i].reclaimable()) continue;
      if (p.index == 128 ||
          (dirty && m_entries[i].status == CLEAN && m_entries[p.index].status != CLEAN) ||
          ((!dirty || m_entries[i].status == m_entries[p.index].status) &&
           m_entries[i].age < m_entries[p.index].age))
        p.index = i;
    }
    if (p.index != 128) { p.status = OK; p.displaced = m_entries[p.index].payload; }
    return p;
  }
  // Caller pre-reserves landing/control/lower credit and releases displaced
  // payload only when this commit succeeds. On failure nothing changes.
  result commit_install(const install_plan &p, address_type line, Handle handle,
                        unsigned valid, unsigned dirty, unsigned pending,
                        bool writeback, cycle_type now, ticket &installed) {
    if (p.status != OK) return p.status;
    if (p.revision != m_revision || p.index >= m_capacity) return STALE;
    if (p.line != line || p.writeback != writeback) return INVALID_INPUT;
    if ((line & 127) || live(find(line)) || (valid & ~15u) ||
        (dirty & ~valid) || pending || (!writeback && (!valid || dirty)) ||
        (!writeback && !handle.valid()) || (writeback && !dirty))
      return INVALID_INPUT;
    if ((!writeback && !m_clean) || (writeback && handle.valid() && !m_dirty))
      return DISABLED;
    for (unsigned i = 0; i < m_capacity; ++i)
      if (m_entries[i].status != EMPTY && handle.valid() &&
          same(m_entries[i].payload, handle)) return INVALID_INPUT;
    advance(now);
    entry &e = m_entries[p.index];
    assert(e.status == EMPTY || e.reclaimable());
    if (e.status != EMPTY) ++m_stats.reclaimed;
    bump(e.incarnation);
    e.status = writeback ? WB_STAGED : CLEAN;
    e.line = line; e.payload = handle;
    e.valid_sectors = valid; e.dirty_sectors = dirty; e.readers = 0;
    e.age = ++m_age; e.born = now; e.lower_accepted = 0;
    installed = ticket(p.index, e.incarnation);
    ++m_revision;
    if (writeback) ++m_stats.dirty_installs; else ++m_stats.clean_installs;
    return OK;
  }
  observation preview_access(address_type line, unsigned sectors, operation op,
                             cycle_type tag_miss, cycle_type now) const {
    if (!m_clean && !m_dirty) return observation(DISABLED);
    if (!sectors || (sectors & ~15u) || (line & 127)) return observation(INVALID_INPUT);
    // Overflow-safe exact extra one-cycle serialized directory lookup.
    if (now <= tag_miss) return observation(LOOKUP_DELAY);
    ticket t = find(line);
    if (!live(t)) return observation(ABSENT);
    const entry &e = m_entries[t.index];
    observation o; o.identity = t; o.payload = e.payload;
    if (op != READ_OP && e.mandatory()) { o.status = WAIT_WRITEBACK; return o; }
    if (!e.payload.valid()) { o.status = e.mandatory() ? WAIT_WRITEBACK : ABSENT; return o; }
    if (op != READ_OP) {
      o.status = e.readers ? PINNED : PROMOTE_REQUIRED; return o;
    }
    if ((sectors & ~e.valid_sectors) != 0) {
      o.status = e.mandatory() ? WAIT_WRITEBACK : MISSING_SECTOR; return o;
    }
    o.dirty_forward = e.mandatory();
    o.status = READ_READY;
    return o;
  }
  result begin_read(const observation &o, unsigned sectors, read_lease &lease) {
    if (lease.active) return INVALID_INPUT;
    if (o.status != READ_READY || !live(o.identity)) return STALE;
    entry &e = m_entries[o.identity.index];
    if (!sectors || (sectors & ~e.valid_sectors) ||
        !same(e.payload, o.payload)) return STALE;
    if (e.readers == std::numeric_limits<unsigned>::max()) return DIRECTORY_FULL;
    ++e.readers; ++m_revision;
    lease.owner = this; lease.identity = o.identity; lease.payload = o.payload;
    lease.sectors = sectors; lease.forward = e.mandatory(); lease.active = true;
    return OK;
  }
  // Invoke ONLY after real payload bank service and successful response enqueue.
  result response_commit(read_lease &lease, cycle_type now) {
    if (!lease.active || lease.owner != this || !live(lease.identity)) return STALE;
    advance(now); entry &e = m_entries[lease.identity.index];
    assert(e.readers && same(e.payload, lease.payload));
    --e.readers; e.age = ++m_age; lease.active = false; ++m_revision;
    ++m_stats.read_responses;
    if (lease.forward) ++m_stats.dirty_forward_responses;
    unsigned count = 0;
    for (unsigned mask = lease.sectors; mask; mask >>= 1) count += mask & 1;
    m_stats.served_sectors += count; m_stats.served_sector_bytes += 32 * count;
    m_stats.avoided_lower_sectors = m_stats.served_sectors;
    m_stats.avoided_lower_bytes = m_stats.served_sector_bytes;
    return OK;
  }
  result cancel_read(read_lease &lease) {
    if (!lease.active || lease.owner != this || !live(lease.identity)) return STALE;
    entry &e = m_entries[lease.identity.index]; assert(e.readers);
    --e.readers; lease.active = false; ++m_revision; ++m_stats.cancelled_reads;
    return OK;
  }
  // Caller atomically installs the resident tag using this SAME handle;
  // prepare all promotion victim/WB resources before calling. No data copy.
  result promote(ticket t, Handle &transferred, cycle_type now) {
    if (!live(t)) return STALE;
    entry &e = m_entries[t.index];
    if (e.mandatory()) return WAIT_WRITEBACK;
    if (e.readers) return PINNED;
    advance(now); transferred = e.payload; erase(e); ++m_stats.promotions;
    return OK;
  }
  // Promotion can displace a dirty resident while all 128 address slots are
  // occupied. Reuse the PROMOTED entry's address slot in one commit, rather
  // than requiring a transient 129th slot or losing mandatory WAD state.
  // Caller must pre-reserve the real WB/lower resources without mutation.
  promotion_plan preview_dirty_promotion(ticket source, address_type victim_line,
                                         Handle victim, unsigned valid,
                                         unsigned dirty, unsigned pending, bool retain = true) const {
    promotion_plan p;
    if (!m_clean && !m_dirty) { p.status = DISABLED; return p; }
    if (!live(source)) { p.status = STALE; return p; }
    const entry &e = m_entries[source.index];
    if (e.mandatory()) { p.status = WAIT_WRITEBACK; return p; }
    if (e.readers) { p.status = PINNED; return p; }
    if ((victim_line & 127) || !victim.valid() || !dirty || (valid & ~15u) ||
        (dirty & ~valid) || pending || live(find(victim_line))) return p;
    for (unsigned i = 0; i < m_capacity; ++i)
      if (m_entries[i].status != EMPTY && same(m_entries[i].payload, victim)) return p;
    p.status = OK; p.source = source; p.revision = m_revision;
    p.victim_line = victim_line; p.victim = victim; p.valid = valid; p.dirty = dirty;
    p.retain_payload = m_dirty && retain;
    return p;
  }
  result commit_dirty_promotion(const promotion_plan &p, cycle_type now,
                                Handle &promoted, ticket &writeback) {
    if (p.status != OK) return p.status;
    if (p.revision != m_revision || !live(p.source)) return STALE;
    entry &e = m_entries[p.source.index];
    assert(!e.mandatory() && !e.readers);
    advance(now); promoted = e.payload;
    bump(e.incarnation); e.status = WB_STAGED; e.line = p.victim_line;
    e.payload = p.retain_payload ? p.victim : Handle(); e.valid_sectors = p.valid; e.dirty_sectors = p.dirty;
    e.age = ++m_age; e.born = now; e.lower_accepted = 0;
    writeback = ticket(p.source.index, e.incarnation);
    ++m_revision; ++m_stats.promotions; ++m_stats.dirty_installs;
    return OK;
  }
  // Clean missing-sector fallback/reclaim drops the old directory visibility
  // before an ordinary miss is issued. Dirty entries must first complete WB.
  result reclaim(ticket t, Handle &released, cycle_type now) {
    if (!live(t)) return STALE;
    entry &e = m_entries[t.index];
    if (e.mandatory()) return WAIT_WRITEBACK;
    if (e.readers) return PINNED;
    advance(now); released = e.payload; erase(e); ++m_stats.reclaimed;
    return OK;
  }
  result rollback_staged(ticket t, cycle_type now) {
    if (!live(t) || m_entries[t.index].status != WB_STAGED) return STALE;
    if (m_entries[t.index].readers) return PINNED;
    advance(now); erase(m_entries[t.index]); return OK;
  }
  result lower_accept(ticket t, bool lower_has_independent_data, cycle_type now) {
    if (!live(t) || m_entries[t.index].status != WB_STAGED) return STALE;
    if (!lower_has_independent_data) return DATA_TRANSFER_UNPROVEN;
    advance(now); entry &e = m_entries[t.index];
    e.status = WB_INFLIGHT; e.lower_accepted = now;
    ++m_revision; ++m_stats.lower_accepts; return OK;
  }
  result set_done(ticket t, cycle_type now) {
    if (!live(t) || m_entries[t.index].status != WB_INFLIGHT) return STALE;
    advance(now); entry &e = m_entries[t.index];
    if (e.payload.valid()) {
      e.status = WB_COMPLETE; e.dirty_sectors = 0; e.age = ++m_age; ++m_revision;
    } else { assert(!e.readers); erase(e); }
    ++m_stats.wb_completions; return OK;
  }
  // Epoch cleanup refuses to discard mandatory WB or an uncommitted response.
  // Reclaim each clean handle through the allocator before declaring drained.
  bool epoch_ready() const {
    for (unsigned i = 0; i < m_capacity; ++i)
      if (m_entries[i].mandatory() || m_entries[i].readers) return false;
    return true;
  }
  void advance(cycle_type now) {
    if (now < m_clock) throw std::logic_error("nonmonotonic TVD cycle");
    const cycle_type delta = now - m_clock;
    for (unsigned i = 0; i < m_capacity; ++i) {
      const entry &e = m_entries[i];
      if (!e.payload.valid()) continue;
      if (e.status == WB_STAGED) m_stats.wb_staging_slot_cycles += delta;
      if (e.status == WB_INFLIGHT || e.status == WB_COMPLETE)
        m_stats.tvd_extra_retention_slot_cycles += delta;
    }
    m_clock = now;
  }

 private:
  static bool same(Handle a, Handle b) {
    return a.payload_id == b.payload_id && a.generation == b.generation;
  }
  static void bump(std::uint64_t &v) {
    if (v == std::numeric_limits<std::uint64_t>::max())
      throw std::overflow_error("TVD generation exhausted: drain epoch");
    ++v;
  }
  void erase(entry &e) {
    assert(!e.readers); bump(e.incarnation); e.status = EMPTY;
    e.payload = Handle(); e.valid_sectors = e.dirty_sectors = 0; ++m_revision;
  }
  unsigned m_capacity;
  bool m_clean, m_dirty;
  std::uint64_t m_revision;
  cycle_type m_clock;
  std::uint64_t m_age;
  std::array<entry, 128> m_entries;
  counters m_stats;
};
#endif
