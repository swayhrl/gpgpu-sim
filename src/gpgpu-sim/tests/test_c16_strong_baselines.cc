#include "../c16_strong_baselines.h"

#include <assert.h>
#include <stdint.h>

#include <iostream>
#include <vector>

using namespace c16_strong_baselines;

static candidate row(unsigned index, bool invalid, bool priority,
                     unsigned rrpv, uint64_t lru) {
  candidate value = {index, invalid, priority, rrpv, lru, lru};
  return value;
}

static void test_priority() {
  std::vector<candidate> rows;
  rows.push_back(row(0, false, true, 0, 1));
  rows.push_back(row(1, false, false, 0, 10));
  rows.push_back(row(2, false, false, 0, 5));
  victim_result ordinary = choose_priority_victim(rows, true);
  assert(ordinary.found && ordinary.index == 2 &&
         ordinary.kind == VICTIM_ORDINARY);
  rows[1].priority = rows[2].priority = true;
  victim_result fallback = choose_priority_victim(rows, true);
  assert(fallback.found && fallback.index == 0 &&
         fallback.kind == VICTIM_PRIORITY);
  rows.push_back(row(3, true, false, 0, 0));
  rows.push_back(row(4, true, false, 0, 0));
  victim_result invalid = choose_priority_victim(rows, true);
  assert(invalid.found && invalid.index == 4 &&
         invalid.kind == VICTIM_INVALID);
  diagnostics state;
  assert(state.priority_resident == 0 && state.priority_pending == 0);
}

static void test_stable_selector() {
  assert(std::string(stable_hash_version()) ==
         "C16_M1F_STABLE_ADMISSION_HASH_V1");
  assert(stable_hash_seed() == 0x6a09e667f3bcc908ULL);
  assert(stable_hash_threshold() == 0x0484baf3b723b966ULL);
  assert(stable_hash(1, 1186) == 0xd92231f51d5c2efbULL);
  assert(stable_hash(15, 2) == 0xc69c317586b04f9eULL);
  assert(stable_hash(28, 1193) == 0xfcf49fc4e0bf282dULL);
  bool saw_selected = false, saw_filtered = false;
  for (uint64_t line = 0; line < 4096; ++line) {
    const bool first = stable_selected(7, line);
    assert(first == stable_selected(7, line));
    assert(first == (stable_hash(7, line) < stable_hash_threshold()));
    saw_selected |= first;
    saw_filtered |= !first;
  }
  assert(saw_selected && saw_filtered);
}

static void test_drrip() {
  unsigned srrip = 0, brrip = 0;
  for (unsigned set = 0; set < 2048; ++set) {
    leader_role role = drrip_leader_role(set, 2048);
    srrip += role == SRRIP_LEADER_SET;
    brrip += role == BRRIP_LEADER_SET;
  }
  assert(srrip == 32 && brrip == 32);
  for (unsigned slot = 0; slot < 64; ++slot)
    assert(drrip_leader_role(slot * 32, 2048) ==
           ((slot & 1) ? BRRIP_LEADER_SET : SRRIP_LEADER_SET));

  unsigned psel = 512;
  drrip_train_psel(SRRIP_LEADER_SET, &psel);
  assert(psel == 513);
  drrip_train_psel(BRRIP_LEADER_SET, &psel);
  assert(psel == 512);
  for (unsigned i = 0; i < 2000; ++i)
    drrip_train_psel(SRRIP_LEADER_SET, &psel);
  assert(psel == 1023);
  for (unsigned i = 0; i < 2000; ++i)
    drrip_train_psel(BRRIP_LEADER_SET, &psel);
  assert(psel == 0);

  uint64_t counter = 0;
  bool used_brrip = false;
  for (unsigned i = 0; i < 64; ++i) {
    const unsigned rrpv = drrip_insertion_rrpv(
        BRRIP_LEADER_SET, 0, &counter, &used_brrip);
    assert(used_brrip);
    assert(rrpv == ((i % 32 == 0) ? 2U : 3U));
  }
  assert(counter == 64);
  assert(drrip_insertion_rrpv(SRRIP_LEADER_SET, 1023, &counter,
                              &used_brrip) == 2);
  assert(!used_brrip && counter == 64);

  std::vector<candidate> rows;
  rows.push_back(row(0, false, false, 1, 0));
  rows.push_back(row(1, false, false, 2, 0));
  rows.push_back(row(2, false, false, 2, 0));
  victim_result aged = choose_drrip_victim(&rows);
  assert(aged.found && aged.index == 1 && aged.aging_rounds == 1);
  assert(rows[0].rrpv == 2 && rows[1].rrpv == 3 && rows[2].rrpv == 3);
  rows.push_back(row(7, true, false, 0, 0));
  rows.push_back(row(8, true, false, 0, 0));
  victim_result invalid = choose_drrip_victim(&rows);
  assert(invalid.index == 8 && invalid.kind == VICTIM_INVALID);
}

static void test_gpu_event_contract() {
  assert(event_promotes(EVENT_TRUE_VALID_SECTOR_HIT));
  assert(!event_promotes(EVENT_SAME_TAG_SECTOR_MISS));
  assert(!event_promotes(EVENT_HIT_RESERVED_MSHR_MERGE));
  assert(!event_promotes(EVENT_RESERVATION_FAIL_RETRY));
  assert(!event_promotes(EVENT_FILL));
  assert(!event_promotes(EVENT_WRITEBACK_HIT));
  assert(event_inserts(EVENT_NEW_LINE_ALLOCATION));
  assert(!event_inserts(EVENT_SAME_TAG_SECTOR_MISS));
  assert(!event_inserts(EVENT_HIT_RESERVED_MSHR_MERGE));
  assert(!event_inserts(EVENT_RESERVATION_FAIL_RETRY));
}

int main() {
  assert(policy_value_valid(POLICY_SHIP_SW));
  assert(!ship_sw_contract_complete());
  test_priority();
  test_stable_selector();
  test_drrip();
  test_gpu_event_contract();
  std::cout << "C16_STRONG_BASELINES_POLICY_TEST_PASS\n";
  return 0;
}
