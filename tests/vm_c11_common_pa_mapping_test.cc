#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

// C11 directed proof: a V2 driver registration owns the conventional PA map
// even when a fair arm disables Segment.  No object map is supplied.
static const uint64_t kPage = 64ULL * 1024ULL;
static const unsigned kAsid = 7;
struct allocation {
  const char *roi;
  const char *path;
  uint64_t vpn;
  uint64_t ppn;
};

// These are actual C11 full-page bases derived from the two frozen runtime
// allocation sidecars and C5_MODELED_PA_HIGH_UNUSED_BIT_V1, not toy VPNs.
static const allocation kAllocations[] = {
    {"prefill", "/tmp/vm_c11_common_pa_prefill.tsv", 2144968192ULL,
     5366193664ULL},
    {"decode1", "/tmp/vm_c11_common_pa_decode1.tsv", 2139014656ULL,
     5360240128ULL},
};

static void write_registration(const allocation &input) {
  std::ofstream out(input.path);
  assert(out.good());
  out << "M4B_WEIGHT_SEGMENT_REGISTRATION_V2\n";
  out << "roi\t" << input.roi << "\n";
  out << "source_sha256\t"
      << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  out << "archive_sha256\t"
      << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  out << "object_map_sha256\t"
      << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  out << "provisioned_asid\t7\n";
  out << "epoch\t1\n";
  out << "descriptor\t7\t1\t" << input.vpn << "\t"
      << input.vpn + 1 << "\t" << input.ppn << "\t1\t0\n";
}

static vm_translation::translation_config seed(unsigned sms, unsigned lseg,
                                               const allocation &input) {
  return vm_translation::translation_config(
      sms, kPage, vm_translation::tlb_config(4, 4, 1),
      vm_translation::tlb_config(800, 16, 1), 8, 8, 1, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 0, 0, "",
      vm_translation::L2_TLB_STANDARD,
      vm_translation::segment_config(true, 8, lseg, input.path));
}

static void drive(vm_translation::translation_controller *controller,
                  const allocation &input, uint64_t uid,
                  vm_translation::translation_source expected) {
  const uint64_t va = input.vpn * kPage + 37;
  uint64_t pa = 0;
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  for (uint64_t cycle = 0; cycle < 256; ++cycle) {
    const vm_translation::lookup_result result = controller->translate(
        0, kAsid, va, 32, cycle, uid, &pa, &source,
        vm_translation::TRANSLATION_ACCESS_READ);
    if (result == vm_translation::READY) {
      assert(pa == input.ppn * kPage + 37);
      assert(source == expected);
      assert(pa != va);
      assert(controller->quiescent_invariants_hold());
      return;
    }
    assert(result == vm_translation::TRANSLATION_PENDING ||
           result == vm_translation::L1_PORT_STALL ||
           result == vm_translation::L2_PORT_STALL);
    controller->cycle(cycle);
  }
  assert(false && "C11 directed mapping request did not finish");
}

static void run_conventional(const allocation &input) {
  vm_translation::translation_config config = seed(1, 5, input);
  assert(vm_translation::configure_fair_arm(
      &config, vm_translation::FAIR_ARM_F0_BASELINE_EXACT));
  assert(config.valid() && !config.segment.enabled &&
         config.segment.map_path == input.path);
  vm_translation::translation_controller controller(config);
  assert(!controller.stats().object_attribution_enabled);
  drive(&controller, input, 1, vm_translation::TRANSLATION_SOURCE_PTW);
  assert(controller.stats().segment_lookup_attempts == 0);
}

static void run_segment(const allocation &input, unsigned arm, unsigned uid) {
  vm_translation::translation_config config = seed(35, 5, input);
  assert(vm_translation::configure_fair_arm(&config, arm));
  assert(config.valid() && config.segment.enabled && config.segment.entries == 8);
  vm_translation::translation_controller controller(config);
  assert(controller.begin_segment_install());
  for (unsigned sid = 0; sid < 35; ++sid)
    assert(controller.acknowledge_segment_install(sid));
  assert(controller.segment_active());
  drive(&controller, input, uid,
        vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  assert(controller.stats().segment_hits == 1 &&
         controller.stats().segment_l2_suppressed == 1);
}

int main() {
  for (unsigned index = 0;
       index < sizeof(kAllocations) / sizeof(kAllocations[0]); ++index) {
    const allocation &input = kAllocations[index];
    write_registration(input);
    run_conventional(input);
    run_segment(input, vm_translation::FAIR_ARM_F7_SEGMENT_EXACT_E320,
                2 + 3 * index);
    run_segment(input, vm_translation::FAIR_ARM_F8_SEGMENT_SUBENTRY_G32,
                3 + 3 * index);
    assert(remove(input.path) == 0);
  }
  printf("vm_c11_common_pa_mapping_test PASS\n");
  return 0;
}
