#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

// C10B-5: a bounded controller sanity suite. It validates realized fair-arm
// configurations and exact-once behavior; it deliberately makes no timing or
// performance comparison claim.
static const uint64_t kPage = 64ULL * 1024ULL;
static const char *kRegistration = "/tmp/vm_c10b_fair_arm_registration.tsv";

static void write_registration() {
  std::ofstream out(kRegistration);
  assert(out.good());
  out << "M4B_WEIGHT_SEGMENT_REGISTRATION_V2\n";
  out << "roi\tc10b_fair_sanity\n";
  out << "source_sha256\t"
      << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  out << "archive_sha256\t"
      << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  out << "object_map_sha256\t"
      << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  out << "provisioned_asid\t7\n";
  out << "epoch\t11\n";
  out << "descriptor\t7\t11\t16\t17\t256\t1\t0\n";
}

static vm_translation::translation_config seed(unsigned sms, unsigned lseg,
                                                bool segment) {
  return vm_translation::translation_config(
      sms, kPage, vm_translation::tlb_config(8, 8, 1),
      vm_translation::tlb_config(800, 16, 1), 16, 16, 2, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 0, 0, "",
      vm_translation::L2_TLB_STANDARD,
      segment ? vm_translation::segment_config(true, 8, lseg, kRegistration)
              : vm_translation::segment_config());
}

static void drive(vm_translation::translation_controller *controller,
                  unsigned asid, uint64_t va, uint64_t uid,
                  vm_translation::translation_source expected_source,
                  uint64_t expected_pa) {
  uint64_t pa = 0;
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  for (uint64_t cycle = 0; cycle < 256; ++cycle) {
    const vm_translation::lookup_result result = controller->translate(
        0, asid, va, 32, cycle, uid, &pa, &source,
        vm_translation::TRANSLATION_ACCESS_READ);
    if (result == vm_translation::READY) {
      assert(pa == expected_pa && source == expected_source);
      assert(controller->quiescent_invariants_hold());
      return;
    }
    assert(result == vm_translation::TRANSLATION_PENDING ||
           result == vm_translation::L1_PORT_STALL ||
           result == vm_translation::L2_PORT_STALL);
    controller->cycle(cycle);
  }
  assert(false && "bounded fair-arm request did not finish");
}

static void print_arm(const char *label,
                      vm_translation::translation_controller *controller) {
  printf("C10B_FAIR_SANITY_BEGIN %s\n", label);
  controller->print_stats(stdout);
  printf("C10B_FAIR_SANITY_END %s\n", label);
}

static void run_nonsegment(unsigned arm, const char *label) {
  vm_translation::translation_config config = seed(1, 5, false);
  assert(vm_translation::configure_fair_arm(&config, arm));
  assert(config.valid() && !config.segment.enabled);
  vm_translation::translation_controller controller(config);
  const uint64_t va = config.page_size + 19;
  drive(&controller, 0, va, arm, vm_translation::TRANSLATION_SOURCE_PTW, va);
  assert(controller.stats().completed == 1 &&
         controller.stats().mshr_entries_completed == 1);
  print_arm(label, &controller);
}

static void run_segment(unsigned arm, unsigned lseg, const char *label) {
  vm_translation::translation_config config = seed(35, lseg, true);
  assert(vm_translation::configure_fair_arm(&config, arm));
  assert(config.valid() && config.num_sms == 35 &&
         config.segment.lookup_latency == lseg && config.segment.entries == 8);
  vm_translation::translation_controller controller(config);
  assert(controller.begin_segment_install());
  for (unsigned sid = 0; sid < 35; ++sid)
    assert(controller.acknowledge_segment_install(sid));
  assert(controller.segment_active());
  const uint64_t hit_va = 16 * kPage + 9;
  drive(&controller, 7, hit_va, 100 + lseg,
        vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT, 256 * kPage + 9);
  assert(controller.stats().segment_hits == 1 &&
         controller.stats().segment_l2_suppressed == 1 &&
         controller.stats().completed == 1);
  assert(controller.begin_segment_revoke());
  for (unsigned sid = 0; sid < 35; ++sid)
    assert(controller.acknowledge_segment_revoke(sid));
  print_arm(label, &controller);
}

int main() {
  write_registration();
  run_nonsegment(vm_translation::FAIR_ARM_F0_BASELINE_EXACT, "F0");
  run_nonsegment(vm_translation::FAIR_ARM_F1_SUBENTRY_G96, "F1");
  run_nonsegment(vm_translation::FAIR_ARM_F2_EXACT_E688, "F2");
  run_nonsegment(vm_translation::FAIR_ARM_F3_EXACT_SWEEP, "F3_E800");
  run_nonsegment(vm_translation::FAIR_ARM_F4_EXACT_E1536, "F4");
  run_nonsegment(vm_translation::FAIR_ARM_F5_PHYSICAL_PWC, "F5");
  run_nonsegment(vm_translation::FAIR_ARM_F6_EXACT_2M_E848, "F6");
  run_nonsegment(vm_translation::FAIR_ARM_F9_EXACT_E656, "F9");
  const unsigned latencies[] = {5, 10, 20};
  for (unsigned index = 0; index < 3; ++index) {
    run_segment(vm_translation::FAIR_ARM_F7_SEGMENT_EXACT_E320,
                latencies[index], "F7");
    run_segment(vm_translation::FAIR_ARM_F8_SEGMENT_SUBENTRY_G32,
                latencies[index], "F8");
  }
  vm_translation::translation_config rejected = seed(1, 5, false);
  assert(!vm_translation::configure_fair_arm(
      &rejected, vm_translation::FAIR_ARM_H0_HISTORICAL_UNFAIR));
  assert(remove(kRegistration) == 0);
  printf("vm_c10b_fair_arm_sanity_test PASS\n");
  return 0;
}
