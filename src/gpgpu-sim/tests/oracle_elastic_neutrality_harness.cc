#include "../oracle_elastic_residency.h"

#include <assert.h>
#include <stdint.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace oracle_elastic_residency;

static uint64_t mix(uint64_t hash, uint64_t value) {
  hash ^= value;
  hash *= 1099511628211ULL;
  return hash;
}

struct line {
  bool valid;
  bool protected_line;
  uint64_t tag;
  uint64_t last;
  uint64_t allocation;
  line() : valid(false), protected_line(false), tag(0), last(0), allocation(0) {}
};

static unsigned baseline_victim(const std::vector<candidate> &rows) {
  unsigned invalid = static_cast<unsigned>(-1);
  unsigned valid = static_cast<unsigned>(-1);
  uint64_t oldest = ~uint64_t(0);
  for (size_t i = 0; i < rows.size(); ++i) {
    if (rows[i].invalid)
      invalid = rows[i].index;
    else if (rows[i].last_access < oldest) {
      oldest = rows[i].last_access;
      valid = rows[i].index;
    }
  }
  return invalid != static_cast<unsigned>(-1) ? invalid : valid;
}

int main(int argc, char **argv) {
  if (argc != 2) return 2;
  const std::string mode(argv[1]);
  const bool baseline = mode == "baseline";
  const bool enabled = mode == "enabled_diag_off" || mode == "enabled_diag_on";
  const bool diagnostics = mode == "enabled_diag_on";
  if (!baseline && mode != "feature_off" && !enabled) return 2;

  const unsigned sets = 4, ways = 4;
  std::vector<std::vector<line> > cache(sets, std::vector<line>(ways));
  uint64_t occupancy = 0, quota = 4, clock = 1;
  uint64_t accesses = 0, hits = 0, misses = 0, evictions = 0;
  uint64_t denied = 0, protected_fills = 0, diagnostics_events = 0;
  uint64_t victim_hash = 1469598103934665603ULL;
  uint64_t allocation_hash = victim_hash;
  uint64_t request_hash = victim_hash;

  for (unsigned i = 0; i < 512; ++i) {
    const uint64_t address = ((i * 17) % 64) * 128ULL;
    const bool target = ((i * 17 + 3) % 11) < 5;
    ++accesses;
    request_hash = mix(mix(request_hash, address), target ? 1 : 0);
    const unsigned set = (address / 128) % sets;
    const uint64_t tag = (address / 128) / sets;
    int hit_way = -1;
    for (unsigned way = 0; way < ways; ++way)
      if (cache[set][way].valid && cache[set][way].tag == tag) hit_way = way;
    if (hit_way >= 0) {
      ++hits;
      cache[set][hit_way].last = clock++;
      continue;  // target hits never promote.
    }
    ++misses;
    std::vector<candidate> candidates;
    for (unsigned way = 0; way < ways; ++way) {
      candidate row;
      row.index = way;
      row.invalid = !cache[set][way].valid;
      row.protected_line = cache[set][way].protected_line;
      row.last_access = cache[set][way].last;
      row.allocation_time = cache[set][way].allocation;
      candidates.push_back(row);
    }
    unsigned victim = 0;
    bool protect = false;
    admission_denial_reason reason = ADMISSION_DENIAL_NONE;
    if (baseline) {
      victim = baseline_victim(candidates);
    } else {
      selection selected = choose_victim(candidates, target, occupancy, quota,
                                         POLICY_LRU, enabled);
      assert(selected.status == SELECTION_FOUND);
      victim = selected.index;
      protect = selected.protect_fill;
      reason = selected.denial_reason;
      admission admitted = decide_fill_admission(
          target, occupancy, quota, !cache[set][victim].valid,
          cache[set][victim].protected_line, enabled);
      assert(admitted.protect_fill == protect && admitted.denial_reason == reason);
    }
    victim_hash = mix(victim_hash, set * ways + victim);
    if (cache[set][victim].valid) {
      ++evictions;
      if (cache[set][victim].protected_line) {
        assert(occupancy > 0);
        --occupancy;
      }
    }
    if (reason != ADMISSION_DENIAL_NONE) ++denied;
    if (protect) {
      ++occupancy;
      ++protected_fills;
    }
    assert(occupancy <= quota);
    cache[set][victim].valid = true;
    cache[set][victim].protected_line = protect;
    cache[set][victim].tag = tag;
    cache[set][victim].last = cache[set][victim].allocation = clock++;
    allocation_hash = mix(mix(allocation_hash, set * ways + victim), tag);
    if (diagnostics) ++diagnostics_events;
  }

  uint64_t output_hash = 1469598103934665603ULL;
  unsigned counted = 0;
  for (unsigned set = 0; set < sets; ++set)
    for (unsigned way = 0; way < ways; ++way) {
      const line &value = cache[set][way];
      output_hash = mix(output_hash, value.valid ? 1 : 0);
      output_hash = mix(output_hash, value.protected_line ? 1 : 0);
      output_hash = mix(output_hash, value.tag);
      if (value.valid && value.protected_line) ++counted;
    }
  assert(counted == occupancy);
  const uint64_t cycles = accesses + misses * 10;
  const uint64_t instructions = accesses;
  uint64_t signature = 1469598103934665603ULL;
  signature = mix(signature, cycles);
  signature = mix(signature, instructions);
  signature = mix(signature, hits);
  signature = mix(signature, misses);
  signature = mix(signature, evictions);
  signature = mix(signature, victim_hash);
  signature = mix(signature, allocation_hash);
  signature = mix(signature, request_hash);
  signature = mix(signature, output_hash);
  signature = mix(signature, occupancy);
  signature = mix(signature, denied);
  signature = mix(signature, protected_fills);

  std::cout << "functional_signature=" << signature << "\n";
  std::cout << "cycles=" << cycles << "\n";
  std::cout << "instructions=" << instructions << "\n";
  std::cout << "hits=" << hits << "\n";
  std::cout << "misses=" << misses << "\n";
  std::cout << "evictions=" << evictions << "\n";
  std::cout << "victim_hash=" << victim_hash << "\n";
  std::cout << "allocation_hash=" << allocation_hash << "\n";
  std::cout << "request_order_hash=" << request_hash << "\n";
  std::cout << "output_hash=" << output_hash << "\n";
  std::cout << "occupancy=" << occupancy << "\n";
  std::cout << "admission_denied=" << denied << "\n";
  std::cout << "protected_fills=" << protected_fills << "\n";
  std::cout << "termination=PASS\n";
  std::cout << "diagnostic_events=" << diagnostics_events << "\n";
  return 0;
}
