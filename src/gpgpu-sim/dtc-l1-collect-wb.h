// OPT-COLLECT: two bounded result banks, with no data copy on stage transfer.
// This is a timing/ownership model, like service_entry; it does not simulate
// load values. Each bank is explicitly charged physical_lines * 128 data bytes.
// The PIB continues to own instruction/reference descriptors, NEVER data space.
#ifndef DTC_L1_COLLECT_WB_H
#define DTC_L1_COLLECT_WB_H

#include "dtc-l1-common.h"
#include <cstdio>

namespace dtc_l1 {

struct collect_wb_pipeline {
  bool collecting_valid = false;
  bool result_valid = false;
  uint64_t collecting_uid = 0;
  uint64_t result_uid = 0;
  unsigned collecting_sectors = 0;  // data actually read, retained in its bank
  unsigned result_sectors = 0;
  unsigned bank_capacity_sectors = 0;
  unsigned collecting_bank = 0;
  unsigned result_bank = 1;

  uint64_t edge_cycle = UINT64_MAX;
  uint64_t sampled_cycle = UINT64_MAX;
  uint64_t first_sample_cycle = UINT64_MAX;
  uint64_t last_wb_attempt_cycle = UINT64_MAX;
  unsigned cycle_sectors = 0;
  unsigned cycle_retires = 0;
  bool cycle_cacheable_retire = false;
  uint64_t samples = 0;
  uint64_t eligible_ready_sum = 0;
  uint64_t eligible_ready_cycles = 0;
  uint64_t idle_despite_eligible = 0;
  uint64_t overlap_cycles = 0;
  uint64_t result_occupancy_samples = 0;
  uint64_t collecting_occupancy_samples = 0;
  uint64_t result_backpressure_cycles = 0;
  uint64_t wb_arbitration_blocked_cycles = 0;
  uint64_t wb_attempts = 0;
  uint64_t wb_denied = 0;
  uint64_t sectors = 0;
  uint64_t retires = 0;
  uint64_t cacheable_retires = 0;
  uint64_t sidecar_retires = 0;
  unsigned peak_buffered_sectors = 0;
  std::array<uint64_t, 5> sector_hist{};
  std::array<uint64_t, 2> retire_hist{};

  void configure(unsigned physical_lines) {
    assert(physical_lines && physical_lines <= UINT32_MAX / 4);
    bank_capacity_sectors = physical_lines * 4;
  }
  void begin_edge(uint64_t cycle) {
    if (edge_cycle == cycle) return;
    assert(edge_cycle == UINT64_MAX || cycle > edge_cycle);
    edge_cycle = cycle;
    cycle_sectors = cycle_retires = 0;
    cycle_cacheable_retire = false;
  }
  void claim(uint64_t uid, unsigned total_sectors) {
    assert(!collecting_valid && total_sectors &&
           total_sectors <= bank_capacity_sectors);
    assert(!result_valid || uid != result_uid);
    collecting_valid = true;
    collecting_uid = uid;
    collecting_sectors = 0;
    check();
  }
  void consumed(uint64_t cycle, unsigned count) {
    begin_edge(cycle);
    assert(collecting_valid && count <= 4);
    collecting_sectors += count;
    cycle_sectors += count;
    sectors += count;
    assert(cycle_sectors <= 4);
    check();
  }
  void promote() {
    assert(collecting_valid && !result_valid && collecting_sectors);
    result_valid = true;
    result_uid = collecting_uid;
    result_sectors = collecting_sectors;
    // Swap bank roles, not their contents: no unmodeled bulk data transfer.
    std::swap(result_bank, collecting_bank);
    collecting_valid = false;
    collecting_uid = collecting_sectors = 0;
    check();
  }
  void attempted(uint64_t cycle, bool granted) {
    begin_edge(cycle);
    assert(result_valid && last_wb_attempt_cycle != cycle);
    last_wb_attempt_cycle = cycle;
    ++wb_attempts;
    if (!granted) ++wb_denied;
  }
  void retired(uint64_t cycle, uint64_t uid, bool cacheable) {
    begin_edge(cycle);
    assert(!cycle_retires);
    ++cycle_retires;
    ++retires;
    if (cacheable) {
      assert(result_valid && result_uid == uid);
      result_valid = false;
      result_uid = result_sectors = 0;
      cycle_cacheable_retire = true;
      ++cacheable_retires;
    } else {
      ++sidecar_retires;
    }
    // LDST can be invoked more than once per absolute core cycle. Collection
    // samples only once; a later same-cycle WB grant must still enter its bins.
    if (sampled_cycle == cycle) {
      assert(retire_hist[0]);
      --retire_hist[0];
      ++retire_hist[1];
      overlap_cycles += cacheable && cycle_sectors;
    }
    check();
  }
  void sample(uint64_t cycle, unsigned eligible_ready,
              unsigned eligible_collect, bool result_backpressure,
              bool wb_arbitration_blocked) {
    begin_edge(cycle);
    assert(sampled_cycle != cycle);
    sampled_cycle = cycle;
    if (first_sample_cycle == UINT64_MAX) first_sample_cycle = cycle;
    ++samples;
    eligible_ready_sum += eligible_ready;
    eligible_ready_cycles += eligible_ready != 0;
    idle_despite_eligible += eligible_collect && !cycle_sectors;
    overlap_cycles += cycle_cacheable_retire && cycle_sectors;
    result_occupancy_samples += result_valid;
    collecting_occupancy_samples += collecting_valid;
    result_backpressure_cycles += result_backpressure;
    wb_arbitration_blocked_cycles += wb_arbitration_blocked;
    ++sector_hist.at(cycle_sectors);
    ++retire_hist.at(cycle_retires);
    check();
  }
  void check() {
    assert(bank_capacity_sectors);
    assert(collecting_bank != result_bank && collecting_bank < 2 && result_bank < 2);
    assert(!collecting_valid || !result_valid || collecting_uid != result_uid);
    assert(collecting_sectors <= bank_capacity_sectors &&
           result_sectors <= bank_capacity_sectors);
    assert(collecting_valid || !collecting_sectors);
    assert(result_valid || !result_sectors);
    peak_buffered_sectors = std::max(peak_buffered_sectors,
                                   collecting_sectors + result_sectors);
  }
  void print(FILE *fp, unsigned sm) const {
    fprintf(fp, "DTC_COLLECT_WB_V1_SUMMARY sm=%u", sm);
#define DTC_CW_PRINT(name, value) \
    fprintf(fp, " " name "=%llu", (unsigned long long)(value))
    DTC_CW_PRINT("samples", samples);
    DTC_CW_PRINT("first_cycle", first_sample_cycle);
    DTC_CW_PRINT("last_cycle", sampled_cycle);
    DTC_CW_PRINT("eligible_ready_sum", eligible_ready_sum);
    DTC_CW_PRINT("eligible_ready_cycles", eligible_ready_cycles);
    DTC_CW_PRINT("collect_idle_eligible_cycles", idle_despite_eligible);
    DTC_CW_PRINT("overlap_cycles", overlap_cycles);
    DTC_CW_PRINT("result_occupancy_samples", result_occupancy_samples);
    DTC_CW_PRINT("collecting_occupancy_samples", collecting_occupancy_samples);
    DTC_CW_PRINT("result_backpressure_cycles", result_backpressure_cycles);
    DTC_CW_PRINT("wb_arbitration_blocked_cycles", wb_arbitration_blocked_cycles);
    DTC_CW_PRINT("wb_attempts", wb_attempts);
    DTC_CW_PRINT("wb_denied_attempts", wb_denied);
    DTC_CW_PRINT("collected_sectors", sectors);
    DTC_CW_PRINT("retires", retires);
    DTC_CW_PRINT("cacheable_retires", cacheable_retires);
    DTC_CW_PRINT("sidecar_retires", sidecar_retires);
    DTC_CW_PRINT("collecting_valid", collecting_valid);
    DTC_CW_PRINT("result_valid", result_valid);
    DTC_CW_PRINT("collecting_uid", collecting_uid);
    DTC_CW_PRINT("result_uid", result_uid);
    DTC_CW_PRINT("buffered_sectors", collecting_sectors + result_sectors);
    DTC_CW_PRINT("peak_buffered_sectors", peak_buffered_sectors);
    DTC_CW_PRINT("bank_capacity_bytes", uint64_t(bank_capacity_sectors) * 32);
    DTC_CW_PRINT("total_buffer_capacity_bytes", uint64_t(bank_capacity_sectors) * 64);
    for (unsigned i = 0; i < sector_hist.size(); ++i)
      fprintf(fp, " sector_cycles_%u=%llu", i, (unsigned long long)sector_hist[i]);
    for (unsigned i = 0; i < retire_hist.size(); ++i)
      fprintf(fp, " retire_cycles_%u=%llu", i, (unsigned long long)retire_hist[i]);
    DTC_CW_PRINT("last_cycle_sectors", cycle_sectors);
    DTC_CW_PRINT("last_cycle_retires", cycle_retires);
#undef DTC_CW_PRINT
    fputc('\n', fp);
  }
};
} // namespace dtc_l1
#endif
