#include "vm_translation.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include <fstream>
#include <sstream>

namespace vm_translation {

const char *object_class_name(object_class object) {
  switch (object) {
    case OBJECT_WEIGHT:
      return "WEIGHT";
    case OBJECT_KV_CACHE:
      return "KV_CACHE";
    case OBJECT_UNKNOWN:
      return "UNKNOWN";
    default:
      assert(false && "invalid VM object class");
      return "UNKNOWN";
  }
}

namespace {

std::vector<std::string> split_tab_fields(const std::string &line) {
  std::vector<std::string> fields;
  std::stringstream stream(line);
  std::string field;
  while (std::getline(stream, field, '\t')) fields.push_back(field);
  return fields;
}

bool parse_u64(const std::string &text, uint64_t *value) {
  if (text.empty() || value == 0) return false;
  errno = 0;
  char *end = 0;
  const unsigned long long parsed = strtoull(text.c_str(), &end, 0);
  if (errno != 0 || end == text.c_str() || *end != '\0') return false;
  *value = static_cast<uint64_t>(parsed);
  return true;
}

object_class parse_object_class(const std::string &text) {
  if (text == "WEIGHT") return OBJECT_WEIGHT;
  if (text == "KV_CACHE") return OBJECT_KV_CACHE;
  assert(false && "object map contains an unsupported object class");
  return OBJECT_UNKNOWN;
}

}  // namespace

object_range_map::object_range_map(const std::string &path)
    : m_enabled(false), m_ranges() {
  if (path.empty()) return;
  std::ifstream input(path.c_str());
  assert(input.good() && "unable to open frozen VM object range map");
  std::string line;
  assert(std::getline(input, line));
  assert(line == "M4C_OBJECT_MAP_V1" &&
         "unsupported frozen VM object range map schema");
  bool saw_roi = false;
  bool saw_source_sha = false;
  bool saw_archive_sha = false;
  bool saw_sidecar_sha = false;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') continue;
    const std::vector<std::string> fields = split_tab_fields(line);
    assert(!fields.empty());
    if (fields[0] == "roi") {
      assert(fields.size() == 2 && !fields[1].empty());
      saw_roi = true;
    } else if (fields[0] == "source_sha256") {
      assert(fields.size() == 2 && fields[1].size() == 64);
      saw_source_sha = true;
    } else if (fields[0] == "archive_sha256") {
      assert(fields.size() == 2 && fields[1].size() == 64);
      saw_archive_sha = true;
    } else if (fields[0] == "sidecar_sha256") {
      assert(fields.size() == 2 && fields[1].size() == 64);
      saw_sidecar_sha = true;
    } else {
      assert(fields[0] == "range" && fields.size() == 4);
      uint64_t start = 0;
      uint64_t end = 0;
      assert(parse_u64(fields[2], &start) && parse_u64(fields[3], &end));
      assert(start <= end);
      const object_class object = parse_object_class(fields[1]);
      if (!m_ranges.empty()) {
        // The producer must globally sort and make ranges disjoint.  Rejecting
        // a malformed map is preferable to silently changing provenance.
        assert(m_ranges.back().start < start && m_ranges.back().end < start);
      }
      m_ranges.push_back(range(start, end, object));
    }
  }
  assert(saw_roi && saw_source_sha && saw_archive_sha && saw_sidecar_sha &&
         !m_ranges.empty());
  m_enabled = true;
}

object_class object_range_map::classify(uint64_t start, uint64_t bytes) const {
  if (!m_enabled) return OBJECT_UNKNOWN;
  assert(bytes != 0 && start <= ~0ULL - (bytes - 1));
  const uint64_t end = start + bytes - 1;
  unsigned intersected_ranges = 0;
  for (unsigned index = 0; index < m_ranges.size(); ++index) {
    const range &candidate = m_ranges[index];
    if (candidate.start > end) break;
    if (candidate.end < start) continue;
    ++intersected_ranges;
    // A request fully inside one recorded range has an unambiguous class.
    if (start >= candidate.start && end <= candidate.end) {
      assert(intersected_ranges == 1);
      return candidate.object;
    }
  }
  // A transaction spanning two known ranges is an object-attribution
  // ambiguity, not a permissible unknown access.
  assert(intersected_ranges <= 1 &&
         "VM request overlaps multiple object ranges");
  return OBJECT_UNKNOWN;
}

namespace {

const uint64_t kPteBytes = 8;
const uint64_t kBasePage64KB = 64ULL * 1024ULL;
const uint64_t kLargePage2MB = 2ULL * 1024ULL * 1024ULL;
const unsigned kMaximumGenericVirtualAddressBits = 56;

unsigned log2_exact(uint64_t value) {
  assert(value != 0 && (value & (value - 1)) == 0);
  unsigned result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

bool add_no_overflow(uint64_t left, uint64_t right, uint64_t *result) {
  if (left > ~0ULL - right) return false;
  *result = left + right;
  return true;
}

// The generic hierarchy is deliberately balanced by VPN bits, not by an
// NVIDIA-specific page-table format: r=ceil(B/L), then [top,r,...,r].
bool hierarchy_namespace_bytes(unsigned levels, unsigned va_bits,
                               uint64_t *required_bytes) {
  const uint64_t page_sizes[] = {kBasePage64KB, kLargePage2MB};
  uint64_t total = 0;
  for (unsigned page_class = 0; page_class < 2; ++page_class) {
    const unsigned vpn_bits = va_bits - log2_exact(page_sizes[page_class]);
    if (levels == 0 || levels > vpn_bits) return false;
    const unsigned radix_bits = (vpn_bits + levels - 1) / levels;
    const unsigned top_bits = vpn_bits - radix_bits * (levels - 1);
    if (top_bits == 0) return false;
    for (unsigned level = 0; level < levels; ++level) {
      const unsigned prefix_bits = top_bits + level * radix_bits;
      // The generic VA cap keeps this below 64, but retain the explicit
      // bound so a future wider configuration cannot silently overflow.
      if (prefix_bits > 60) return false;
      const uint64_t bytes = 1ULL << (prefix_bits + 3);
      if (!add_no_overflow(total, bytes, &total)) return false;
    }
  }
  *required_bytes = total;
  return true;
}

}  // namespace

uint64_t present_page_mapper::translate(const translation_key &key) const {
  assert(vm_core::valid_page_size(key.page_size));
  return key.vpn;  // distinct resident VPNs retain distinct PPNs.
}

bool page_table_config::valid() const {
  const unsigned base_page_bits = log2_exact(kBasePage64KB);
  const unsigned large_page_bits = log2_exact(kLargePage2MB);
  if (levels == 0 || virtual_address_bits <= large_page_bits ||
      virtual_address_bits > kMaximumGenericVirtualAddressBits ||
      application_physical_limit < (1ULL << virtual_address_bits) ||
      application_physical_limit > pte_physical_base ||
      (pte_physical_base % kBasePage64KB) != 0 ||
      pte_physical_bytes == 0 ||
      pte_physical_base > ~0ULL - pte_physical_bytes)
    return false;

  (void)base_page_bits;
  // Prefix namespaces are sized exactly by their radix level.  The current
  // 56-bit generic reservation remains 2^46 bytes (larger than required),
  // but validity must not depend on the old flat full-VPN encoding.
  uint64_t required_bytes = 0;
  if (!hierarchy_namespace_bytes(levels, virtual_address_bits,
                                 &required_bytes))
    return false;
  return pte_physical_bytes >= required_bytes;
}

radix_page_table_backend::radix_page_table_backend(
    const page_table_config &config)
    : m_config(config) {
  assert(valid());
}

bool radix_page_table_backend::valid() const { return m_config.valid(); }

unsigned radix_page_table_backend::page_size_class(uint64_t page_size) const {
  if (page_size == kBasePage64KB) return 0;
  if (page_size == kLargePage2MB) return 1;
  assert(false && "M3 generic PTE backend supports 64KB and 2MB only");
  return 0;
}

unsigned radix_page_table_backend::vpn_bits(uint64_t page_size) const {
  assert(vm_core::valid_page_size(page_size));
  const unsigned shift = log2_exact(page_size);
  assert(shift < m_config.virtual_address_bits);
  return m_config.virtual_address_bits - shift;
}

unsigned radix_page_table_backend::level_width(uint64_t page_size,
                                                unsigned level) const {
  assert(level < m_config.levels);
  const unsigned bits = vpn_bits(page_size);
  const unsigned radix_bits = (bits + m_config.levels - 1) / m_config.levels;
  if (level == 0) return bits - radix_bits * (m_config.levels - 1);
  return radix_bits;
}

unsigned radix_page_table_backend::prefix_bits_for_level(
    uint64_t page_size, unsigned level) const {
  assert(level < m_config.levels);
  unsigned result = 0;
  for (unsigned index = 0; index <= level; ++index)
    result += level_width(page_size, index);
  return result;
}

uint64_t radix_page_table_backend::vpn_prefix(const translation_key &key,
                                               unsigned level) const {
  assert(supports_key(key));
  const unsigned bits = vpn_bits(key.page_size);
  const unsigned prefix_bits = prefix_bits_for_level(key.page_size, level);
  assert(prefix_bits <= bits);
  return key.vpn >> (bits - prefix_bits);
}

uint64_t radix_page_table_backend::pte_namespace_offset(
    uint64_t page_size, unsigned level) const {
  assert(level < m_config.levels);
  const unsigned target_class = page_size_class(page_size);
  const uint64_t page_sizes[] = {kBasePage64KB, kLargePage2MB};
  uint64_t offset = 0;
  for (unsigned page_class = 0; page_class <= target_class; ++page_class) {
    const unsigned final_level =
        page_class == target_class ? level : m_config.levels;
    for (unsigned index = 0; index < final_level; ++index) {
      const unsigned prefix_bits =
          prefix_bits_for_level(page_sizes[page_class], index);
      const uint64_t bytes = 1ULL << (prefix_bits + 3);
      assert(add_no_overflow(offset, bytes, &offset));
    }
  }
  return offset;
}

bool radix_page_table_backend::pte_prefix_identity(
    const translation_key &key, unsigned level, unsigned *page_class,
    uint64_t *prefix) const {
  if (!supports_key(key) || level >= m_config.levels || page_class == 0 ||
      prefix == 0)
    return false;
  *page_class = page_size_class(key.page_size);
  *prefix = vpn_prefix(key, level);
  return true;
}

bool radix_page_table_backend::supports_key(const translation_key &key) const {
  if (!valid() || (key.page_size != kBasePage64KB &&
                   key.page_size != kLargePage2MB))
    return false;
  const unsigned key_vpn_bits = vpn_bits(key.page_size);
  return key.vpn < (1ULL << key_vpn_bits);
}

uint64_t radix_page_table_backend::pte_address(const translation_key &key,
                                                unsigned level) const {
  assert(valid());
  assert(level < m_config.levels);
  assert(supports_key(key));
  // Physical PTE identities use the radix prefix at each level, never the
  // full VPN.  This gives upper-level PTE locality exactly where two VPNs
  // share their hierarchy prefix, while the leaf remains unique.
  const uint64_t offset =
      pte_namespace_offset(key.page_size, level) +
      vpn_prefix(key, level) * kPteBytes;
  assert(offset <= m_config.pte_physical_bytes - kPteBytes);
  const uint64_t result = m_config.pte_physical_base + offset;
  assert(owns_pte_physical_address(result));
  return result;
}

uint64_t radix_page_table_backend::resolve_ppn(
    const translation_key &key) const {
  assert(vm_core::valid_page_size(key.page_size));
  return key.vpn;  // Preserve the frozen identity-like data mapping.
}

pte_request radix_page_table_backend::make_pte_request(
    const translation_key &key, unsigned level, uint64_t request_id) const {
  return pte_request(key, level, pte_address(key, level), request_id);
}

bool radix_page_table_backend::owns_pte_physical_address(uint64_t pa) const {
  return pa >= m_config.pte_physical_base &&
         pa < m_config.pte_physical_base + m_config.pte_physical_bytes;
}

bool tlb_config::valid() const {
  return entries != 0 && assoc != 0 && ports_per_cycle != 0 &&
         entries >= assoc && (entries % assoc) == 0;
}

unsigned tlb_config::sets() const {
  assert(valid());
  return entries / assoc;
}

bool pwc_config::valid() const {
  if (mode > PWC_IDEAL || lookup_latency == 0) return false;
  if (mode == PWC_OFF) return entries == 0;
  if (mode == PWC_FINITE) return entries != 0;
  return true;  // IDEAL is intentionally unbounded; entries is ignored.
}

set_associative_tlb::set_associative_tlb(const tlb_config &config)
    : m_config(config), m_entries(), m_stats(), m_port_cycle(~0ULL),
      m_ports_used(0), m_touch_clock(0) {
  assert(m_config.valid());
  m_entries.resize(m_config.entries);
}

unsigned set_associative_tlb::set_for(const translation_key &key) const {
  const uint64_t mixed = key.vpn ^ (uint64_t(key.asid) << 17) ^
                         (key.page_size >> 12);
  return unsigned(mixed % m_config.sets());
}

bool set_associative_tlb::try_consume_port(uint64_t cycle) {
  if (m_port_cycle != cycle) {
    m_port_cycle = cycle;
    m_ports_used = 0;
  }
  if (m_ports_used >= m_config.ports_per_cycle) {
    ++m_stats.port_stalls;
    return false;
  }
  ++m_ports_used;
  return true;
}

bool set_associative_tlb::probe(const translation_key &key, uint64_t cycle,
                                uint64_t *ppn) {
  (void)cycle;
  ++m_stats.accesses;
  const unsigned begin = set_for(key) * m_config.assoc;
  for (unsigned way = 0; way < m_config.assoc; ++way) {
    entry &candidate = m_entries[begin + way];
    if (candidate.valid && candidate.key == key) {
      candidate.last_touch = ++m_touch_clock;
      *ppn = candidate.ppn;
      ++m_stats.hits;
      return true;
    }
  }
  ++m_stats.misses;
  return false;
}

tlb_fill_result set_associative_tlb::fill(const translation_key &key,
                                          uint64_t ppn, uint64_t cycle,
                                          object_class object) {
  (void)cycle;
  const unsigned begin = set_for(key) * m_config.assoc;
  entry *victim = 0;
  for (unsigned way = 0; way < m_config.assoc; ++way) {
    entry &candidate = m_entries[begin + way];
    if (candidate.valid && candidate.key == key) {
      candidate.ppn = ppn;
      candidate.last_touch = ++m_touch_clock;
      candidate.object = object;
      return tlb_fill_result();
    }
    if (!candidate.valid && victim == 0) victim = &candidate;
  }
  if (victim == 0) {
    victim = &m_entries[begin];
    for (unsigned way = 1; way < m_config.assoc; ++way) {
      entry &candidate = m_entries[begin + way];
      if (candidate.last_touch < victim->last_touch) victim = &candidate;
    }
    ++m_stats.evictions;
    const object_class victim_object = victim->object;
    victim->valid = true;
    victim->key = key;
    victim->ppn = ppn;
    victim->last_touch = ++m_touch_clock;
    victim->object = object;
    return tlb_fill_result(true, victim_object);
  }
  victim->valid = true;
  victim->key = key;
  victim->ppn = ppn;
  victim->last_touch = ++m_touch_clock;
  victim->object = object;
  return tlb_fill_result();
}

unsigned set_associative_tlb::occupancy() const {
  unsigned result = 0;
  for (unsigned i = 0; i < m_entries.size(); ++i)
    if (m_entries[i].valid) ++result;
  return result;
}

bool translation_config::valid() const {
  return num_sms != 0 && vm_core::valid_page_size(page_size) && l1.valid() &&
         l2.valid() && mshr_entries != 0 && pwq_entries != 0 && walkers != 0 &&
         walk_latency != 0 && ptw_mode <= 1 && page_table.valid() &&
         pwc.valid();
}

translation_controller::translation_controller(const translation_config &config)
    : m_config(config), m_default_page_table(config.page_table),
      m_page_table(&m_default_page_table), m_l1s(), m_l2(config.l2), m_mshrs(),
      m_lookups(), m_pwq(), m_active_walks(), m_pwc(),
      m_object_map(config.object_map_path), m_object_unique_keys(), m_stats(),
      m_next_pte_request_id(1), m_pwc_touch_clock(0) {
  assert(m_config.valid());
  m_stats.object_attribution_enabled = m_object_map.enabled();
  initialize_pwc_stats();
  for (unsigned sid = 0; sid < m_config.num_sms; ++sid)
    m_l1s.push_back(set_associative_tlb(m_config.l1));
}

translation_controller::translation_controller(const translation_config &config,
                                               page_table_backend *backend)
    : m_config(config), m_default_page_table(config.page_table),
      m_page_table(backend), m_l1s(), m_l2(config.l2), m_mshrs(), m_lookups(),
      m_pwq(), m_active_walks(), m_pwc(), m_object_map(config.object_map_path),
      m_object_unique_keys(), m_stats(),
      m_next_pte_request_id(1),
      m_pwc_touch_clock(0) {
  assert(m_config.valid());
  assert(m_page_table != 0 && m_page_table->valid());
  m_stats.object_attribution_enabled = m_object_map.enabled();
  initialize_pwc_stats();
  for (unsigned sid = 0; sid < m_config.num_sms; ++sid)
    m_l1s.push_back(set_associative_tlb(m_config.l1));
}

void translation_controller::initialize_pwc_stats() {
  const unsigned levels = m_page_table->levels();
  m_stats.pwc_accesses_by_level.assign(levels, 0);
  m_stats.pwc_hits_by_level.assign(levels, 0);
  m_stats.pwc_misses_by_level.assign(levels, 0);
  m_stats.pwc_pte_requests_skipped_by_level.assign(levels, 0);
}

object_class translation_controller::classify_key(
    const translation_key &key) const {
  assert(key.page_size != 0 && key.vpn <= ~0ULL / key.page_size);
  return m_object_map.classify(key.vpn * key.page_size, key.page_size);
}

void translation_controller::note_object_requester(
    object_class object, const translation_key &key) {
  if (!m_stats.object_attribution_enabled) return;
  assert(object < OBJECT_CLASS_COUNT);
  ++m_stats.object[object].translation_requesters;
  m_object_unique_keys[object].insert(key);
}

bool translation_controller::pwc_identity(const translation_key &key,
                                          unsigned level,
                                          unsigned *page_class,
                                          uint64_t *prefix) const {
  assert(page_class != 0 && prefix != 0);
  return m_page_table->pte_prefix_identity(key, level, page_class, prefix);
}

bool translation_controller::pwc_lookup(const active_walk &walk) {
  assert(pwc_enabled() && !pwc_is_leaf(walk.next_level));
  unsigned page_class = 0;
  uint64_t prefix = 0;
  assert(pwc_identity(walk.key, walk.next_level, &page_class, &prefix));
  const unsigned level = walk.next_level;
  ++m_stats.pwc_accesses;
  if (m_stats.object_attribution_enabled)
    ++m_stats.object[walk.object].pwc_accesses;
  ++m_stats.pwc_accesses_by_level[level];
  m_stats.pwc_service_cycles += m_config.pwc.lookup_latency;
  for (unsigned index = 0; index < m_pwc.size(); ++index) {
    pwc_entry &entry = m_pwc[index];
    if (entry.asid == walk.key.asid && entry.page_class == page_class &&
        entry.level == level && entry.prefix == prefix) {
      entry.last_touch = ++m_pwc_touch_clock;
      ++m_stats.pwc_hits;
      if (m_stats.object_attribution_enabled)
        ++m_stats.object[walk.object].pwc_hits;
      ++m_stats.pwc_hits_by_level[level];
      ++m_stats.pwc_pte_requests_skipped_by_level[level];
      return true;
    }
  }
  ++m_stats.pwc_misses;
  if (m_stats.object_attribution_enabled)
    ++m_stats.object[walk.object].pwc_misses;
  ++m_stats.pwc_misses_by_level[level];
  return false;
}

void translation_controller::pwc_insert(const translation_key &key,
                                        unsigned level) {
  if (!pwc_enabled() || pwc_is_leaf(level)) return;
  unsigned page_class = 0;
  uint64_t prefix = 0;
  assert(pwc_identity(key, level, &page_class, &prefix));
  for (unsigned index = 0; index < m_pwc.size(); ++index) {
    pwc_entry &entry = m_pwc[index];
    if (entry.asid == key.asid && entry.page_class == page_class &&
        entry.level == level && entry.prefix == prefix) {
      entry.last_touch = ++m_pwc_touch_clock;
      return;
    }
  }
  if (m_config.pwc.mode == PWC_FINITE &&
      m_pwc.size() >= m_config.pwc.entries) {
    unsigned victim = 0;
    for (unsigned index = 1; index < m_pwc.size(); ++index)
      if (m_pwc[index].last_touch < m_pwc[victim].last_touch) victim = index;
    m_pwc[victim] = pwc_entry(key.asid, page_class, level, prefix,
                              ++m_pwc_touch_clock);
    ++m_stats.pwc_evictions;
  } else {
    m_pwc.push_back(pwc_entry(key.asid, page_class, level, prefix,
                              ++m_pwc_touch_clock));
  }
  ++m_stats.pwc_inserts;
  m_stats.pwc_occupancy = m_pwc.size();
  if (m_stats.pwc_occupancy > m_stats.pwc_occupancy_high_watermark)
    m_stats.pwc_occupancy_high_watermark = m_stats.pwc_occupancy;
}

void translation_controller::service_pwc(uint64_t cycle) {
  if (!pwc_enabled()) return;
  for (unsigned index = 0; index < m_active_walks.size(); ++index) {
    active_walk &walk = m_active_walks[index];
    if (walk.pte_outstanding || walk.pwc_miss_ready ||
        pwc_is_leaf(walk.next_level))
      continue;
    if (!walk.pwc_probe_scheduled) {
      walk.pwc_probe_scheduled = true;
      walk.pwc_probe_ready_cycle = cycle + m_config.pwc.lookup_latency;
      continue;
    }
    if (walk.pwc_probe_ready_cycle > cycle) continue;
    walk.pwc_probe_scheduled = false;
    if (pwc_lookup(walk)) {
      ++walk.next_level;
    } else {
      walk.pwc_miss_ready = true;
    }
  }
}

bool translation_controller::mshr_entry::has_waiter(uint64_t uid) const {
  for (unsigned i = 0; i < waiters.size(); ++i)
    if (waiters[i].uid == uid) return true;
  return false;
}

translation_controller::mshr_entry *translation_controller::find_mshr(
    const translation_key &key) {
  for (unsigned i = 0; i < m_mshrs.size(); ++i)
    if (m_mshrs[i].key == key) return &m_mshrs[i];
  return 0;
}

const translation_controller::mshr_entry *translation_controller::find_mshr(
    const translation_key &key) const {
  for (unsigned i = 0; i < m_mshrs.size(); ++i)
    if (m_mshrs[i].key == key) return &m_mshrs[i];
  return 0;
}

translation_controller::lookup_operation *translation_controller::find_lookup(
    unsigned sid, uint64_t uid, const translation_key &key) {
  for (unsigned i = 0; i < m_lookups.size(); ++i)
    if (m_lookups[i].sid == sid && m_lookups[i].uid == uid &&
        m_lookups[i].key == key)
      return &m_lookups[i];
  return 0;
}

const translation_controller::lookup_operation *
translation_controller::find_lookup(unsigned sid, uint64_t uid,
                                    const translation_key &key) const {
  for (unsigned i = 0; i < m_lookups.size(); ++i)
    if (m_lookups[i].sid == sid && m_lookups[i].uid == uid &&
        m_lookups[i].key == key)
      return &m_lookups[i];
  return 0;
}

void translation_controller::service_lookups(uint64_t cycle) {
  // A stage transition may be zero-cycle in the legacy diagnostic setting.
  // Iterate to a fixed point at this cycle, but never probe a completed stage
  // twice: each operation advances monotonically or waits for a resource.
  bool progress = true;
  while (progress) {
    progress = false;
    for (unsigned index = 0; index < m_lookups.size();) {
      lookup_operation &lookup = m_lookups[index];
      if (lookup.stage == LOOKUP_L1_SERVICE && lookup.ready_cycle <= cycle) {
        uint64_t ppn = 0;
        const bool hit = m_l1s[lookup.sid].probe(lookup.key, cycle, &ppn);
        ++m_stats.l1_lookup_completions;
        if (m_stats.object_attribution_enabled) {
          if (hit)
            ++m_stats.object[lookup.object].l1_hits;
          else
            ++m_stats.object[lookup.object].l1_misses;
        }
        m_stats.l1_lookup_service_cycles += m_config.l1_lookup_latency;
        lookup.l1_complete_cycle = cycle;
        if (hit) {
          lookup.ppn = ppn;
          if (lookup.source == TRANSLATION_SOURCE_UNOBSERVED)
            lookup.source = TRANSLATION_SOURCE_L1_TLB_HIT;
          lookup.stage = LOOKUP_READY;
        } else {
          lookup.stage = LOOKUP_L2_LAUNCH;
        }
        progress = true;
        ++index;
        continue;
      }
      if (lookup.stage == LOOKUP_L2_LAUNCH) {
        if (!m_l2.try_consume_port(cycle)) {
          ++index;
          continue;
        }
        ++m_stats.l2_lookup_launches;
        if (m_stats.object_attribution_enabled)
          ++m_stats.object[lookup.object].l2_lookup_launches;
        lookup.l2_issued = true;
        lookup.l2_launch_cycle = cycle;
        lookup.stage = LOOKUP_L2_SERVICE;
        lookup.ready_cycle = cycle + m_config.l2_lookup_latency;
        progress = true;
        ++index;
        continue;
      }
      if (lookup.stage == LOOKUP_L2_SERVICE && lookup.ready_cycle <= cycle) {
        uint64_t ppn = 0;
        const bool hit = m_l2.probe(lookup.key, cycle, &ppn);
        ++m_stats.l2_lookup_completions;
        if (m_stats.object_attribution_enabled) {
          if (hit)
            ++m_stats.object[lookup.object].l2_hits;
          else
            ++m_stats.object[lookup.object].l2_misses;
        }
        m_stats.l2_lookup_service_cycles += m_config.l2_lookup_latency;
        lookup.l2_complete_cycle = cycle;
        if (hit) {
          m_l1s[lookup.sid].fill(lookup.key, ppn, cycle, lookup.object);
          lookup.ppn = ppn;
          lookup.source = TRANSLATION_SOURCE_L2_TLB_HIT;
          lookup.stage = LOOKUP_READY;
        } else {
          lookup.stage = LOOKUP_MSHR_HANDOFF;
        }
        progress = true;
        ++index;
        continue;
      }
      if (lookup.stage == LOOKUP_MSHR_HANDOFF) {
        const lookup_result result =
            allocate_or_merge(lookup.sid, lookup.uid, lookup.key, cycle,
                              &lookup);
        if (result == TRANSLATION_PENDING) {
          m_lookups.erase(m_lookups.begin() + index);
          progress = true;
          continue;
        }
        // Capacity backpressure waits at a completed handoff; it never
        // returns to an L1/L2 probe or consumes another lookup port.
        assert(result == MSHR_FULL || result == PWQ_FULL);
      }
      ++index;
    }
  }
}

lookup_result translation_controller::allocate_or_merge(
    unsigned sid, uint64_t waiter_uid, const translation_key &key,
    uint64_t cycle, const lookup_operation *lookup) {
  assert(lookup != 0);
  const uint64_t entry = lookup ? lookup->entry_cycle : cycle;
  const uint64_t l1_launch = lookup ? lookup->l1_launch_cycle : cycle;
  const uint64_t l1_complete = lookup ? lookup->l1_complete_cycle : cycle;
  const bool l2_issued = lookup ? lookup->l2_issued : false;
  const uint64_t l2_launch = lookup ? lookup->l2_launch_cycle : cycle;
  const uint64_t l2_complete = lookup ? lookup->l2_complete_cycle : cycle;
  mshr_entry *existing = find_mshr(key);
  if (existing != 0) {
    if (existing->has_waiter(waiter_uid)) return TRANSLATION_PENDING;
    existing->waiters.push_back(waiter(sid, waiter_uid, entry, l1_launch,
                                       l1_complete, l2_issued, l2_launch,
                                       l2_complete, cycle, lookup->object));
    ++m_stats.mshr_merges;
    if (m_stats.object_attribution_enabled)
      ++m_stats.object[lookup->object].mshr_merges;
    ++m_stats.waiter_registrations;
    if (existing->waiters.size() > m_stats.mshr_waiter_depth_max)
      m_stats.mshr_waiter_depth_max = existing->waiters.size();
    return TRANSLATION_PENDING;
  }
  if (m_mshrs.size() >= m_config.mshr_entries) {
    ++m_stats.mshr_full_events;
    return MSHR_FULL;
  }
  if (m_pwq.size() >= m_config.pwq_entries) {
    ++m_stats.pwq_full_events;
    return PWQ_FULL;
  }
  assert(find_mshr(key) == 0);
  m_mshrs.push_back(mshr_entry(key, cycle, lookup->object));
  m_mshrs.back().waiters.push_back(waiter(sid, waiter_uid, entry, l1_launch,
                                         l1_complete, l2_issued, l2_launch,
                                         l2_complete, cycle, lookup->object));
  m_pwq.push_back(key);
  ++m_stats.mshr_allocations;
  if (m_stats.object_attribution_enabled)
    ++m_stats.object[lookup->object].mshr_allocations;
  ++m_stats.waiter_registrations;
  note_mshr_occupancy();
  if (m_mshrs.back().waiters.size() > m_stats.mshr_waiter_depth_max)
    m_stats.mshr_waiter_depth_max = m_mshrs.back().waiters.size();
  return TRANSLATION_PENDING;
}

void translation_controller::note_mshr_occupancy() {
  if (m_mshrs.size() > m_stats.mshr_occupancy_high_watermark)
    m_stats.mshr_occupancy_high_watermark = m_mshrs.size();
}

void translation_controller::note_requester_completion(
    uint64_t entry_cycle, uint64_t l1_launch_cycle,
    uint64_t l1_complete_cycle, bool l2_issued, uint64_t l2_launch_cycle,
    uint64_t l2_complete_cycle, uint64_t mshr_join_cycle, bool used_mshr,
    uint64_t ready_cycle, object_class object) {
  assert(entry_cycle <= l1_launch_cycle);
  assert(l1_launch_cycle <= l1_complete_cycle);
  assert(l1_complete_cycle <= ready_cycle);
  if (l2_issued) {
    assert(l1_complete_cycle <= l2_launch_cycle);
    assert(l2_launch_cycle <= l2_complete_cycle);
    assert(l2_complete_cycle <= ready_cycle);
  }
  if (used_mshr) {
    assert(l2_complete_cycle <= mshr_join_cycle);
    assert(mshr_join_cycle <= ready_cycle);
  }
  const uint64_t total = ready_cycle - entry_cycle;
  const uint64_t l1_queue = l1_launch_cycle - entry_cycle;
  const uint64_t l1_service = l1_complete_cycle - l1_launch_cycle;
  const uint64_t l2_queue =
      l2_issued ? l2_launch_cycle - l1_complete_cycle : 0;
  const uint64_t l2_service =
      l2_issued ? l2_complete_cycle - l2_launch_cycle : 0;
  ++m_stats.requester_completions;
  m_stats.requester_latency_cycles_total += total;
  if (total > m_stats.requester_latency_cycles_max)
    m_stats.requester_latency_cycles_max = total;
  m_stats.requester_l1_queue_cycles_total += l1_queue;
  if (l1_queue > m_stats.requester_l1_queue_cycles_max)
    m_stats.requester_l1_queue_cycles_max = l1_queue;
  m_stats.requester_l1_service_cycles_total += l1_service;
  if (l1_service > m_stats.requester_l1_service_cycles_max)
    m_stats.requester_l1_service_cycles_max = l1_service;
  m_stats.requester_l2_queue_cycles_total += l2_queue;
  if (l2_queue > m_stats.requester_l2_queue_cycles_max)
    m_stats.requester_l2_queue_cycles_max = l2_queue;
  m_stats.requester_l2_service_cycles_total += l2_service;
  if (l2_service > m_stats.requester_l2_service_cycles_max)
    m_stats.requester_l2_service_cycles_max = l2_service;
  if (used_mshr) {
    const uint64_t mshr_wait = ready_cycle - mshr_join_cycle;
    m_stats.requester_mshr_wait_cycles_total += mshr_wait;
    if (mshr_wait > m_stats.requester_mshr_wait_cycles_max)
      m_stats.requester_mshr_wait_cycles_max = mshr_wait;
  }
  if (!m_stats.object_attribution_enabled) return;
  translation_stats::object_stats &object_stats = m_stats.object[object];
  ++object_stats.completed;
  object_stats.requester_latency_cycles_total += total;
  if (total > object_stats.requester_latency_cycles_max)
    object_stats.requester_latency_cycles_max = total;
  if (used_mshr) {
    const uint64_t mshr_wait = ready_cycle - mshr_join_cycle;
    object_stats.requester_mshr_wait_cycles_total += mshr_wait;
    if (mshr_wait > object_stats.requester_mshr_wait_cycles_max)
      object_stats.requester_mshr_wait_cycles_max = mshr_wait;
  }
}

lookup_result translation_controller::translate(unsigned sid, unsigned asid,
                                                uint64_t sim_va,
                                                uint64_t request_bytes,
                                                uint64_t cycle,
                                                uint64_t waiter_uid,
                                                uint64_t *sim_pa,
                                                translation_source *source) {
  assert(sid < m_l1s.size());
  if (source != 0) *source = TRANSLATION_SOURCE_UNOBSERVED;
  translation_key key(asid, vm_core::vpn(sim_va, m_config.page_size),
                      m_config.page_size);
  const object_class object = m_object_map.classify(sim_va, request_bytes);
  // A previously accepted waiter is already represented by this active MSHR.
  // It must wait without competing for either TLB lookup resource or
  // polluting probe/miss statistics.  A distinct UID deliberately falls
  // through to the normal first lookup and may then merge below.
  const mshr_entry *existing = find_mshr(key);
  if (existing != 0 && existing->has_waiter(waiter_uid)) {
    ++m_stats.pending_waiter_bypasses;
    return TRANSLATION_PENDING;
  }
  lookup_operation *inflight = find_lookup(sid, waiter_uid, key);
  if (inflight != 0) {
    if (inflight->stage != LOOKUP_READY) {
      ++m_stats.lookup_inflight_bypasses;
      return TRANSLATION_PENDING;
    }
    const uint64_t ppn = inflight->ppn;
    const translation_source completed_source = inflight->source;
    note_requester_completion(
        inflight->entry_cycle, inflight->l1_launch_cycle,
        inflight->l1_complete_cycle, inflight->l2_issued,
        inflight->l2_launch_cycle, inflight->l2_complete_cycle, 0, false,
        cycle, inflight->object);
    for (unsigned index = 0; index < m_lookups.size(); ++index)
      if (&m_lookups[index] == inflight) {
        m_lookups.erase(m_lookups.begin() + index);
        break;
      }
    *sim_pa =
        ppn * key.page_size + vm_core::page_offset(sim_va, key.page_size);
    ++m_stats.completed;
    if (source != 0) *source = completed_source;
    return READY;
  }
  ++m_stats.lookup_requests;
  set_associative_tlb &l1_tlb = m_l1s[sid];
  if (!l1_tlb.try_consume_port(cycle)) return L1_PORT_STALL;
  ++m_stats.l1_lookup_launches;
  if (m_stats.object_attribution_enabled)
    ++m_stats.object[object].l1_lookup_launches;
  note_object_requester(object, key);
  m_lookups.push_back(
      lookup_operation(sid, waiter_uid, key, cycle,
                       cycle + m_config.l1_lookup_latency, object));
  std::map<uint64_t, completed_outcome>::iterator completed =
      m_completed_outcomes.find(waiter_uid);
  if (completed != m_completed_outcomes.end() && completed->second.key == key) {
    m_lookups.back().source = completed->second.source;
    m_completed_outcomes.erase(completed);
  }
  // Preserve the accepted zero-latency diagnostic behavior without making the
  // non-zero model poll or re-probe.  service_lookups() completes only stages
  // whose modeled service interval ends at this cycle.
  if (m_config.l1_lookup_latency == 0) service_lookups(cycle);
  inflight = find_lookup(sid, waiter_uid, key);
  if (inflight != 0 && inflight->stage == LOOKUP_READY)
    return translate(sid, asid, sim_va, request_bytes, cycle, waiter_uid,
                     sim_pa, source);
  // Legacy zero-latency diagnostics historically surface MSHR/PWQ fullness
  // synchronously.  Preserve that contract without allowing the non-zero
  // service path to re-probe either TLB while a handoff is stalled.
  if (m_config.l1_lookup_latency == 0 && inflight != 0 &&
      inflight->stage == LOOKUP_MSHR_HANDOFF) {
    const lookup_result result =
        m_mshrs.size() >= m_config.mshr_entries ? MSHR_FULL : PWQ_FULL;
    for (unsigned index = 0; index < m_lookups.size(); ++index)
      if (&m_lookups[index] == inflight) {
        m_lookups.erase(m_lookups.begin() + index);
        break;
      }
    return result;
  }
  return TRANSLATION_PENDING;
}

bool translation_controller::complete_translation(const translation_key &key,
                                                   uint64_t cycle) {
  for (unsigned index = 0; index < m_mshrs.size(); ++index) {
    if (!(m_mshrs[index].key == key)) continue;
    const uint64_t ppn = m_page_table->resolve_ppn(key);
    ++m_stats.mapper_lookups;
    const object_class fill_object = classify_key(key);
    const tlb_fill_result l2_fill =
        m_l2.fill(key, ppn, cycle, fill_object);
    if (m_stats.object_attribution_enabled) {
      ++m_stats.object[fill_object].l2_fills;
      if (l2_fill.evicted)
        ++m_stats.l2_replacement_matrix[fill_object]
                                      [l2_fill.victim_object];
    }
    for (unsigned waiter_index = 0;
         waiter_index < m_mshrs[index].waiters.size(); ++waiter_index) {
      const waiter &entry_waiter = m_mshrs[index].waiters[waiter_index];
      assert(entry_waiter.sid < m_l1s.size());
      m_l1s[entry_waiter.sid].fill(key, ppn, cycle, entry_waiter.object);
      note_requester_completion(
          entry_waiter.entry_cycle, entry_waiter.l1_launch_cycle,
          entry_waiter.l1_complete_cycle, entry_waiter.l2_issued,
          entry_waiter.l2_launch_cycle, entry_waiter.l2_complete_cycle,
          entry_waiter.mshr_join_cycle, true, cycle, entry_waiter.object);
      // The regular requester retry will still perform the accepted L1 probe
      // and obtain the same SimPA.  This side map preserves only the fact
      // that its residency originated in a PTW, for later cache correlation.
      m_completed_outcomes[entry_waiter.uid] =
          completed_outcome(key, TRANSLATION_SOURCE_PTW);
      ++m_stats.waiter_wakeups;
    }
    const uint64_t waiter_depth = m_mshrs[index].waiters.size();
    const uint64_t lifetime = cycle - m_mshrs[index].enqueue_cycle;
    ++m_stats.mshr_entries_completed;
    m_stats.mshr_waiter_depth_total += waiter_depth;
    if (waiter_depth > m_stats.mshr_waiter_depth_max)
      m_stats.mshr_waiter_depth_max = waiter_depth;
    m_stats.mshr_lifetime_cycles_total += lifetime;
    if (lifetime > m_stats.mshr_lifetime_cycles_max)
      m_stats.mshr_lifetime_cycles_max = lifetime;
    m_mshrs.erase(m_mshrs.begin() + index);
    for (unsigned queued = 0; queued < m_pwq.size(); ++queued) {
      if (m_pwq[queued] == key) {
        m_pwq.erase(m_pwq.begin() + queued);
        break;
      }
    }
    ++m_stats.mshr_releases;
    return true;
  }
  return false;
}

void translation_controller::cycle(uint64_t cycle) {
  service_lookups(cycle);
  if (uses_real_memory_ptw()) {
    while (!m_pwq.empty() && m_active_walks.size() < m_config.walkers) {
      const translation_key key = m_pwq.front();
      m_pwq.erase(m_pwq.begin());
      mshr_entry *entry = find_mshr(key);
      assert(entry != 0);
      m_stats.pwq_wait_cycles += cycle - entry->enqueue_cycle;
      m_active_walks.push_back(active_walk(key, cycle, 0, entry->object));
      ++m_stats.walk_starts;
      if (m_stats.object_attribution_enabled)
        ++m_stats.object[entry->object].walk_starts;
    }
    // A PWC probe is a one-cycle generic service, with no modeled port
    // contention.  A hit advances exactly one intermediate level; a miss
    // makes that level available to the existing physical PTE path below.
    service_pwc(cycle);
    assert(m_active_walks.size() <= m_config.walkers);
    return;
  }
  for (unsigned index = 0; index < m_active_walks.size();) {
    if (m_active_walks[index].ready_cycle > cycle) {
      ++index;
      continue;
    }
    const active_walk finished = m_active_walks[index];
    assert(complete_translation(finished.key, cycle));
    ++m_stats.walk_completions;
    m_stats.walk_service_cycles += cycle - finished.start_cycle;
    m_active_walks.erase(m_active_walks.begin() + index);
  }
  while (!m_pwq.empty() && m_active_walks.size() < m_config.walkers) {
    const translation_key key = m_pwq.front();
    m_pwq.erase(m_pwq.begin());
    mshr_entry *entry = find_mshr(key);
    assert(entry != 0);
    m_stats.pwq_wait_cycles += cycle - entry->enqueue_cycle;
    m_active_walks.push_back(active_walk(key, cycle,
                                         cycle + m_config.walk_latency,
                                         entry->object));
    ++m_stats.walk_starts;
    if (m_stats.object_attribution_enabled)
      ++m_stats.object[entry->object].walk_starts;
  }
  assert(m_active_walks.size() <= m_config.walkers);
}

bool translation_controller::next_pte_request(pte_request *request) const {
  assert(request != 0);
  if (!uses_real_memory_ptw()) return false;
  for (unsigned index = 0; index < m_active_walks.size(); ++index) {
    const active_walk &walk = m_active_walks[index];
    if (walk.pte_outstanding) continue;
    assert(walk.next_level < m_page_table->levels());
    if (pwc_enabled() && !pwc_is_leaf(walk.next_level) &&
        !walk.pwc_miss_ready)
      continue;
    const uint64_t request_id =
        walk.pte_request_id ? walk.pte_request_id : m_next_pte_request_id;
    *request =
        m_page_table->make_pte_request(walk.key, walk.next_level, request_id);
    assert(request->is_physical && request->bypass_translation);
    return true;
  }
  return false;
}

bool translation_controller::pte_request_issued(const pte_request &request,
                                                 uint64_t cycle) {
  if (!uses_real_memory_ptw() || !request.is_physical ||
      !request.bypass_translation ||
      !m_page_table->owns_pte_physical_address(request.physical_address))
    return false;
  for (unsigned index = 0; index < m_active_walks.size(); ++index) {
    active_walk &walk = m_active_walks[index];
    if (walk.key == request.key && !walk.pte_outstanding &&
        walk.next_level == request.level &&
        (walk.pte_request_id == 0 || walk.pte_request_id == request.request_id)) {
      if (pwc_enabled() && !pwc_is_leaf(walk.next_level) &&
          !walk.pwc_miss_ready)
        return false;
      walk.pte_outstanding = true;
      walk.pte_request_id = request.request_id;
      walk.pte_issue_cycle = cycle;
      walk.pwc_miss_ready = false;
      if (request.request_id == m_next_pte_request_id) ++m_next_pte_request_id;
      ++m_stats.pte_requests;
      if (m_stats.object_attribution_enabled)
        ++m_stats.object[walk.object].pte_requests;
      return true;
    }
  }
  return false;
}

bool translation_controller::complete_pte_response(uint64_t request_id,
                                                    uint64_t physical_address,
                                                    bool reached_dram,
                                                    uint64_t cycle) {
  if (!uses_real_memory_ptw() ||
      !m_page_table->owns_pte_physical_address(physical_address))
    return false;
  for (unsigned index = 0; index < m_active_walks.size(); ++index) {
    active_walk &walk = m_active_walks[index];
    if (!walk.pte_outstanding || walk.pte_request_id != request_id) continue;
    const pte_request expected =
        m_page_table->make_pte_request(walk.key, walk.next_level, request_id);
    if (expected.physical_address != physical_address) {
      ++m_stats.pte_response_misassociations;
      return false;
    }
    ++m_stats.pte_responses;
    assert(walk.pte_issue_cycle <= cycle);
    const uint64_t pte_memory_wait = cycle - walk.pte_issue_cycle;
    m_stats.pte_memory_wait_cycles_total += pte_memory_wait;
    if (pte_memory_wait > m_stats.pte_memory_wait_cycles_max)
      m_stats.pte_memory_wait_cycles_max = pte_memory_wait;
    if (m_stats.object_attribution_enabled) {
      translation_stats::object_stats &object_stats =
          m_stats.object[walk.object];
      object_stats.pte_memory_wait_cycles_total += pte_memory_wait;
      if (pte_memory_wait > object_stats.pte_memory_wait_cycles_max)
        object_stats.pte_memory_wait_cycles_max = pte_memory_wait;
    }
    if (reached_dram) {
      ++m_stats.pte_dram_responses;
      if (m_stats.object_attribution_enabled)
        ++m_stats.object[walk.object].pte_dram_responses;
    } else {
      ++m_stats.pte_l2_only_responses;
      if (m_stats.object_attribution_enabled)
        ++m_stats.object[walk.object].pte_l2_only_responses;
    }
    const unsigned completed_level = walk.next_level;
    walk.pte_outstanding = false;
    walk.pte_request_id = 0;
    walk.pte_issue_cycle = 0;
    pwc_insert(walk.key, completed_level);
    ++walk.next_level;
    if (walk.next_level < m_page_table->levels()) return true;
    assert(complete_translation(walk.key, cycle));
    ++m_stats.walk_completions;
    m_stats.walk_service_cycles += cycle - walk.start_cycle;
    m_active_walks.erase(m_active_walks.begin() + index);
    return true;
  }
  ++m_stats.pte_response_misassociations;
  return false;
}

bool translation_controller::invariants_hold() const {
  if (m_stats.mshr_allocations < m_stats.mshr_releases ||
      m_stats.mshr_allocations - m_stats.mshr_releases != m_mshrs.size() ||
      m_stats.waiter_registrations < m_stats.waiter_wakeups ||
      m_stats.walk_starts < m_stats.walk_completions ||
      m_active_walks.size() > m_config.walkers ||
      m_stats.pwc_occupancy != m_pwc.size() ||
      (m_config.pwc.mode == PWC_FINITE &&
       m_pwc.size() > m_config.pwc.entries))
    return false;
  uint64_t active_waiters = 0;
  for (unsigned i = 0; i < m_mshrs.size(); ++i) {
    for (unsigned j = i + 1; j < m_mshrs.size(); ++j)
      if (m_mshrs[i].key == m_mshrs[j].key) return false;
    active_waiters += m_mshrs[i].waiters.size();
    for (unsigned a = 0; a < m_mshrs[i].waiters.size(); ++a)
      for (unsigned b = a + 1; b < m_mshrs[i].waiters.size(); ++b)
        if (m_mshrs[i].waiters[a].uid == m_mshrs[i].waiters[b].uid)
          return false;
  }
  if (m_stats.waiter_registrations - m_stats.waiter_wakeups != active_waiters)
    return false;
  for (unsigned i = 0; i < m_lookups.size(); ++i) {
    if (m_lookups[i].sid >= m_l1s.size() ||
        m_lookups[i].stage > LOOKUP_READY)
      return false;
    for (unsigned j = i + 1; j < m_lookups.size(); ++j)
      if (m_lookups[i].sid == m_lookups[j].sid &&
          m_lookups[i].uid == m_lookups[j].uid &&
          m_lookups[i].key == m_lookups[j].key)
        return false;
  }
  for (unsigned i = 0; i < m_pwq.size(); ++i)
    if (find_mshr(m_pwq[i]) == 0) return false;
  for (unsigned i = 0; i < m_active_walks.size(); ++i)
    if (find_mshr(m_active_walks[i].key) == 0 ||
        (uses_real_memory_ptw() &&
         m_active_walks[i].next_level >= m_page_table->levels()))
      return false;
  for (unsigned i = 0; i < m_pwc.size(); ++i)
    for (unsigned j = i + 1; j < m_pwc.size(); ++j)
      if (m_pwc[i].asid == m_pwc[j].asid &&
          m_pwc[i].page_class == m_pwc[j].page_class &&
          m_pwc[i].level == m_pwc[j].level &&
          m_pwc[i].prefix == m_pwc[j].prefix)
        return false;
  return true;
}

bool translation_controller::quiescent_invariants_hold() const {
  return m_mshrs.empty() && m_lookups.empty() &&
         m_completed_outcomes.empty() && invariants_hold();
}

bool translation_controller::object_attribution_conserves() const {
  if (!m_stats.object_attribution_enabled) return true;
  uint64_t requesters = 0;
  uint64_t l1_launches = 0;
  uint64_t l1_hits = 0;
  uint64_t l1_misses = 0;
  uint64_t l2_launches = 0;
  uint64_t l2_hits = 0;
  uint64_t l2_misses = 0;
  uint64_t mshr_allocations = 0;
  uint64_t mshr_merges = 0;
  uint64_t walk_starts = 0;
  uint64_t completed = 0;
  uint64_t pwc_accesses = 0;
  uint64_t pwc_hits = 0;
  uint64_t pwc_misses = 0;
  uint64_t pte_requests = 0;
  uint64_t pte_l2_only_responses = 0;
  uint64_t pte_dram_responses = 0;
  uint64_t pte_memory_wait = 0;
  uint64_t l2_fills = 0;
  uint64_t replacement_evictions = 0;
  for (unsigned object = 0; object < OBJECT_CLASS_COUNT; ++object) {
    const translation_stats::object_stats &stats = m_stats.object[object];
    requesters += stats.translation_requesters;
    l1_launches += stats.l1_lookup_launches;
    l1_hits += stats.l1_hits;
    l1_misses += stats.l1_misses;
    l2_launches += stats.l2_lookup_launches;
    l2_hits += stats.l2_hits;
    l2_misses += stats.l2_misses;
    mshr_allocations += stats.mshr_allocations;
    mshr_merges += stats.mshr_merges;
    walk_starts += stats.walk_starts;
    completed += stats.completed;
    pwc_accesses += stats.pwc_accesses;
    pwc_hits += stats.pwc_hits;
    pwc_misses += stats.pwc_misses;
    pte_requests += stats.pte_requests;
    pte_l2_only_responses += stats.pte_l2_only_responses;
    pte_dram_responses += stats.pte_dram_responses;
    pte_memory_wait += stats.pte_memory_wait_cycles_total;
    l2_fills += stats.l2_fills;
    for (unsigned victim = 0; victim < OBJECT_CLASS_COUNT; ++victim)
      replacement_evictions += m_stats.l2_replacement_matrix[object][victim];
  }
  tlb_stats l1_total;
  for (unsigned sid = 0; sid < m_l1s.size(); ++sid) {
    const tlb_stats &stats = m_l1s[sid].stats();
    l1_total.accesses += stats.accesses;
    l1_total.hits += stats.hits;
    l1_total.misses += stats.misses;
  }
  const tlb_stats &l2_stats = m_l2.stats();
  return requesters == m_stats.l1_lookup_launches &&
         l1_launches == m_stats.l1_lookup_launches &&
         l1_hits == l1_total.hits && l1_misses == l1_total.misses &&
         l1_hits + l1_misses == l1_total.accesses &&
         l2_launches == m_stats.l2_lookup_launches &&
         l2_hits == l2_stats.hits && l2_misses == l2_stats.misses &&
         l2_hits + l2_misses == l2_stats.accesses &&
         mshr_allocations == m_stats.mshr_allocations &&
         mshr_merges == m_stats.mshr_merges &&
         walk_starts == m_stats.walk_starts &&
         completed == m_stats.requester_completions &&
         pwc_accesses == m_stats.pwc_accesses && pwc_hits == m_stats.pwc_hits &&
         pwc_misses == m_stats.pwc_misses &&
         pte_requests == m_stats.pte_requests &&
         pte_l2_only_responses == m_stats.pte_l2_only_responses &&
         pte_dram_responses == m_stats.pte_dram_responses &&
         pte_memory_wait == m_stats.pte_memory_wait_cycles_total &&
         l2_fills == m_stats.mapper_lookups &&
         replacement_evictions == l2_stats.evictions;
}

const set_associative_tlb &translation_controller::l1(unsigned sid) const {
  assert(sid < m_l1s.size());
  return m_l1s[sid];
}

void translation_controller::print_stats(FILE *fout) const {
  tlb_stats l1_total;
  unsigned l1_occupancy = 0;
  for (unsigned sid = 0; sid < m_l1s.size(); ++sid) {
    const tlb_stats &stats = m_l1s[sid].stats();
    l1_total.accesses += stats.accesses;
    l1_total.hits += stats.hits;
    l1_total.misses += stats.misses;
    l1_total.evictions += stats.evictions;
    l1_total.port_stalls += stats.port_stalls;
    l1_occupancy += m_l1s[sid].occupancy();
  }
  const tlb_stats &l2_stats = m_l2.stats();
  fprintf(fout, "vm_translation_page_size_bytes = %llu\n",
          (unsigned long long)m_config.page_size);
  fprintf(fout, "vm_translation_lookup_requests = %llu\n",
          (unsigned long long)m_stats.lookup_requests);
  fprintf(fout, "vm_translation_pending_waiter_bypasses = %llu\n",
          (unsigned long long)m_stats.pending_waiter_bypasses);
  fprintf(fout, "vm_translation_lookup_inflight_bypasses = %llu\n",
          (unsigned long long)m_stats.lookup_inflight_bypasses);
  fprintf(fout, "vm_l1_tlb_lookup_latency_cycles = %u\n",
          m_config.l1_lookup_latency);
  fprintf(fout, "vm_l1_tlb_lookup_launches = %llu\n",
          (unsigned long long)m_stats.l1_lookup_launches);
  fprintf(fout, "vm_l1_tlb_lookup_completions = %llu\n",
          (unsigned long long)m_stats.l1_lookup_completions);
  fprintf(fout, "vm_l1_tlb_lookup_service_cycles = %llu\n",
          (unsigned long long)m_stats.l1_lookup_service_cycles);
  fprintf(fout, "vm_l2_tlb_lookup_latency_cycles = %u\n",
          m_config.l2_lookup_latency);
  fprintf(fout, "vm_l2_tlb_lookup_launches = %llu\n",
          (unsigned long long)m_stats.l2_lookup_launches);
  fprintf(fout, "vm_l2_tlb_lookup_completions = %llu\n",
          (unsigned long long)m_stats.l2_lookup_completions);
  fprintf(fout, "vm_l2_tlb_lookup_service_cycles = %llu\n",
          (unsigned long long)m_stats.l2_lookup_service_cycles);
  fprintf(fout, "vm_translation_requester_completions = %llu\n",
          (unsigned long long)m_stats.requester_completions);
  fprintf(fout, "vm_translation_requester_latency_cycles_total = %llu\n",
          (unsigned long long)m_stats.requester_latency_cycles_total);
  fprintf(fout, "vm_translation_requester_latency_cycles_max = %llu\n",
          (unsigned long long)m_stats.requester_latency_cycles_max);
  fprintf(fout, "vm_translation_requester_l1_queue_cycles_total = %llu\n",
          (unsigned long long)m_stats.requester_l1_queue_cycles_total);
  fprintf(fout, "vm_translation_requester_l1_queue_cycles_max = %llu\n",
          (unsigned long long)m_stats.requester_l1_queue_cycles_max);
  fprintf(fout, "vm_translation_requester_l1_service_cycles_total = %llu\n",
          (unsigned long long)m_stats.requester_l1_service_cycles_total);
  fprintf(fout, "vm_translation_requester_l1_service_cycles_max = %llu\n",
          (unsigned long long)m_stats.requester_l1_service_cycles_max);
  fprintf(fout, "vm_translation_requester_l2_queue_cycles_total = %llu\n",
          (unsigned long long)m_stats.requester_l2_queue_cycles_total);
  fprintf(fout, "vm_translation_requester_l2_queue_cycles_max = %llu\n",
          (unsigned long long)m_stats.requester_l2_queue_cycles_max);
  fprintf(fout, "vm_translation_requester_l2_service_cycles_total = %llu\n",
          (unsigned long long)m_stats.requester_l2_service_cycles_total);
  fprintf(fout, "vm_translation_requester_l2_service_cycles_max = %llu\n",
          (unsigned long long)m_stats.requester_l2_service_cycles_max);
  fprintf(fout, "vm_translation_requester_mshr_wait_cycles_total = %llu\n",
          (unsigned long long)m_stats.requester_mshr_wait_cycles_total);
  fprintf(fout, "vm_translation_requester_mshr_wait_cycles_max = %llu\n",
          (unsigned long long)m_stats.requester_mshr_wait_cycles_max);
  fprintf(fout, "vm_functional_mapper_lookups = %llu\n",
          (unsigned long long)m_stats.mapper_lookups);
  fprintf(fout, "vm_functional_completed = %llu\n",
          (unsigned long long)m_stats.completed);
  fprintf(fout, "vm_translation_mshr_allocations = %llu\n",
          (unsigned long long)m_stats.mshr_allocations);
  fprintf(fout, "vm_translation_mshr_merges = %llu\n",
          (unsigned long long)m_stats.mshr_merges);
  fprintf(fout, "vm_translation_mshr_full_events = %llu\n",
          (unsigned long long)m_stats.mshr_full_events);
  fprintf(fout, "vm_translation_mshr_active = %u\n", active_mshrs());
  fprintf(fout, "vm_translation_mshr_occupancy_high_watermark = %llu\n",
          (unsigned long long)m_stats.mshr_occupancy_high_watermark);
  fprintf(fout, "vm_translation_mshr_entries_completed = %llu\n",
          (unsigned long long)m_stats.mshr_entries_completed);
  fprintf(fout, "vm_translation_mshr_waiter_depth_total = %llu\n",
          (unsigned long long)m_stats.mshr_waiter_depth_total);
  fprintf(fout, "vm_translation_mshr_waiter_depth_max = %llu\n",
          (unsigned long long)m_stats.mshr_waiter_depth_max);
  fprintf(fout, "vm_translation_mshr_lifetime_cycles_total = %llu\n",
          (unsigned long long)m_stats.mshr_lifetime_cycles_total);
  fprintf(fout, "vm_translation_mshr_lifetime_cycles_max = %llu\n",
          (unsigned long long)m_stats.mshr_lifetime_cycles_max);
  fprintf(fout, "vm_translation_pwq_occupancy = %u\n", (unsigned)m_pwq.size());
  fprintf(fout, "vm_translation_pwq_full_events = %llu\n",
          (unsigned long long)m_stats.pwq_full_events);
  fprintf(fout, "vm_translation_walkers_active = %u\n", active_walkers());
  fprintf(fout, "vm_translation_walk_starts = %llu\n",
          (unsigned long long)m_stats.walk_starts);
  fprintf(fout, "vm_translation_walk_completions = %llu\n",
          (unsigned long long)m_stats.walk_completions);
  fprintf(fout, "vm_pte_requests = %llu\n",
          (unsigned long long)m_stats.pte_requests);
  fprintf(fout, "vm_pte_responses = %llu\n",
          (unsigned long long)m_stats.pte_responses);
  fprintf(fout, "vm_pte_l2_only_responses = %llu\n",
          (unsigned long long)m_stats.pte_l2_only_responses);
  fprintf(fout, "vm_pte_dram_responses = %llu\n",
          (unsigned long long)m_stats.pte_dram_responses);
  fprintf(fout, "vm_pte_response_misassociations = %llu\n",
          (unsigned long long)m_stats.pte_response_misassociations);
  fprintf(fout, "vm_pte_memory_wait_cycles_total = %llu\n",
          (unsigned long long)m_stats.pte_memory_wait_cycles_total);
  fprintf(fout, "vm_pte_memory_wait_cycles_max = %llu\n",
          (unsigned long long)m_stats.pte_memory_wait_cycles_max);
  fprintf(fout, "vm_pwc_mode = %u\n", m_config.pwc.mode);
  fprintf(fout, "vm_pwc_entries_configured = %u\n", m_config.pwc.entries);
  fprintf(fout, "vm_pwc_lookup_latency_cycles = %u\n",
          m_config.pwc.lookup_latency);
  fprintf(fout, "vm_pwc_accesses = %llu\n",
          (unsigned long long)m_stats.pwc_accesses);
  fprintf(fout, "vm_pwc_hits = %llu\n",
          (unsigned long long)m_stats.pwc_hits);
  fprintf(fout, "vm_pwc_misses = %llu\n",
          (unsigned long long)m_stats.pwc_misses);
  fprintf(fout, "vm_pwc_inserts = %llu\n",
          (unsigned long long)m_stats.pwc_inserts);
  fprintf(fout, "vm_pwc_evictions = %llu\n",
          (unsigned long long)m_stats.pwc_evictions);
  fprintf(fout, "vm_pwc_occupancy = %llu\n",
          (unsigned long long)m_stats.pwc_occupancy);
  fprintf(fout, "vm_pwc_occupancy_high_watermark = %llu\n",
          (unsigned long long)m_stats.pwc_occupancy_high_watermark);
  fprintf(fout, "vm_pwc_service_cycles = %llu\n",
          (unsigned long long)m_stats.pwc_service_cycles);
  for (unsigned level = 0; level < m_stats.pwc_accesses_by_level.size();
       ++level) {
    fprintf(fout, "vm_pwc_level_%u_accesses = %llu\n", level,
            (unsigned long long)m_stats.pwc_accesses_by_level[level]);
    fprintf(fout, "vm_pwc_level_%u_hits = %llu\n", level,
            (unsigned long long)m_stats.pwc_hits_by_level[level]);
    fprintf(fout, "vm_pwc_level_%u_misses = %llu\n", level,
            (unsigned long long)m_stats.pwc_misses_by_level[level]);
    fprintf(fout, "vm_pwc_level_%u_pte_requests_skipped = %llu\n", level,
            (unsigned long long)m_stats.pwc_pte_requests_skipped_by_level[level]);
  }
  fprintf(fout, "vm_translation_waiter_registrations = %llu\n",
          (unsigned long long)m_stats.waiter_registrations);
  fprintf(fout, "vm_translation_waiter_wakeups = %llu\n",
          (unsigned long long)m_stats.waiter_wakeups);
  fprintf(fout, "vm_l1_tlb_accesses = %llu\n",
          (unsigned long long)l1_total.accesses);
  fprintf(fout, "vm_l1_tlb_hits = %llu\n", (unsigned long long)l1_total.hits);
  fprintf(fout, "vm_l1_tlb_misses = %llu\n",
          (unsigned long long)l1_total.misses);
  fprintf(fout, "vm_l1_tlb_evictions = %llu\n",
          (unsigned long long)l1_total.evictions);
  fprintf(fout, "vm_l1_tlb_port_stalls = %llu\n",
          (unsigned long long)l1_total.port_stalls);
  fprintf(fout, "vm_l1_tlb_occupancy = %u\n", l1_occupancy);
  fprintf(fout, "vm_l2_tlb_accesses = %llu\n",
          (unsigned long long)l2_stats.accesses);
  fprintf(fout, "vm_l2_tlb_hits = %llu\n", (unsigned long long)l2_stats.hits);
  fprintf(fout, "vm_l2_tlb_misses = %llu\n",
          (unsigned long long)l2_stats.misses);
  fprintf(fout, "vm_l2_tlb_evictions = %llu\n",
          (unsigned long long)l2_stats.evictions);
  fprintf(fout, "vm_l2_tlb_port_stalls = %llu\n",
          (unsigned long long)l2_stats.port_stalls);
  fprintf(fout, "vm_l2_tlb_occupancy = %u\n", m_l2.occupancy());
  fprintf(fout, "vm_object_attribution_enabled = %u\n",
          m_stats.object_attribution_enabled ? 1U : 0U);
  if (!m_stats.object_attribution_enabled) return;
  for (unsigned object = 0; object < OBJECT_CLASS_COUNT; ++object) {
    const translation_stats::object_stats &stats = m_stats.object[object];
    const char *name = object_class_name(static_cast<object_class>(object));
    fprintf(fout, "vm_object_%s_translation_requesters = %llu\n", name,
            (unsigned long long)stats.translation_requesters);
    fprintf(fout, "vm_object_%s_unique_translation_keys = %llu\n", name,
            (unsigned long long)m_object_unique_keys[object].size());
    fprintf(fout, "vm_object_%s_l1_lookup_launches = %llu\n", name,
            (unsigned long long)stats.l1_lookup_launches);
    fprintf(fout, "vm_object_%s_l1_hits = %llu\n", name,
            (unsigned long long)stats.l1_hits);
    fprintf(fout, "vm_object_%s_l1_misses = %llu\n", name,
            (unsigned long long)stats.l1_misses);
    fprintf(fout, "vm_object_%s_l2_lookup_launches = %llu\n", name,
            (unsigned long long)stats.l2_lookup_launches);
    fprintf(fout, "vm_object_%s_l2_hits = %llu\n", name,
            (unsigned long long)stats.l2_hits);
    fprintf(fout, "vm_object_%s_l2_misses = %llu\n", name,
            (unsigned long long)stats.l2_misses);
    fprintf(fout, "vm_object_%s_mshr_allocations = %llu\n", name,
            (unsigned long long)stats.mshr_allocations);
    fprintf(fout, "vm_object_%s_mshr_merges = %llu\n", name,
            (unsigned long long)stats.mshr_merges);
    fprintf(fout, "vm_object_%s_walk_starts = %llu\n", name,
            (unsigned long long)stats.walk_starts);
    fprintf(fout, "vm_object_%s_completed = %llu\n", name,
            (unsigned long long)stats.completed);
    fprintf(fout, "vm_object_%s_requester_latency_cycles_total = %llu\n",
            name, (unsigned long long)stats.requester_latency_cycles_total);
    fprintf(fout, "vm_object_%s_requester_latency_cycles_max = %llu\n",
            name, (unsigned long long)stats.requester_latency_cycles_max);
    fprintf(fout, "vm_object_%s_requester_mshr_wait_cycles_total = %llu\n",
            name, (unsigned long long)stats.requester_mshr_wait_cycles_total);
    fprintf(fout, "vm_object_%s_requester_mshr_wait_cycles_max = %llu\n",
            name, (unsigned long long)stats.requester_mshr_wait_cycles_max);
    fprintf(fout, "vm_object_%s_pwc_accesses = %llu\n", name,
            (unsigned long long)stats.pwc_accesses);
    fprintf(fout, "vm_object_%s_pwc_hits = %llu\n", name,
            (unsigned long long)stats.pwc_hits);
    fprintf(fout, "vm_object_%s_pwc_misses = %llu\n", name,
            (unsigned long long)stats.pwc_misses);
    fprintf(fout, "vm_object_%s_pte_requests = %llu\n", name,
            (unsigned long long)stats.pte_requests);
    fprintf(fout, "vm_object_%s_pte_l2_only_responses = %llu\n", name,
            (unsigned long long)stats.pte_l2_only_responses);
    fprintf(fout, "vm_object_%s_pte_dram_responses = %llu\n", name,
            (unsigned long long)stats.pte_dram_responses);
    fprintf(fout, "vm_object_%s_pte_memory_wait_cycles_total = %llu\n",
            name, (unsigned long long)stats.pte_memory_wait_cycles_total);
    fprintf(fout, "vm_object_%s_pte_memory_wait_cycles_max = %llu\n", name,
            (unsigned long long)stats.pte_memory_wait_cycles_max);
    fprintf(fout, "vm_object_%s_l2_fills = %llu\n", name,
            (unsigned long long)stats.l2_fills);
    for (unsigned victim = 0; victim < OBJECT_CLASS_COUNT; ++victim)
      fprintf(fout, "vm_l2_tlb_replacement_incoming_%s_victim_%s = %llu\n",
              name,
              object_class_name(static_cast<object_class>(victim)),
              (unsigned long long)m_stats.l2_replacement_matrix[object][victim]);
  }
  fprintf(fout, "vm_object_attribution_conservation_pass = %u\n",
          object_attribution_conserves() ? 1U : 0U);
}

}  // namespace vm_translation
