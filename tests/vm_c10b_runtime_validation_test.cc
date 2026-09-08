#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

// This is a directed C10B runtime validator, not a simulator replay.  It
// exercises the frozen C9 controls through the public controller interface.
static const char *kRegistration = "/tmp/vm_c10b_registration.tsv";
static const uint64_t kPage = 64ULL * 1024ULL;

static void write_registration(unsigned epoch = 11) {
  std::ofstream out(kRegistration);
  assert(out.good());
  out << "M4B_WEIGHT_SEGMENT_REGISTRATION_V2\n";
  out << "roi\tc10b_unit\n";
  out << "source_sha256\t"
      << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  out << "archive_sha256\t"
      << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  out << "object_map_sha256\t"
      << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  out << "provisioned_asid\t7\n";
  out << "epoch\t" << epoch << "\n";
  // Two read-only, non-identity physical extents.  No object map is present:
  // registration, not OBJECT_WEIGHT attribution, is the eligibility source.
  out << "descriptor\t7\t" << epoch << "\t16\t17\t256\t1\t0\n";
  out << "descriptor\t7\t" << epoch << "\t32\t32\t512\t1\t0\n";
}

static vm_translation::translation_config candidate_config(
    unsigned sms, unsigned lseg, unsigned l1_latency,
    unsigned l2_mode = vm_translation::L2_TLB_STANDARD) {
  return vm_translation::translation_config(
      sms, kPage, vm_translation::tlb_config(4, 4, 1),
      vm_translation::tlb_config(64, 16, 1), 8, 8, 1, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1),
      l1_latency, 1, "", l2_mode,
      vm_translation::segment_config(true, 8, lseg, kRegistration));
}

static vm_translation::translation_config conventional_config(unsigned l2_mode,
                                                               unsigned l1_latency) {
  return vm_translation::translation_config(
      1, kPage, vm_translation::tlb_config(4, 4, 1),
      vm_translation::tlb_config(64, 16, 1), 8, 8, 1, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1),
      l1_latency, 0, "", l2_mode);
}

static void activate_all(vm_translation::translation_controller *vm,
                         unsigned replicas) {
  assert(vm->begin_segment_install());
  for (unsigned sid = 0; sid + 1 < replicas; ++sid) {
    assert(vm->acknowledge_segment_install(sid));
    assert(!vm->segment_active());
  }
  assert(vm->acknowledge_segment_install(replicas - 1));
  assert(vm->segment_active());
}

static void revoke_all(vm_translation::translation_controller *vm,
                       unsigned replicas) {
  assert(vm->begin_segment_revoke());
  for (unsigned sid = 0; sid < replicas; ++sid)
    assert(vm->acknowledge_segment_revoke(sid));
  assert(!vm->segment_active());
  assert(vm->segment_lifecycle() == vm_translation::SEGMENT_LIFECYCLE_INACTIVE);
}

static uint64_t wait_ready(vm_translation::translation_controller *vm,
                           unsigned sid, unsigned asid, uint64_t va,
                           vm_translation::translation_access access,
                           uint64_t uid, uint64_t first_cycle,
                           uint64_t *pa,
                           vm_translation::translation_source *source) {
  for (uint64_t cycle = first_cycle; cycle < first_cycle + 160; ++cycle) {
    const vm_translation::lookup_result result =
        vm->translate(sid, asid, va, 32, cycle, uid, pa, source, access);
    if (result == vm_translation::READY) return cycle;
    assert(result == vm_translation::TRANSLATION_PENDING ||
           result == vm_translation::L1_PORT_STALL ||
           result == vm_translation::L2_PORT_STALL);
    vm->cycle(cycle);
  }
  assert(false && "directed translation did not become ready");
  return 0;
}

static void test_35_replica_lifecycle_and_accesses(unsigned fair_arm,
                                                    unsigned lseg) {
  write_registration();
  vm_translation::translation_config config = candidate_config(35, lseg, 10);
  assert(vm_translation::configure_fair_arm(&config, fair_arm));
  assert(config.valid() && config.num_sms == 35 && config.segment.entries == 8 &&
         config.segment.lookup_latency == lseg);
  vm_translation::translation_controller vm(config);
  assert(!vm.stats().object_attribution_enabled);

  // A request admitted during INSTALLING is entirely conventional even when
  // 34 of 35 local images have acknowledged; there is no partial visibility.
  assert(vm.begin_segment_install());
  for (unsigned sid = 0; sid < 34; ++sid) {
    assert(vm.acknowledge_segment_install(sid));
    assert(!vm.segment_active());
  }
  uint64_t pa = 0;
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  assert(vm.translate(34, 7, 16 * kPage + 9, 32, 0, 1, &pa, &source) ==
         vm_translation::TRANSLATION_PENDING);
  assert(vm.stats().segment_lookup_attempts == 0);
  assert(vm.acknowledge_segment_install(34));
  assert(vm.segment_active() && vm.segment_active_asid() == 7 &&
         vm.segment_active_epoch() == 11);
  uint64_t cycle = wait_ready(&vm, 34, 7, 16 * kPage + 9,
                              vm_translation::TRANSLATION_ACCESS_READ, 1, 1,
                              &pa, &source);
  assert(source != vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  assert(vm.stats().segment_lookup_attempts == 0);
  assert(vm.stats().segment_install_replica_acks == 35 &&
         vm.stats().segment_install_acks == 1);

  // The next read is newly admitted after ACTIVE and must use the true PA
  // descriptor.  Segment completes before the deliberately slower L1 path.
  const uint64_t segment_hits_before = vm.stats().segment_hits;
  cycle = wait_ready(&vm, 34, 7, 17 * kPage + 13,
                     vm_translation::TRANSLATION_ACCESS_READ, 2, cycle + 1,
                     &pa, &source);
  assert(pa == 257 * kPage + 13 &&
         source == vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  assert(vm.stats().segment_hits == segment_hits_before + 1);

  // The same descriptor is read-only.  WRITE and ATOMIC must be ordinary
  // paging, while the registered PA backend remains agreement-preserving.
  const uint64_t fallback_before = vm.stats().segment_fallback_by_reason[
      vm_translation::weight_segment_map::SEGMENT_FALLBACK_RIGHTS];
  cycle = wait_ready(&vm, 34, 7, 16 * kPage + 21,
                     vm_translation::TRANSLATION_ACCESS_WRITE, 3, cycle + 1,
                     &pa, &source);
  assert(pa == 256 * kPage + 21 &&
         source != vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  // Use the second registered extent for ATOMIC so this remains an
  // independent fallback attempt rather than relying on the WRITE page's
  // conventional L1 residency.
  cycle = wait_ready(&vm, 34, 7, 32 * kPage + 29,
                     vm_translation::TRANSLATION_ACCESS_ATOMIC, 4, cycle + 1,
                     &pa, &source);
  assert(pa == 512 * kPage + 29 &&
         source != vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  // At Lseg=20 the previously conventional V16 L1 hit is the legal
  // HIT_FIRST winner, so its still-pending Segment fallback is a late
  // discard.  The independent V32 ATOMIC request still reaches the rights
  // fallback.  At 5/10 both fallback lookups complete before the L1 winner.
  const uint64_t expected_rights_fallbacks = lseg == 20 ? 1 : 2;
  assert(vm.stats().segment_fallback_by_reason[
             vm_translation::weight_segment_map::SEGMENT_FALLBACK_RIGHTS] ==
         fallback_before + expected_rights_fallbacks);

  // An ASID outside the provisioned registration cannot hit Segment.
  const uint64_t asid_fallback_before = vm.stats().segment_fallback_by_reason[
      vm_translation::weight_segment_map::SEGMENT_FALLBACK_ASID];
  cycle = wait_ready(&vm, 34, 8, 16 * kPage + 37,
                     vm_translation::TRANSLATION_ACCESS_READ, 5, cycle + 1,
                     &pa, &source);
  assert(source != vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  assert(vm.stats().segment_fallback_by_reason[
             vm_translation::weight_segment_map::SEGMENT_FALLBACK_ASID] ==
         asid_fallback_before + 1);

  const uint64_t generation = vm.translation_generation(7);
  vm.flush_translation_asid(7);
  assert(vm.translation_generation(7) == generation + 1);
  assert(vm.segment_lifecycle() == vm_translation::SEGMENT_LIFECYCLE_REVOKING &&
         vm.segment_active_epoch() == 11);
  for (unsigned sid = 0; sid < 35; ++sid)
    assert(vm.acknowledge_segment_revoke(sid));
  assert(vm.segment_lifecycle() == vm_translation::SEGMENT_LIFECYCLE_INACTIVE &&
         vm.stats().segment_revoke_replica_acks == 35 &&
         vm.stats().segment_revoke_acks == 1);
  assert(vm.invariants_hold());
}

static void test_ordering_and_retry() {
  write_registration();
  // Segment-first: L1 is slower.  An L1 miss must wait for, then be
  // suppressed by, the Segment hit; no conventional lower request is issued.
  vm_translation::translation_controller segment_first(
      candidate_config(1, 5, 10));
  activate_all(&segment_first, 1);
  uint64_t pa = 0;
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  uint64_t cycle = wait_ready(&segment_first, 0, 7, 16 * kPage + 7,
                              vm_translation::TRANSLATION_ACCESS_READ, 10, 0,
                              &pa, &source);
  assert(pa == 256 * kPage + 7 &&
         source == vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  assert(segment_first.stats().segment_first_owners == 1 &&
         segment_first.stats().segment_late_result_discards == 1 &&
         segment_first.stats().l2_lookup_launches == 0 &&
         segment_first.stats().mshr_allocations == 0);

  // An unregistered read produces the MISS_JOIN both-miss path exactly once.
  const uint64_t l2_before = segment_first.stats().l2_lookup_launches;
  const uint64_t launches_before = segment_first.stats().segment_lookup_launches;
  const uint64_t misses_before = segment_first.stats().segment_misses;
  cycle = wait_ready(&segment_first, 0, 7, 64 * kPage + 3,
                     vm_translation::TRANSLATION_ACCESS_READ, 11, cycle + 1,
                     &pa, &source);
  assert(source == vm_translation::TRANSLATION_SOURCE_PTW &&
         segment_first.stats().l2_lookup_launches == l2_before + 1 &&
         segment_first.stats().segment_lookup_launches == launches_before + 1 &&
         segment_first.stats().segment_misses == misses_before + 1 &&
         segment_first.stats().segment_both_miss >= 1);
  // wait_ready retried UID 11 until the PTW delivery completed; the exact
  // +1 launch assertion above proves those retries did not re-probe Segment.

  // Segment miss + L1 hit: Segment completes first as a miss, then the
  // previously filled L1 provides the conventional mapping without another L2.
  const uint64_t l2_after_fill = segment_first.stats().l2_lookup_launches;
  cycle = wait_ready(&segment_first, 0, 7, 64 * kPage + 3,
                     vm_translation::TRANSLATION_ACCESS_READ, 12, cycle + 2,
                     &pa, &source);
  assert(source == vm_translation::TRANSLATION_SOURCE_L1_TLB_HIT &&
         segment_first.stats().l2_lookup_launches == l2_after_fill);

  // L1-first: prefill L1 through a write fallback, then issue a read at the
  // 20-cycle Segment point.  The hit must not wait for the slow Segment.
  vm_translation::translation_controller l1_first(candidate_config(1, 20, 3));
  activate_all(&l1_first, 1);
  cycle = wait_ready(&l1_first, 0, 7, 32 * kPage + 11,
                     vm_translation::TRANSLATION_ACCESS_WRITE, 20, 0,
                     &pa, &source);
  const uint64_t l2_after_write = l1_first.stats().l2_lookup_launches;
  cycle = wait_ready(&l1_first, 0, 7, 32 * kPage + 11,
                     vm_translation::TRANSLATION_ACCESS_READ, 21, cycle + 1,
                     &pa, &source);
  assert(pa == 512 * kPage + 11 &&
         source == vm_translation::TRANSLATION_SOURCE_L1_TLB_HIT &&
         l1_first.stats().segment_l1_first_owners == 1 &&
         l1_first.stats().segment_late_result_discards == 1 &&
         l1_first.stats().l2_lookup_launches == l2_after_write);
  assert(segment_first.invariants_hold() && l1_first.invariants_hold());
}

static void test_stale_generation(unsigned l2_mode) {
  // Two waiters share an obsolete walk; its exact or sub-entry fill must be
  // discarded after the ASID generation advances, never resurrected.
  vm_translation::translation_controller stale(
      conventional_config(l2_mode, 0));
  const uint64_t va = 96 * kPage + 5;
  const vm_translation::translation_key key(9, 96, kPage);
  uint64_t pa = 0;
  assert(stale.translate(0, 9, va, 32, 0, 30, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  stale.cycle(0);
  assert(stale.active_mshrs() == 1);
  assert(stale.translate(0, 9, va, 32, 1, 31, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  const uint64_t generation = stale.translation_generation(9);
  stale.flush_translation_asid(9);
  assert(stale.translation_generation(9) == generation + 1);
  assert(stale.complete_translation(key, 1));
  assert(stale.stats().translation_stale_fills_discarded == 1 &&
         stale.stats().translation_stale_waiters_discarded == 2 &&
         stale.active_mshrs() == 0);
  if (l2_mode == vm_translation::L2_TLB_STANDARD)
    assert(stale.l2().occupancy() == 0);
  else
    assert(stale.l2_subentries().valid_subentries() == 0);

  // An accepted L1 operation is likewise discarded before it becomes ready.
  vm_translation::translation_controller ready(
      conventional_config(l2_mode, 5));
  assert(ready.translate(0, 10, 112 * kPage + 7, 32, 0, 40, &pa) ==
         vm_translation::TRANSLATION_PENDING);
  ready.flush_translation_asid(10);
  ready.cycle(5);
  assert(ready.stats().translation_stale_ready_discards == 1 &&
         ready.active_mshrs() == 0 && ready.invariants_hold());
}

static void test_epoch_wrap_requires_quiesce() {
  write_registration(0xffff);
  vm_translation::translation_controller vm(candidate_config(1, 5, 3));
  activate_all(&vm, 1);
  revoke_all(&vm, 1);
  // Reuse is forbidden until the explicit quiesce control has run.
  assert(!vm.begin_segment_install());
  assert(vm.quiesce_segment_epoch_wrap());
  assert(vm.begin_segment_install());
  assert(vm.acknowledge_segment_install(0));
  revoke_all(&vm, 1);
}

static vm_translation::translation_config fair_seed(unsigned lseg) {
  return vm_translation::translation_config(
      35, kPage, vm_translation::tlb_config(4, 4, 1),
      vm_translation::tlb_config(800, 16, 1), 8, 8, 1, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 3, 1, "",
      vm_translation::L2_TLB_STANDARD,
      vm_translation::segment_config(true, 8, lseg, kRegistration));
}

static void test_fair_selector() {
  const unsigned arms[] = {
      vm_translation::FAIR_ARM_F0_BASELINE_EXACT,
      vm_translation::FAIR_ARM_F1_SUBENTRY_G96,
      vm_translation::FAIR_ARM_F2_EXACT_E688,
      vm_translation::FAIR_ARM_F3_EXACT_SWEEP,
      vm_translation::FAIR_ARM_F4_EXACT_E1536,
      vm_translation::FAIR_ARM_F6_EXACT_2M_E848,
      vm_translation::FAIR_ARM_F9_EXACT_E656};
  for (unsigned index = 0; index < sizeof(arms) / sizeof(arms[0]); ++index) {
    vm_translation::translation_config config = fair_seed(5);
    assert(vm_translation::configure_fair_arm(&config, arms[index]));
    assert(config.valid() && vm_translation::fair_arm_charged_bits(config) != 0);
  }
  for (unsigned latency = 5; latency <= 20; latency += latency == 5 ? 5 : 10) {
    vm_translation::translation_config f7 = fair_seed(latency);
    vm_translation::translation_config f8 = fair_seed(latency);
    assert(vm_translation::configure_fair_arm(
        &f7, vm_translation::FAIR_ARM_F7_SEGMENT_EXACT_E320));
    assert(vm_translation::configure_fair_arm(
        &f8, vm_translation::FAIR_ARM_F8_SEGMENT_SUBENTRY_G32));
    assert(f7.valid() && f8.valid() && f7.l2.entries == 320 &&
           f8.l2.entries == 32 && f8.l2.sets() == 2 &&
           f7.segment.entries == 8 && f8.segment.entries == 8);
  }
  vm_translation::translation_config rejected = fair_seed(5);
  assert(!vm_translation::configure_fair_arm(
      &rejected, vm_translation::FAIR_ARM_F5_BLOCKED_PHYSICAL_PWC));
  rejected = fair_seed(5);
  assert(!vm_translation::configure_fair_arm(
      &rejected, vm_translation::FAIR_ARM_H0_HISTORICAL_UNFAIR));
}

static void test_mapping_mismatch_fails_closed() {
  write_registration();
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    // Fill L1 under the inactive historical mapper (VPN=16), then activate a
    // descriptor that maps the same VA to PPN=256.  Same-cycle L1/Segment
    // completions must take the explicit correctness-fault assertion rather
    // than choose an arbitrary PA.
    vm_translation::translation_controller vm(candidate_config(1, 5, 5));
    uint64_t pa = 0;
    vm_translation::translation_source source =
        vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
    wait_ready(&vm, 0, 7, 16 * kPage + 1,
               vm_translation::TRANSLATION_ACCESS_READ, 50, 0, &pa, &source);
    assert(pa == 16 * kPage + 1);
    activate_all(&vm, 1);
    assert(vm.translate(0, 7, 16 * kPage + 1, 32, 100, 51, &pa, &source) ==
           vm_translation::TRANSLATION_PENDING);
    for (uint64_t cycle = 100; cycle <= 105; ++cycle) vm.cycle(cycle);
    _exit(0);  // Reaching here would mean the mismatch was not fail-closed.
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

int main() {
  // F7/F8 instantiate all 35 local replicas at every mandatory Lseg point.
  const unsigned latencies[] = {5, 10, 20};
  for (unsigned index = 0; index < 3; ++index) {
    test_35_replica_lifecycle_and_accesses(
        vm_translation::FAIR_ARM_F7_SEGMENT_EXACT_E320, latencies[index]);
    test_35_replica_lifecycle_and_accesses(
        vm_translation::FAIR_ARM_F8_SEGMENT_SUBENTRY_G32, latencies[index]);
  }
  test_ordering_and_retry();
  test_stale_generation(vm_translation::L2_TLB_STANDARD);
  test_stale_generation(vm_translation::L2_TLB_SUBENTRY_16);
  test_epoch_wrap_requires_quiesce();
  test_fair_selector();
  test_mapping_mismatch_fails_closed();
  assert(remove(kRegistration) == 0);
  printf("vm_c10b_runtime_validation_test PASS\n");
  return 0;
}
