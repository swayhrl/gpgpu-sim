#include "oracle_elastic_residency.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace oracle_elastic_residency {
namespace {

uint32_t rotate_right(uint32_t value, unsigned bits) {
  return (value >> bits) | (value << (32 - bits));
}

const uint32_t kSha256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
    0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
    0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
    0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

void sha256_block(const unsigned char *block, uint32_t state[8]) {
  uint32_t words[64];
  for (unsigned i = 0; i < 16; ++i) {
    words[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
  }
  for (unsigned i = 16; i < 64; ++i) {
    uint32_t s0 = rotate_right(words[i - 15], 7) ^
                  rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
    uint32_t s1 = rotate_right(words[i - 2], 17) ^
                  rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
    words[i] = words[i - 16] + s0 + words[i - 7] + s1;
  }
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  for (unsigned i = 0; i < 64; ++i) {
    uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
    uint32_t choose = (e & f) ^ ((~e) & g);
    uint32_t t1 = h + s1 + choose + kSha256[i] + words[i];
    uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
    uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = s0 + majority;
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  state[0] += a; state[1] += b; state[2] += c; state[3] += d;
  state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

std::string sha256_bytes(const std::vector<unsigned char> &input) {
  std::vector<unsigned char> bytes(input);
  uint64_t bit_length = static_cast<uint64_t>(bytes.size()) * 8;
  bytes.push_back(0x80);
  while ((bytes.size() % 64) != 56) bytes.push_back(0);
  for (int shift = 56; shift >= 0; shift -= 8)
    bytes.push_back(static_cast<unsigned char>((bit_length >> shift) & 0xff));
  uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                       0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  for (size_t offset = 0; offset < bytes.size(); offset += 64)
    sha256_block(&bytes[offset], state);
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (unsigned i = 0; i < 8; ++i) out << std::setw(8) << state[i];
  return out.str();
}

uint64_t parse_u64(const std::string &text, const char *label) {
  if (text.empty() || text[0] == '-') throw std::runtime_error(std::string("invalid ") + label);
  char *end = 0;
  unsigned long long value = strtoull(text.c_str(), &end, 0);
  if (!end || *end != '\0') throw std::runtime_error(std::string("invalid ") + label);
  return static_cast<uint64_t>(value);
}

bool is_lower_hex64(const std::string &value) {
  if (value.size() != 64) return false;
  for (size_t i = 0; i < value.size(); ++i)
    if (!isdigit(value[i]) && !(value[i] >= 'a' && value[i] <= 'f')) return false;
  return true;
}

unsigned choose_oldest(const std::vector<candidate> &rows,
                       replacement_policy policy) {
  assert(!rows.empty());
  unsigned selected = 0;
  uint64_t timestamp = std::numeric_limits<uint64_t>::max();
  for (size_t i = 0; i < rows.size(); ++i) {
    uint64_t current = policy == POLICY_LRU ? rows[i].last_access
                                            : rows[i].allocation_time;
    if (current < timestamp) {
      timestamp = current;
      selected = static_cast<unsigned>(i);
    }
  }
  return selected;
}

selection found(const candidate &row, victim_kind kind, bool protect,
                bool quota_full) {
  selection result;
  result.status = SELECTION_FOUND;
  result.index = row.index;
  result.victim = kind;
  result.protect_fill = protect;
  result.quota_full = quota_full;
  return result;
}

}  // namespace

diagnostic_counters::diagnostic_counters()
    : target_accesses(0), target_hits(0), target_misses(0),
      protected_fills(0), protected_hits(0), target_normal_victims(0),
      target_protected_victims(0), normal_normal_victims(0),
      normal_fallback_protected_victims(0), quota_full_events(0),
      protected_protected_replacements(0), ordinary_borrowing_fills(0),
      ordinary_borrowing_current(0), ordinary_borrowing_max(0),
      contract_unrepresentable_events(0), occupancy_current(0),
      occupancy_max(0) {}

oracle_config::oracle_config()
    : m_enabled(false), m_diagnostics(false), m_global_lines(0), m_instances(0) {}

std::string oracle_config::sha256_file(const std::string &path) {
  std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
  if (!file) throw std::runtime_error("cannot open oracle interval sidecar");
  std::vector<unsigned char> bytes;
  char buffer[4096];
  while (file.good()) {
    file.read(buffer, sizeof(buffer));
    bytes.insert(bytes.end(), buffer, buffer + file.gcount());
  }
  return sha256_bytes(bytes);
}

void oracle_config::configure(bool enabled, uint64_t global_budget_bytes,
                              uint64_t exact_global_line_budget,
                              unsigned line_bytes, unsigned l2_instances,
                              const char *sidecar_path,
                              const char *accepted_sha256, bool diagnostics) {
  m_enabled = enabled;
  m_diagnostics = diagnostics;
  m_intervals.clear();
  m_quotas.clear();
  m_sidecar_sha256.clear();
  if (!enabled) {
    m_global_lines = 0;
    m_instances = l2_instances;
    return;
  }
  if (!line_bytes || !l2_instances) throw std::runtime_error("zero line size/L2 instances");
  if ((global_budget_bytes == 0) == (exact_global_line_budget == 0))
    throw std::runtime_error("enable requires exactly one nonzero quota input");
  m_global_lines = exact_global_line_budget
                       ? exact_global_line_budget
                       : (global_budget_bytes + line_bytes - 1) / line_bytes;
  m_instances = l2_instances;
  uint64_t quotient = m_global_lines / l2_instances;
  uint64_t remainder = m_global_lines % l2_instances;
  uint64_t sum = 0;
  for (unsigned i = 0; i < l2_instances; ++i) {
    uint64_t value = quotient + (i < remainder ? 1 : 0);
    m_quotas.push_back(value);
    sum += value;
  }
  if (sum != m_global_lines) throw std::runtime_error("quota distribution mismatch");
  if (!sidecar_path || !*sidecar_path || !accepted_sha256 || !*accepted_sha256)
    throw std::runtime_error("enabled oracle requires sidecar path and accepted SHA256");
  load_sidecar(sidecar_path, accepted_sha256);
}

uint64_t oracle_config::quota_for_instance(unsigned instance) const {
  if (!m_enabled) return 0;
  if (instance >= m_quotas.size()) throw std::runtime_error("L2 instance outside quota map");
  return m_quotas[instance];
}

void oracle_config::load_sidecar(const std::string &path,
                                 const std::string &accepted_sha) {
  if (!is_lower_hex64(accepted_sha)) throw std::runtime_error("invalid accepted sidecar SHA256");
  m_sidecar_sha256 = sha256_file(path);
  if (m_sidecar_sha256 != accepted_sha) throw std::runtime_error("oracle sidecar SHA256 mismatch");
  std::ifstream file(path.c_str());
  if (!file) throw std::runtime_error("cannot read oracle interval sidecar");
  std::string line;
  if (!std::getline(file, line) ||
      line != "ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1\t1\tMODELED_L2_GET_ADDR")
    throw std::runtime_error("oracle sidecar header/schema/namespace mismatch");
  unsigned line_number = 1;
  while (std::getline(file, line)) {
    ++line_number;
    if (line.empty()) continue;
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, '\t')) fields.push_back(field);
    if (fields.size() != 4) throw std::runtime_error("oracle sidecar row must have four fields");
    interval item;
    item.begin = parse_u64(fields[0], "interval begin");
    item.end = parse_u64(fields[1], "interval end");
    uint64_t target_class = parse_u64(fields[3], "target class");
    if (item.begin >= item.end || fields[2].empty() || target_class > 0xffffffffULL)
      throw std::runtime_error("invalid oracle interval row");
    item.allocation = fields[2];
    item.target_class = static_cast<unsigned>(target_class);
    if (!m_intervals.empty() && m_intervals.back().end > item.begin)
      throw std::runtime_error("oracle intervals unsorted or overlapping");
    m_intervals.push_back(item);
  }
  if (m_intervals.empty()) throw std::runtime_error("oracle sidecar has no intervals");
}

bool oracle_config::lookup(uint64_t address, unsigned *target_class) const {
  if (!m_enabled) return false;
  size_t low = 0, high = m_intervals.size();
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    const interval &item = m_intervals[mid];
    if (address < item.begin)
      high = mid;
    else if (address >= item.end)
      low = mid + 1;
    else {
      if (target_class) *target_class = item.target_class;
      return true;
    }
  }
  return false;
}

selection choose_victim(const std::vector<candidate> &eligible,
                        bool incoming_target, uint64_t occupancy,
                        uint64_t quota, replacement_policy policy,
                        bool mechanism_enabled) {
  selection none;
  if (eligible.empty()) return none;
  if (!mechanism_enabled || quota == 0) {
    for (size_t i = eligible.size(); i > 0; --i)
      if (eligible[i - 1].invalid) return found(eligible[i - 1], VICTIM_INVALID, false, false);
    unsigned selected = choose_oldest(eligible, policy);
    return found(eligible[selected], eligible[selected].protected_line ? VICTIM_PROTECTED : VICTIM_NORMAL,
                 false, false);
  }
  if (occupancy > quota) throw std::runtime_error("protected occupancy exceeds quota");
  const bool full = occupancy == quota;
  if (incoming_target && full) {
    for (size_t i = 0; i < eligible.size(); ++i) {
      if (eligible[i].invalid) {
        none.status = SELECTION_FROZEN_CONTRACT_UNREPRESENTABLE;
        none.quota_full = true;
        return none;
      }
    }
  }
  if (!incoming_target || !full) {
    for (size_t i = eligible.size(); i > 0; --i)
      if (eligible[i - 1].invalid)
        return found(eligible[i - 1], VICTIM_INVALID, incoming_target, full);
  }
  std::vector<candidate> normal, protected_rows;
  for (size_t i = 0; i < eligible.size(); ++i) {
    if (eligible[i].invalid) continue;
    (eligible[i].protected_line ? protected_rows : normal).push_back(eligible[i]);
  }
  if (incoming_target) {
    if (full) {
      if (protected_rows.empty()) {
        none.status = SELECTION_FROZEN_CONTRACT_UNREPRESENTABLE;
        none.quota_full = true;
        return none;
      }
      unsigned selected = choose_oldest(protected_rows, policy);
      return found(protected_rows[selected], VICTIM_PROTECTED, true, true);
    }
    if (!normal.empty()) {
      unsigned selected = choose_oldest(normal, policy);
      return found(normal[selected], VICTIM_NORMAL, true, false);
    }
    if (!protected_rows.empty()) {
      unsigned selected = choose_oldest(protected_rows, policy);
      return found(protected_rows[selected], VICTIM_PROTECTED, true, false);
    }
  } else {
    if (!normal.empty()) {
      unsigned selected = choose_oldest(normal, policy);
      return found(normal[selected], VICTIM_NORMAL, false, false);
    }
    if (!protected_rows.empty()) {
      unsigned selected = choose_oldest(protected_rows, policy);
      return found(protected_rows[selected], VICTIM_PROTECTED, false, false);
    }
  }
  return none;
}

bool target_request_eligible(unsigned access_type, bool is_write,
                             unsigned global_read_access_type) {
  return !is_write && access_type == global_read_access_type;
}

}  // namespace oracle_elastic_residency
