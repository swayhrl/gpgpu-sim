#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <string>

#include "gpgpu-sim/memory_telemetry.h"
#include "gpgpu-sim/vm_translation.h"

static const uint64_t kPage = 64ULL * 1024ULL;

static vm_translation::translation_config config(bool telemetry) {
  return vm_translation::translation_config(
      1, kPage, vm_translation::tlb_config(1, 1, 1),
      vm_translation::tlb_config(4, 4, 1), 8, 8, 1, 4,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 2, 2, "",
      vm_translation::L2_TLB_STANDARD,
      vm_translation::segment_config(false, 0, 0, ""),
      vm_translation::FAIR_ARM_MANUAL, telemetry);
}

struct run_result {
  uint64_t ready_cycle;
  vm_translation::translation_stats stats;
};

static run_result run(bool telemetry) {
  vm_translation::translation_controller controller(config(telemetry));
  uint64_t pa = 0;
  for (uint64_t cycle = 0; cycle < 128; ++cycle) {
    const vm_translation::lookup_result result =
        controller.translate(0, 0, 0x50020, 32, cycle, 7, &pa);
    if (result == vm_translation::READY) {
      assert(pa == 0x50020);
      assert(controller.invariants_hold());
      return run_result{cycle, controller.stats()};
    }
    assert(result == vm_translation::TRANSLATION_PENDING ||
           result == vm_translation::L1_PORT_STALL ||
           result == vm_translation::L2_PORT_STALL);
    controller.cycle(cycle);
  }
  assert(false && "C14 criticality directed request did not complete");
  return run_result{0, vm_translation::translation_stats()};
}

int main() {
  const run_result off = run(false);
  const run_result on = run(true);

  // The controller-side occupancy sample is observational and must preserve
  // all externally visible translation timing and resource behavior.
  assert(off.ready_cycle == on.ready_cycle);
  assert(off.stats.completed == on.stats.completed);
  assert(off.stats.l1_lookup_launches == on.stats.l1_lookup_launches);
  assert(off.stats.l2_lookup_launches == on.stats.l2_lookup_launches);
  assert(off.stats.mshr_allocations == on.stats.mshr_allocations);
  assert(off.stats.walk_starts == on.stats.walk_starts);
  assert(off.stats.c14_criticality_pending_requester_samples == 0);
  assert(on.stats.c14_criticality_pending_requester_samples != 0);
  assert(on.stats.c14_criticality_pending_requester_high_watermark != 0);

  // The local-proxy text stream has explicit semantics and separates ready
  // events from data-admission gap samples.
  c14_translation_criticality_telemetry &telemetry =
      c14_translation_criticality_telemetry_instance();
  telemetry.configure(true);
  telemetry.record_head_blocked(M4C_DATA_WEIGHT);
  telemetry.record_translation_ready(M4C_DATA_WEIGHT,
                                     M4C_TRANSLATION_SEGMENT_HIT);
  telemetry.record_data_admission(M4C_DATA_WEIGHT,
                                  M4C_TRANSLATION_SEGMENT_HIT, 0);
  const char *path = "/tmp/vm_c14_criticality_telemetry.stats";
  FILE *out = fopen(path, "w");
  assert(out != 0);
  telemetry.print(out, "c14_directed");
  assert(fclose(out) == 0);
  std::ifstream in(path);
  const std::string text((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
  assert(text.find("LOCAL_LDST_HEAD_PROXY") != std::string::npos);
  assert(text.find("c14_translation_head_blocked_cycles_DATA_WEIGHT = 1") !=
         std::string::npos);
  assert(text.find(
             "c14_translation_ready_to_data_admission_same_cycle_DATA_WEIGHT_SEGMENT_HIT = 1") !=
         std::string::npos);
  assert(remove(path) == 0);
  printf("vm_c14_criticality_telemetry_test PASS\n");
  return 0;
}
