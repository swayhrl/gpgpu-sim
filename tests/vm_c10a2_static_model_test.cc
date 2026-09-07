#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

static const char *kRegistration = "/tmp/vm_c10a2_registration.tsv";
static const uint64_t kPage = 64ULL * 1024ULL;

static void header(std::ofstream *out, unsigned asid, unsigned epoch) {
  *out << "M4B_WEIGHT_SEGMENT_REGISTRATION_V2\n";
  *out << "roi\tunit\n";
  *out << "source_sha256\t"
       << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  *out << "archive_sha256\t"
       << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  *out << "object_map_sha256\t"
       << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  *out << "provisioned_asid\t" << asid << "\n";
  *out << "epoch\t" << epoch << "\n";
}

static void descriptor(std::ofstream *out, unsigned asid, unsigned epoch,
                       uint64_t va_base, uint64_t va_limit, uint64_t pa_base,
                       unsigned read_only, unsigned mapping_class) {
  *out << "descriptor\t" << asid << "\t" << epoch << "\t" << va_base
       << "\t" << va_limit << "\t" << pa_base << "\t" << read_only
       << "\t" << mapping_class << "\n";
}

static vm_translation::translation_config config(unsigned sms,
                                                 unsigned l2_mode,
                                                 const char *map) {
  return vm_translation::translation_config(
      sms, kPage, vm_translation::tlb_config(2, 2, 1),
      vm_translation::tlb_config(32, 16, 1), 4, 4, 1, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 1, 1, "",
      l2_mode, vm_translation::segment_config(true, 8, 5, map));
}

static void expect_rejection(vm_translation::segment_registration_status status) {
  vm_translation::weight_segment_map map(kRegistration);
  assert(!map.enabled() && !map.registration_accepted() && map.size() == 0);
  assert(map.registration_status() == status);
  vm_translation::translation_controller controller(
      config(2, vm_translation::L2_TLB_STANDARD, kRegistration));
  assert(!controller.begin_segment_install() && !controller.segment_active());
}

int main() {
  // Nine valid extents are a legal driver transaction that architecture
  // rejects atomically, rather than an assert or eight live prefix entries.
  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    for (unsigned index = 0; index < 9; ++index)
      descriptor(&out, 7, 11, 16 + index, 16 + index, 256 + index, 1, 0);
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_CAPACITY);

  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    descriptor(&out, 7, 11, 16, 17, 256, 1, 0);
    descriptor(&out, 7, 11, 17, 18, 257, 1, 0);
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_OVERLAP);

  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    descriptor(&out, 7, 11, 32, 32, 256, 1, 0);
    descriptor(&out, 7, 11, 16, 16, 512, 1, 0);
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_UNSORTED);

  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    descriptor(&out, 7, 11, 16, 16, 256, 0, 0);
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_RIGHTS);

  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    descriptor(&out, 7, 11, 16, 16, 256, 1, 1);
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_MAPPING_CLASS);

  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    descriptor(&out, 8, 11, 16, 16, 256, 1, 0);
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_ASID_EPOCH);

  {
    std::ofstream out(kRegistration);
    header(&out, 7, 0);
    descriptor(&out, 7, 0, 16, 16, 256, 1, 0);
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_ASID_EPOCH);

  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    descriptor(&out, 7, 11, 17, 16, 256, 1, 0);
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_EXTENT);

  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    out << "descriptor\t7\t11\t16\t16\t256\t1\n";
  }
  expect_rejection(vm_translation::SEGMENT_REGISTRATION_REJECTED_EXTENT);

  // A valid registration is staged into two independent local replicas. A
  // partial ack is not active; revoke clears both before ASID reuse.
  {
    std::ofstream out(kRegistration);
    header(&out, 7, 11);
    descriptor(&out, 7, 11, 16, 17, 256, 1, 0);
  }
  vm_translation::translation_controller controller(
      config(2, vm_translation::L2_TLB_SUBENTRY_16, kRegistration));
  assert(controller.begin_segment_install());
  assert(controller.segment_lifecycle() ==
         vm_translation::SEGMENT_LIFECYCLE_INSTALLING);
  assert(controller.acknowledge_segment_install(0));
  assert(!controller.segment_active());
  assert(controller.acknowledge_segment_install(1));
  assert(controller.segment_active() && controller.segment_active_asid() == 7 &&
         controller.segment_active_epoch() == 11);
  assert(controller.begin_segment_revoke());
  assert(!controller.segment_active());
  assert(controller.acknowledge_segment_revoke(0));
  assert(controller.acknowledge_segment_revoke(1));
  assert(controller.segment_lifecycle() ==
         vm_translation::SEGMENT_LIFECYCLE_INACTIVE);

  // Shootdown advances conventional translation generation independently of
  // the Segment epoch and clears both exact/sub-entry residency domains.
  const vm_translation::translation_key key(7, 16, kPage);
  const uint64_t generation = controller.translation_generation(7);
  controller.flush_translation_asid(7);
  assert(controller.translation_generation(7) == generation + 1);

  vm_translation::translation_config fair;
  assert(vm_translation::configure_fair_arm(
             &fair, vm_translation::FAIR_ARM_F1_SUBENTRY_G96) &&
         fair.valid() && fair.l2.sets() == 6);
  vm_translation::translation_config f7(
      config(35, vm_translation::L2_TLB_STANDARD, kRegistration));
  assert(vm_translation::configure_fair_arm(
             &f7, vm_translation::FAIR_ARM_F7_SEGMENT_EXACT_E320) &&
         f7.valid() && f7.l2.entries == 320 && f7.l2.sets() == 20);
  vm_translation::translation_config f8(
      config(35, vm_translation::L2_TLB_STANDARD, kRegistration));
  assert(vm_translation::configure_fair_arm(
             &f8, vm_translation::FAIR_ARM_F8_SEGMENT_SUBENTRY_G32) &&
         f8.valid() && f8.l2.sets() == 2);
  assert(!vm_translation::configure_fair_arm(
      &fair, vm_translation::FAIR_ARM_F5_BLOCKED_PHYSICAL_PWC));
  assert(!vm_translation::configure_fair_arm(
      &fair, vm_translation::FAIR_ARM_H0_HISTORICAL_UNFAIR));
  assert(remove(kRegistration) == 0);
  printf("vm_c10a2_static_model_test PASS\n");
  return 0;
}
