#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

static const char *kSegmentMap = "/tmp/vm_c14_segment_race_map.tsv";
static const uint64_t kPage = 64ULL * 1024ULL;

static void write_map() {
  std::ofstream out(kSegmentMap);
  assert(out.good());
  out << "M4B_WEIGHT_SEGMENT_MAP_V1\n";
  out << "roi\tc14_race\n";
  out << "source_sha256\t"
      << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  out << "archive_sha256\t"
      << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  out << "object_map_sha256\t"
      << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  out << "segment\tWEIGHT\t0x10000\t0x1ffff\n";
}

static vm_translation::translation_config config(bool telemetry) {
  return vm_translation::translation_config(
      1, kPage, vm_translation::tlb_config(1, 1, 1),
      vm_translation::tlb_config(4, 4, 1), 8, 8, 1, 4,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 5, 3, "",
      vm_translation::L2_TLB_STANDARD,
      vm_translation::segment_config(true, 1, 2, kSegmentMap),
      vm_translation::FAIR_ARM_MANUAL, telemetry);
}

static void activate(vm_translation::translation_controller *controller) {
  assert(controller->begin_segment_install());
  assert(controller->acknowledge_segment_install(0));
  assert(controller->segment_active());
}

static uint64_t drive(vm_translation::translation_controller *controller,
                      uint64_t va, uint64_t uid, uint64_t first_cycle,
                      vm_translation::translation_source *source) {
  uint64_t pa = 0;
  for (uint64_t cycle = first_cycle; cycle < first_cycle + 128; ++cycle) {
    const vm_translation::lookup_result result =
        controller->translate(0, 0, va, 32, cycle, uid, &pa, source);
    if (result == vm_translation::READY) return cycle;
    assert(result == vm_translation::TRANSLATION_PENDING ||
           result == vm_translation::L1_PORT_STALL ||
           result == vm_translation::L2_PORT_STALL);
    controller->cycle(cycle);
  }
  assert(false && "C14 directed request did not complete");
  return 0;
}

struct run_result {
  uint64_t segment_ready;
  uint64_t fallback_ready;
  vm_translation::translation_stats stats;
};

static run_result run(bool telemetry) {
  vm_translation::translation_controller controller(config(telemetry));
  activate(&controller);
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  const uint64_t segment_ready = drive(&controller, 0x10020, 1, 0, &source);
  assert(source == vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  const uint64_t fallback_ready =
      drive(&controller, 0x50020, 2, segment_ready + 1, &source);
  assert(source == vm_translation::TRANSLATION_SOURCE_PTW);
  assert(controller.invariants_hold());
  return run_result{segment_ready, fallback_ready, controller.stats()};
}

int main() {
  write_map();
  const run_result off = run(false);
  const run_result on = run(true);

  // Counter-only telemetry must preserve every modeled timing/resource result.
  assert(off.segment_ready == on.segment_ready);
  assert(off.fallback_ready == on.fallback_ready);
  assert(off.stats.completed == on.stats.completed);
  assert(off.stats.l1_lookup_launches == on.stats.l1_lookup_launches);
  assert(off.stats.l2_lookup_launches == on.stats.l2_lookup_launches);
  assert(off.stats.mshr_allocations == on.stats.mshr_allocations);
  assert(off.stats.walk_starts == on.stats.walk_starts);
  assert(off.stats.c14_segment_race_admissions == 0);

  assert(on.stats.c14_segment_race_admissions == 2);
  assert(on.stats.c14_segment_race_segment_winners == 1);
  assert(on.stats.c14_segment_race_dual_miss_fallbacks == 1);
  assert(on.stats.c14_segment_race_segment_winner_l1_port_consumed == 1);
  assert(on.stats.c14_segment_race_segment_winner_l1_not_completed == 1);
  assert(on.stats.c14_segment_race_segment_winner_l1_late_discard == 1);
  assert(on.stats.c14_segment_race_segment_winner_l2_not_issued == 1);
  assert(on.stats.c14_segment_race_segment_winner_mshr_not_allocated == 1);
  assert(on.stats.c14_segment_race_segment_winner_ptw_not_started == 1);
  assert(on.stats.c14_segment_race_segment_winner_pte_not_issued == 1);
  assert(on.stats.c14_segment_race_ptw_winners == 1);
  assert(on.stats.c14_segment_race_segment_latency_samples == 1);
  assert(on.stats.c14_segment_race_ptw_wake_latency_samples == 1);
  assert(remove(kSegmentMap) == 0);
  printf("vm_c14_segment_race_telemetry_test PASS\n");
  return 0;
}
