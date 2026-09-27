#include "../oracle_elastic_residency.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>

using namespace oracle_elastic_residency;

static candidate row(unsigned index, bool invalid, bool protected_line,
                     uint64_t lru, uint64_t fifo);

struct synthetic_set {
  struct line {
    bool valid;
    bool protected_line;
    uint64_t lru;
    uint64_t fifo;
  };
  std::vector<line> lines;
  uint64_t occupancy;
  uint64_t quota;
  uint64_t clock;
  explicit synthetic_set(unsigned ways, uint64_t protected_quota)
      : lines(ways), occupancy(0), quota(protected_quota), clock(1) {
    for (unsigned i = 0; i < ways; ++i) {
      lines[i].valid = false;
      lines[i].protected_line = false;
      lines[i].lru = lines[i].fifo = 0;
    }
  }
  selection fill(bool target, bool enabled = true,
                 replacement_policy policy = POLICY_LRU) {
    std::vector<candidate> candidates;
    for (unsigned i = 0; i < lines.size(); ++i)
      candidates.push_back(row(i, !lines[i].valid, lines[i].protected_line,
                               lines[i].lru, lines[i].fifo));
    selection picked = choose_victim(candidates, target, occupancy, quota,
                                     policy, enabled);
    if (picked.status != SELECTION_FOUND) return picked;
    line &victim = lines[picked.index];
    if (victim.valid && victim.protected_line) {
      assert(occupancy > 0);
      --occupancy;
    }
    victim.valid = true;
    victim.protected_line = picked.protect_fill;
    victim.lru = victim.fifo = clock++;
    if (victim.protected_line) ++occupancy;
    unsigned counted = 0;
    for (unsigned i = 0; i < lines.size(); ++i)
      if (lines[i].valid && lines[i].protected_line) ++counted;
    assert(counted == occupancy && occupancy <= quota);
    return picked;
  }
  void invalidate(unsigned index) {
    if (lines[index].valid && lines[index].protected_line) {
      assert(occupancy > 0);
      --occupancy;
    }
    lines[index].valid = false;
    lines[index].protected_line = false;
    unsigned counted = 0;
    for (unsigned i = 0; i < lines.size(); ++i)
      if (lines[i].valid && lines[i].protected_line) ++counted;
    assert(counted == occupancy);
  }
  void hit(unsigned index, bool target) {
    (void)target;
    assert(lines[index].valid);
    lines[index].lru = clock++;
    // V1 never promotes an ordinary resident line on a target hit.
  }
};

static candidate row(unsigned index, bool invalid, bool protected_line,
                     uint64_t lru, uint64_t fifo) {
  candidate value = {index, invalid, protected_line, lru, fifo};
  return value;
}

static void assert_same_selection(const selection &left,
                                  const selection &right) {
  assert(left.status == right.status);
  assert(left.index == right.index);
  assert(left.victim == right.victim);
  assert(left.protect_fill == right.protect_fill);
  assert(left.quota_full == right.quota_full);
  assert(left.denial_reason == right.denial_reason);
}

static void assert_same_admission(const admission &left,
                                  const admission &right) {
  assert(left.protect_fill == right.protect_fill);
  assert(left.denial_reason == right.denial_reason);
}

static unsigned baseline_reference(const std::vector<candidate> &rows,
                                   replacement_policy policy) {
  unsigned invalid = static_cast<unsigned>(-1);
  unsigned valid = static_cast<unsigned>(-1);
  uint64_t timestamp = ~uint64_t(0);
  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].invalid) {
      invalid = rows[i].index;
    } else {
      uint64_t current = policy == POLICY_LRU ? rows[i].last_access
                                              : rows[i].allocation_time;
      if (current < timestamp) {
        timestamp = current;
        valid = rows[i].index;
      }
    }
  }
  return invalid != static_cast<unsigned>(-1) ? invalid : valid;
}

static void test_selection() {
  std::vector<candidate> rows;
  rows.push_back(row(0, false, false, 30, 10));
  rows.push_back(row(1, false, true, 5, 20));
  rows.push_back(row(2, false, false, 10, 30));

  selection disabled = choose_victim(rows, true, 1, 2, POLICY_LRU, false);
  assert(disabled.status == SELECTION_FOUND);
  assert(disabled.index == baseline_reference(rows, POLICY_LRU));
  assert(!disabled.protect_fill);

  selection target = choose_victim(rows, true, 1, 2, POLICY_LRU, true);
  assert(target.index == 2 && target.victim == VICTIM_NORMAL && target.protect_fill);
  selection target_fifo = choose_victim(rows, true, 1, 2, POLICY_FIFO, true);
  assert(target_fifo.index == 0 && target_fifo.victim == VICTIM_NORMAL);

  selection full = choose_victim(rows, true, 2, 2, POLICY_LRU, true);
  assert(full.index == 1 && full.victim == VICTIM_PROTECTED && full.protect_fill);
  assert(full.quota_full);

  selection ordinary = choose_victim(rows, false, 1, 2, POLICY_LRU, true);
  assert(ordinary.index == 2 && ordinary.victim == VICTIM_NORMAL && !ordinary.protect_fill);

  std::vector<candidate> all_protected;
  all_protected.push_back(row(7, false, true, 20, 1));
  all_protected.push_back(row(8, false, true, 10, 2));
  selection fallback = choose_victim(all_protected, false, 2, 2, POLICY_LRU, true);
  assert(fallback.index == 8 && fallback.victim == VICTIM_PROTECTED);

  std::vector<candidate> invalids;
  invalids.push_back(row(3, true, false, 0, 0));
  invalids.push_back(row(4, true, false, 0, 0));
  selection invalid = choose_victim(invalids, false, 0, 2, POLICY_LRU, true);
  assert(invalid.index == 4 && invalid.victim == VICTIM_INVALID);
  selection borrowing = choose_victim(invalids, false, 0, 2, POLICY_LRU, true);
  assert(borrowing.index == 4 && !borrowing.protect_fill);

  selection zero = choose_victim(rows, true, 0, 0, POLICY_LRU, true);
  assert(zero.index == baseline_reference(rows, POLICY_LRU) && !zero.protect_fill);

  std::vector<candidate> no_local_protected;
  no_local_protected.push_back(row(0, false, false, 1, 1));
  selection unresolved = choose_victim(no_local_protected, true, 4, 4, POLICY_LRU, true);
  assert(unresolved.status == SELECTION_FOUND && unresolved.index == 0);
  assert(!unresolved.protect_fill);
  assert(unresolved.denial_reason == QUOTA_FULL_NO_LOCAL_PROTECTED_VICTIM);

  std::vector<candidate> full_with_invalid;
  full_with_invalid.push_back(row(0, true, false, 0, 0));
  full_with_invalid.push_back(row(1, false, true, 1, 1));
  selection full_invalid = choose_victim(full_with_invalid, true, 4, 4, POLICY_LRU, true);
  assert(full_invalid.status == SELECTION_FOUND && full_invalid.index == 0);
  assert(!full_invalid.protect_fill);
  assert(full_invalid.denial_reason == QUOTA_FULL_BASELINE_INVALID_PRIORITY);

  std::vector<candidate> invalid_without_protected;
  invalid_without_protected.push_back(row(3, true, false, 0, 0));
  invalid_without_protected.push_back(row(4, true, false, 0, 0));
  selection denied_invalid = choose_victim(invalid_without_protected, true, 4, 4,
                                            POLICY_FIFO, true);
  assert(denied_invalid.index == 4 && !denied_invalid.protect_fill);
  assert(denied_invalid.denial_reason == QUOTA_FULL_BASELINE_INVALID_PRIORITY);

  std::vector<candidate> ordinary_only;
  ordinary_only.push_back(row(10, false, false, 30, 5));
  ordinary_only.push_back(row(11, false, false, 10, 20));
  selection denied_lru = choose_victim(ordinary_only, true, 4, 4,
                                       POLICY_LRU, true);
  selection denied_fifo = choose_victim(ordinary_only, true, 4, 4,
                                        POLICY_FIFO, true);
  assert(denied_lru.index == 11 && denied_fifo.index == 10);
  assert(!denied_lru.protect_fill && !denied_fifo.protect_fill);
  assert(denied_lru.denial_reason == QUOTA_FULL_NO_LOCAL_PROTECTED_VICTIM);
  assert(denied_fifo.denial_reason == QUOTA_FULL_NO_LOCAL_PROTECTED_VICTIM);

  std::vector<candidate> no_eligible;
  selection reservation_fail = choose_victim(no_eligible, true, 4, 4,
                                              POLICY_LRU, true);
  assert(reservation_fail.status == SELECTION_NO_ELIGIBLE);

  // A pending reservation consumes the remaining admission credit even before fill.
  selection pending_full = choose_victim(ordinary_only, true, 2, 2,
                                         POLICY_LRU, true);
  assert(!pending_full.protect_fill);
  assert(pending_full.denial_reason == QUOTA_FULL_NO_LOCAL_PROTECTED_VICTIM);

  admission admit_below = decide_fill_admission(true, 1, 2, true, false, true);
  assert(admit_below.protect_fill &&
         admit_below.denial_reason == ADMISSION_DENIAL_NONE);
  admission deny_invalid = decide_fill_admission(true, 2, 2, true, false, true);
  assert(!deny_invalid.protect_fill &&
         deny_invalid.denial_reason == QUOTA_FULL_BASELINE_INVALID_PRIORITY);
  admission transfer = decide_fill_admission(true, 2, 2, false, true, true);
  assert(transfer.protect_fill && transfer.denial_reason == ADMISSION_DENIAL_NONE);
  admission deny_normal = decide_fill_admission(true, 2, 2, false, false, true);
  assert(!deny_normal.protect_fill &&
         deny_normal.denial_reason == QUOTA_FULL_NO_LOCAL_PROTECTED_VICTIM);

  for (unsigned seed = 1; seed < 1000; ++seed) {
    std::vector<candidate> random_rows;
    unsigned state = seed;
    for (unsigned i = 0; i < 16; ++i) {
      state = state * 1103515245u + 12345u;
      random_rows.push_back(row(i, false, (state & 1) != 0,
                                (state >> 3) & 0xffff, (state >> 11) & 0xffff));
    }
    for (unsigned p = 0; p < 2; ++p) {
      replacement_policy policy = p ? POLICY_FIFO : POLICY_LRU;
      selection off = choose_victim(random_rows, (seed & 1) != 0, 5, 7,
                                    policy, false);
      assert(off.index == baseline_reference(random_rows, policy));
      selection off_again = choose_victim(random_rows, (seed & 1) != 0, 5, 7,
                                          policy, false);
      assert(off.index == off_again.index);  // diagnostics cannot affect selection.
    }
  }
}

static void test_synthetic_mechanism_sequences() {
  synthetic_set protection(2, 1);
  assert(protection.fill(true).protect_fill);
  assert(protection.occupancy == 1);
  selection ordinary_borrow = protection.fill(false);
  assert(ordinary_borrow.victim == VICTIM_INVALID);
  assert(protection.occupancy == 1);  // no static capacity stranding.

  synthetic_set denied_then_hit(2, 1);
  denied_then_hit.fill(true);
  selection denied = denied_then_hit.fill(true);
  assert(denied.victim == VICTIM_INVALID && !denied.protect_fill);
  assert(denied.denial_reason == QUOTA_FULL_BASELINE_INVALID_PRIORITY);
  assert(denied_then_hit.occupancy == 1);
  denied_then_hit.hit(denied.index, true);
  assert(!denied_then_hit.lines[denied.index].protected_line);
  assert(denied_then_hit.occupancy == 1);
  denied_then_hit.invalidate(denied.index);
  assert(denied_then_hit.occupancy == 1);

  synthetic_set target_competition(2, 2);
  target_competition.fill(true);
  target_competition.fill(true);
  assert(target_competition.occupancy == 2);
  selection target_replacement = target_competition.fill(true);
  assert(target_replacement.victim == VICTIM_PROTECTED);
  assert(target_competition.occupancy == 2);

  synthetic_set bounded_fallback(2, 2);
  bounded_fallback.fill(true);
  bounded_fallback.fill(true);
  selection fallback = bounded_fallback.fill(false);
  assert(fallback.victim == VICTIM_PROTECTED);
  assert(bounded_fallback.occupancy == 1);
  bounded_fallback.invalidate(fallback.index);
  assert(bounded_fallback.occupancy == 1);  // exactly-once: ordinary victim was not counted.
  bounded_fallback.invalidate(fallback.index);
  assert(bounded_fallback.occupancy == 1);
  bounded_fallback.invalidate(fallback.index == 0 ? 1 : 0);
  assert(bounded_fallback.occupancy == 0);

  synthetic_set borrowed(4, 2);
  for (unsigned i = 0; i < 4; ++i) borrowed.fill(false);
  assert(borrowed.occupancy == 0);
  for (unsigned i = 0; i < 100; ++i) {
    selection result = borrowed.fill(false);
    assert(result.status == SELECTION_FOUND);
    assert(borrowed.occupancy == 0);
  }

  synthetic_set disabled(4, 2);
  for (unsigned i = 0; i < 100; ++i) {
    selection result = disabled.fill((i & 1) != 0, false);
    assert(result.status == SELECTION_FOUND && !result.protect_fill);
    assert(disabled.occupancy == 0);
  }
}

static void test_sidecar_and_quota() {
  const char *path = "/tmp/c16_oracle_elastic_sidecar_test.tsv";
  std::ofstream file(path, std::ios::binary);
  file << "ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1\t1\tMODELED_L2_GET_ADDR\n";
  file << "0x1000\t0x1100\tlayer0.up_proj.qweight\t7\n";
  file << "0x2000\t0x2200\tlayer1.up_proj.qweight\t8\n";
  file.close();
  std::string digest = oracle_config::sha256_file(path);
  assert(digest.size() == 64);
  oracle_config config;
  config.configure(true, 1000, 0, 128, 3, path, digest.c_str(), true);
  assert(config.global_line_budget() == 8);
  assert(config.quota_for_instance(0) == 3);
  assert(config.quota_for_instance(1) == 3);
  assert(config.quota_for_instance(2) == 2);
  unsigned target_class = 0;
  assert(config.lookup(0x1000, &target_class) && target_class == 7);
  assert(config.lookup(0x10ff, &target_class));
  assert(!config.lookup(0x1100, &target_class));
  assert(!config.lookup(0x1800, &target_class));
  assert(config.lookup(0x21ff, &target_class) && target_class == 8);

  oracle_config exact;
  exact.configure(true, 0, 10, 128, 3, path, digest.c_str(), false);
  assert(exact.quota_for_instance(0) == 4);
  assert(exact.quota_for_instance(1) == 3);
  assert(exact.quota_for_instance(2) == 3);
  oracle_config diagnostics_on;
  diagnostics_on.configure(true, 0, 10, 128, 3, path, digest.c_str(), true);
  assert(diagnostics_on.diagnostics_enabled());
  assert(diagnostics_on.global_line_budget() == exact.global_line_budget());
  for (unsigned i = 0; i < 3; ++i)
    assert(diagnostics_on.quota_for_instance(i) == exact.quota_for_instance(i));
  assert(diagnostics_on.lookup(0x1000) == exact.lookup(0x1000));

  bool mismatch = false;
  try {
    oracle_config bad;
    bad.configure(true, 1000, 0, 128, 3, path,
                  "0000000000000000000000000000000000000000000000000000000000000000", false);
  } catch (const std::runtime_error &) {
    mismatch = true;
  }
  assert(mismatch);

  bool both_quota_inputs = false;
  try {
    oracle_config bad;
    bad.configure(true, 1000, 10, 128, 3, path, digest.c_str(), false);
  } catch (const std::runtime_error &) {
    both_quota_inputs = true;
  }
  assert(both_quota_inputs);

  oracle_config disabled;
  disabled.configure(false, 0, 0, 128, 3, "/missing/sidecar", "bad", true);
  assert(!disabled.enabled() && disabled.global_line_budget() == 0);
  remove(path);
}

static std::string write_m1f_sidecar(const char *path) {
  const uint64_t region_bytes = 33947648ULL;
  const uint64_t stride = region_bytes + 128;
  const uint64_t base = 0x100000000ULL;
  std::ofstream file(path, std::ios::binary);
  file << "ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1\t1\tMODELED_L2_GET_ADDR\n";
  for (unsigned target_class = 1; target_class <= 28; ++target_class) {
    const uint64_t begin = base + (target_class - 1) * stride;
    file << "0x" << std::hex << begin << "\t0x" << begin + region_bytes
         << std::dec << "\tlayer" << target_class - 1
         << ".up_proj.qweight\t" << target_class << "\n";
  }
  file.close();
  return oracle_config::sha256_file(path);
}

static void test_m1f_stable_admission() {
  assert(std::string(m1f_hash_version()) ==
         "C16_M1F_STABLE_ADMISSION_HASH_V1");
  assert(m1f_hash_seed() == 0x6a09e667f3bcc908ULL);
  assert(m1f_b16_threshold() == 0x0484baf3b723b966ULL);
  assert(m1f_integer_threshold(131072, 7426048) == m1f_b16_threshold());
  for (unsigned target_class = 1; target_class <= 28; ++target_class) {
    for (uint64_t line = 0; line < 256; ++line) {
      assert(!m1f_stable_selected(target_class, line, 0, 1));
      assert(m1f_stable_selected(target_class, line, 1, 1));
      const bool first =
          m1f_stable_selected(target_class, line, 131072, 7426048);
      const bool retry =
          m1f_stable_selected(target_class, line, 131072, 7426048);
      assert(first == retry);
    }
  }

  const char *path = "/tmp/c16_oracle_m1f_sidecar_test.tsv";
  const std::string digest = write_m1f_sidecar(path);
  oracle_config m1;
  m1.configure(true, 0, 131072, 128, 16, path, digest.c_str(), false,
               false);
  oracle_config m1f;
  m1f.configure(true, 0, 131072, 128, 16, path, digest.c_str(), true,
                true);
  oracle_config m1f_decode_replay;
  m1f_decode_replay.configure(true, 0, 131072, 128, 16, path,
                              digest.c_str(), false, true);
  assert(m1f.m1f_stable_admission_enabled());
  assert(m1f.m1f_threshold() == m1f_b16_threshold());
  assert(m1f.m1f_seed() == m1f_hash_seed());

  const uint64_t region_bytes = 33947648ULL;
  const uint64_t stride = region_bytes + 128;
  const uint64_t base = 0x100000000ULL;
  uint64_t selected_total = 0;
  for (unsigned target_class = 1; target_class <= 28; ++target_class) {
    const uint64_t begin = base + (target_class - 1) * stride;
    uint64_t selected_class = 0;
    for (uint64_t line = 0; line < 265216; ++line) {
      const uint64_t address = begin + line * 128;
      unsigned observed_class = 0;
      uint64_t observed_line = ~uint64_t(0);
      uint64_t hash = 0;
      assert(m1.protection_eligible(address, &observed_class,
                                    &observed_line, &hash));
      assert(observed_class == target_class && observed_line == line);
      const bool selected = m1f.protection_eligible(
          address, &observed_class, &observed_line, &hash);
      assert(selected == (hash < m1f_b16_threshold()));
      assert(selected == m1f.protection_eligible(address));  // retry
      assert(selected == m1f.protection_eligible(address + 32));
      assert(selected == m1f.protection_eligible(address + 64));
      assert(selected == m1f.protection_eligible(address + 96));
      assert(selected == m1f_decode_replay.protection_eligible(address));
      if (selected) ++selected_class;
    }
    // Qualification checks distribution without changing the frozen seed.
    const uint64_t expected = 131072 / 28;
    assert(selected_class > expected - 600 && selected_class < expected + 600);
    selected_total += selected_class;
  }
  assert(selected_total > 131072 - 2000 && selected_total < 131072 + 2000);

  // M1F OFF uses the exact target-bearing M1 decision inputs.  Fraction one
  // is the same decision, while fraction zero takes the ordinary path and
  // cannot produce a protected admission.
  std::vector<candidate> directed;
  directed.push_back(row(0, false, false, 30, 10));
  directed.push_back(row(1, false, true, 5, 20));
  directed.push_back(row(2, false, false, 10, 30));
  const bool m1_off_target = m1.protection_eligible(base);
  assert(m1_off_target && m1.lookup(base));
  const selection m1_reference =
      choose_victim(directed, true, 1, 2, POLICY_LRU, true);
  const selection m1f_off =
      choose_victim(directed, m1_off_target, 1, 2, POLICY_LRU, true);
  assert_same_selection(m1_reference, m1f_off);
  const admission m1_admission =
      decide_fill_admission(true, 1, 2, false, false, true);
  const admission m1f_off_admission =
      decide_fill_admission(m1_off_target, 1, 2, false, false, true);
  assert_same_admission(m1_admission, m1f_off_admission);

  const bool fraction_one = m1f_stable_selected(1, 0, 1, 1);
  assert(fraction_one);
  assert_same_selection(
      m1_reference,
      choose_victim(directed, fraction_one, 1, 2, POLICY_LRU, true));
  assert_same_admission(
      m1_admission,
      decide_fill_admission(fraction_one, 1, 2, false, false, true));

  const bool fraction_zero = m1f_stable_selected(1, 0, 0, 1);
  assert(!fraction_zero);
  const selection ordinary_reference =
      choose_victim(directed, false, 1, 2, POLICY_LRU, true);
  assert_same_selection(
      ordinary_reference,
      choose_victim(directed, fraction_zero, 1, 2, POLICY_LRU, true));
  const admission zero_admission =
      decide_fill_admission(fraction_zero, 1, 2, false, false, true);
  assert(!zero_admission.protect_fill);
  assert(zero_admission.denial_reason == ADMISSION_DENIAL_NONE);

  bool wrong_quota = false;
  try {
    oracle_config invalid;
    invalid.configure(true, 0, 65536, 128, 16, path, digest.c_str(), false,
                      true);
  } catch (const std::runtime_error &) {
    wrong_quota = true;
  }
  assert(wrong_quota);
  bool mechanism_off = false;
  try {
    oracle_config invalid;
    invalid.configure(false, 0, 0, 128, 16, path, digest.c_str(), false,
                      true);
  } catch (const std::runtime_error &) {
    mechanism_off = true;
  }
  assert(mechanism_off);
  remove(path);
}

struct toy_protected_pool {
  struct entry {
    unsigned target_class;
    unsigned line;
    uint64_t last_access;
  };
  size_t capacity;
  uint64_t clock;
  std::vector<entry> entries;
  explicit toy_protected_pool(size_t value) : capacity(value), clock(1) {}

  bool access(unsigned target_class, unsigned line, bool admit) {
    for (size_t i = 0; i < entries.size(); ++i) {
      if (entries[i].target_class == target_class &&
          entries[i].line == line) {
        entries[i].last_access = clock++;
        return true;
      }
    }
    if (!admit) return false;
    entry incoming = {target_class, line, clock++};
    if (entries.size() < capacity) {
      entries.push_back(incoming);
    } else {
      size_t victim = 0;
      for (size_t i = 1; i < entries.size(); ++i)
        if (entries[i].last_access < entries[victim].last_access) victim = i;
      entries[victim] = incoming;
    }
    return false;
  }

  bool contains(unsigned target_class, unsigned line) const {
    for (size_t i = 0; i < entries.size(); ++i)
      if (entries[i].target_class == target_class &&
          entries[i].line == line)
        return true;
    return false;
  }
};

static void test_toy_scan_survival_comparison() {
  toy_protected_pool m1(4);
  toy_protected_pool class_one(2);
  toy_protected_pool class_two(2);
  toy_protected_pool m1f(4);
  unsigned m1_next_round_hits = 0;
  unsigned quota_next_round_hits = 0;
  unsigned m1f_next_round_hits = 0;
  unsigned selected_lines = 0;
  for (unsigned round = 0; round < 2; ++round) {
    for (unsigned target_class = 1; target_class <= 2; ++target_class) {
      for (unsigned line = 0; line < 8; ++line) {
        const bool count = round == 1;
        if (m1.access(target_class, line, true) && count)
          ++m1_next_round_hits;
        toy_protected_pool &quota =
            target_class == 1 ? class_one : class_two;
        if (quota.access(target_class, line, true) && count)
          ++quota_next_round_hits;
        const bool selected =
            m1f_stable_selected(target_class, line, 1, 4);
        if (round == 0 && selected) ++selected_lines;
        if (m1f.access(target_class, line, selected) && count)
          ++m1f_next_round_hits;
      }
    }
  }
  assert(m1.entries.size() == 4);
  assert(class_one.entries.size() + class_two.entries.size() == 4);
  assert(m1f.entries.size() == selected_lines);
  assert(selected_lines == 3);  // Frozen seed result; it was not searched.
  assert(m1_next_round_hits == 0);
  assert(quota_next_round_hits == 0);
  assert(m1f_next_round_hits == selected_lines);
  assert(!m1.contains(1, 0));
  assert(!class_one.contains(1, 0));
  assert(m1f.contains(1, 4));
  std::cout << "M1F_TOY_SURVIVAL_PASS"
            << " m1_occupancy=" << m1.entries.size()
            << " class_quota_occupancy="
            << class_one.entries.size() + class_two.entries.size()
            << " m1f_occupancy=" << m1f.entries.size()
            << " m1_next_round_hits=" << m1_next_round_hits
            << " class_quota_next_round_hits=" << quota_next_round_hits
            << " m1f_next_round_hits=" << m1f_next_round_hits
            << " m1_old_address_survived=0"
            << " class_quota_old_address_survived=0"
            << " m1f_old_address_survived=1"
            << " old_address_class=1 old_address_line=4\n";
}

static void test_target_selection_accounting_identity() {
  diagnostic_counters counters;
  account_target_access(&counters, false, false, false, true);
  account_selection_filtered_admission(&counters, false, false, true);
  assert(counters.target_accesses == 0);
  assert(counters.selection_filtered_admissions == 0);

  account_target_access(&counters, true, false, false, true, 3);
  account_selection_filtered_admission(&counters, true, false, true, 3);
  assert(counters.target_accesses == 1);
  assert(counters.selected_target_accesses == 0);
  assert(counters.selection_filtered_target_accesses == 1);
  assert(counters.target_misses == 1);
  assert(counters.selection_filtered_admissions == 1);
  assert(counters.target_accesses_by_class[3] == 1);
  assert(counters.selection_filtered_target_accesses_by_class[3] == 1);
  assert(counters.selection_filtered_admissions_by_class[3] == 1);

  account_target_access(&counters, true, true, true, false, 3);
  account_selection_filtered_admission(&counters, true, true, false, 3);
  assert(counters.target_accesses == 2);
  assert(counters.selected_target_accesses == 1);
  assert(counters.selection_filtered_target_accesses == 1);
  assert(counters.target_hits == 1);
  assert(counters.target_misses == 1);
  assert(counters.selection_filtered_admissions == 1);
  assert(counters.target_accesses_by_class[3] == 2);
  assert(counters.selected_target_accesses_by_class[3] == 1);
}

static void test_m1f_activation_observer() {
  const char *path = "/tmp/c16_m1f_activation_observer.tsv";
  configure_m1f_activation_observer(path, 2, 0x1080);
  activation_record filtered = {
      7, 0x1080, 0x1080, 0, false, true, 3, 9,
      0xf000000000000000ULL, false, true, "MISS", 2, 17, "ORDINARY",
      false, false, false, "NONE"};
  observe_m1f_activation(filtered);
  activation_record selected = {
      8, 0x1100, 0x1100, 0, false, true, 3, 10,
      0x0000000000000001ULL, true, false, "MISS", 2, 18, "INVALID",
      true, true, false, "NONE"};
  observe_m1f_activation(selected);
  activation_record ordinary = {
      9, 0x2200, 0x2200, 4, true, false, 0, 0, 0, false, false,
      "MISS", 2, 19, "ORDINARY", false, false, false, "NONE"};
  observe_m1f_activation(ordinary);
  std::ifstream input(path);
  std::stringstream contents;
  contents << input.rdbuf();
  const std::string text = contents.str();
  assert(text.find("record_kind\trequest_uid") == 0);
  assert(text.find("EXACT_AND_TARGET_SAMPLE\t7\t4224") != std::string::npos);
  assert(text.find("\t3\t9\t0xf000000000000000\t0\t1\tMISS\t") !=
         std::string::npos);
  assert(text.find("TARGET_SAMPLE\t8\t4352") != std::string::npos);
  assert(text.find("\t1\t0\tMISS\t2\t18\tINVALID\t1\t1\t0\tNONE") !=
         std::string::npos);
  assert(text.find("NON_TARGET_SAMPLE\t9\t8704") != std::string::npos);
  configure_m1f_activation_observer(NULL, 0, 0);
  remove(path);
}

static void test_sha_and_exclusions() {
  const char *path = "/tmp/c16_oracle_elastic_sha_test";
  std::ofstream file(path, std::ios::binary);
  file << "abc";
  file.close();
  assert(oracle_config::sha256_file(path) ==
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  remove(path);
  const unsigned global_read = 6;
  assert(target_request_eligible(global_read, false, global_read));
  assert(!target_request_eligible(global_read, true, global_read));
  assert(!target_request_eligible(7, false, global_read));
}

static void test_address_observer() {
  const char *sidecar = "/tmp/c16_oracle_observer_sidecar.tsv";
  const char *log = "/tmp/c16_oracle_observer_test.tsv";
  std::ofstream file(sidecar, std::ios::binary);
  file << "ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1\t1\tMODELED_L2_GET_ADDR\n";
  file << "0x1000\t0x2000\ttarget\t4\n";
  file.close();
  std::string digest = oracle_config::sha256_file(sidecar);
  oracle_config config;
  config.configure(true, 0, 1, 128, 1, sidecar, digest.c_str(), false);
  configure_address_observer(log, 0, 0x1120, config.intervals());
  observe_address("TRACE_PARSED_LANE", 0x1120, -1, -1, false, 0);
  observe_address("MEM_ACCESS_CREATED", 0x1120, 1, 0, false, 7);
  observe_address("L2_ENTRY", 0x1120, 1, 0, false, 9);
  observe_address("ORACLE_LOOKUP", 0x1120, 1, 0, false, 9);
  observe_address("ORACLE_LOOKUP", 0x3000, 1, 0, false, 10);
  std::ifstream input(log);
  std::stringstream buffer;
  buffer << input.rdbuf();
  const std::string text = buffer.str();
  assert(text.find("TRACE_PARSED_LANE\t0\t4384\t0x1120") != std::string::npos);
  assert(text.find("MEM_ACCESS_CREATED\t7\t4384\t0x1120") != std::string::npos);
  assert(text.find("L2_ENTRY\t9\t4384\t0x1120") != std::string::npos);
  assert(text.find("ORACLE_LOOKUP\t9\t4384\t0x1120") != std::string::npos);
  remove(log);
  remove(sidecar);
}

int main() {
  test_selection();
  test_synthetic_mechanism_sequences();
  test_sidecar_and_quota();
  test_m1f_stable_admission();
  test_toy_scan_survival_comparison();
  test_target_selection_accounting_identity();
  test_m1f_activation_observer();
  test_sha_and_exclusions();
  test_address_observer();
  diagnostic_counters counters;
  assert(counters.target_accesses == 0 &&
         counters.selected_target_accesses == 0 &&
         counters.selection_filtered_target_accesses == 0 &&
         counters.selection_filtered_admissions == 0 &&
         counters.occupancy_current == 0);
  std::cout << "ORACLE_ELASTIC_QWEIGHT_RESIDENCY_POLICY_TEST_PASS\n";
  return 0;
}
