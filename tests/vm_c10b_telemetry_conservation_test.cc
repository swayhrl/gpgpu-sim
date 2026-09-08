#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

// Tiny directed C10B-3 workload.  It emits the controller's production text
// telemetry; the Framework-side parser validates the emitted counters.
static const char *kRegistration = "/tmp/vm_c10b_telemetry_registration.tsv";
static const char *kObjectMap = "/tmp/vm_c10b_telemetry_objects.tsv";
static const char *kSubentryStats = "/tmp/vm_c10b_subentry_telemetry.stats";
static const uint64_t kPage = 64ULL * 1024ULL;

static void write_artifacts() {
  {
    std::ofstream out(kRegistration);
    assert(out.good());
    out << "M4B_WEIGHT_SEGMENT_REGISTRATION_V2\n";
    out << "roi\tc10b_telemetry\n";
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
  {
    std::ofstream out(kObjectMap);
    assert(out.good());
    out << "M4C_OBJECT_MAP_V1\n";
    out << "roi\tc10b_telemetry\n";
    out << "source_sha256\t"
        << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
    out << "archive_sha256\t"
        << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
    out << "sidecar_sha256\t"
        << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
    out << "range\tWEIGHT\t0x100000\t0x11ffff\n";
    out << "range\tKV_CACHE\t0x400000\t0x40ffff\n";
  }
}

static vm_translation::translation_config config() {
  return vm_translation::translation_config(
      1, kPage, vm_translation::tlb_config(4, 4, 1),
      vm_translation::tlb_config(64, 16, 1), 8, 8, 1, 1,
      vm_translation::page_table_config(), 1,
      vm_translation::pwc_config(vm_translation::PWC_FINITE, 4, 1), 10, 1,
      kObjectMap, vm_translation::L2_TLB_STANDARD,
      vm_translation::segment_config(true, 8, 5, kRegistration));
}

static vm_translation::translation_config subentry_config() {
  // This is intentionally conventional paging: it emits the candidate L2
  // group/leaf telemetry independently of the registered Segment path above.
  return vm_translation::translation_config(
      1, kPage, vm_translation::tlb_config(1, 1, 1),
      vm_translation::tlb_config(2, 2, 1), 8, 8, 1, 1,
      vm_translation::page_table_config(), 1,
      vm_translation::pwc_config(vm_translation::PWC_FINITE, 4, 1), 1, 1,
      kObjectMap, vm_translation::L2_TLB_SUBENTRY_16,
      vm_translation::segment_config(false, 0, 0, ""));
}

static void activate(vm_translation::translation_controller *controller) {
  assert(controller->begin_segment_install());
  assert(controller->acknowledge_segment_install(0));
  assert(controller->segment_active());
}

static uint64_t drive(vm_translation::translation_controller *controller,
                      uint64_t va, vm_translation::translation_access access,
                      uint64_t uid, uint64_t first_cycle, uint64_t *pa,
                      vm_translation::translation_source *source) {
  for (uint64_t cycle = first_cycle; cycle < first_cycle + 256; ++cycle) {
    const vm_translation::lookup_result result =
        controller->translate(0, 7, va, 32, cycle, uid, pa, source, access);
    if (result == vm_translation::READY) return cycle;
    assert(result == vm_translation::TRANSLATION_PENDING ||
           result == vm_translation::L1_PORT_STALL ||
           result == vm_translation::L2_PORT_STALL);
    controller->cycle(cycle);
    vm_translation::pte_request request;
    if (controller->next_pte_request(&request)) {
      assert(request.is_physical && request.bypass_translation);
      assert(controller->pte_request_issued(request, cycle));
      // Alternate L2-only and DRAM completions while retaining the exact
      // request identity and physical PTE address.
      assert(controller->complete_pte_response(
          request.request_id, request.physical_address, (request.level & 1) != 0,
          cycle + 1));
    }
  }
  assert(false && "telemetry-directed request did not complete");
  return 0;
}

int main() {
  write_artifacts();
  vm_translation::translation_controller controller(config());
  activate(&controller);

  uint64_t pa = 0;
  vm_translation::translation_source source =
      vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
  uint64_t cycle = drive(&controller, 16 * kPage + 7,
                         vm_translation::TRANSLATION_ACCESS_READ, 1, 0, &pa,
                         &source);
  assert(pa == 256 * kPage + 7 &&
         source == vm_translation::TRANSLATION_SOURCE_SEGMENT_HIT);
  cycle = drive(&controller, 64 * kPage + 3,
                vm_translation::TRANSLATION_ACCESS_READ, 2, cycle + 1, &pa,
                &source);
  assert(pa == 64 * kPage + 3 &&
         source == vm_translation::TRANSLATION_SOURCE_PTW);
  cycle = drive(&controller, 17 * kPage + 11,
                vm_translation::TRANSLATION_ACCESS_WRITE, 3, cycle + 1, &pa,
                &source);
  assert(pa == 257 * kPage + 11 &&
         source == vm_translation::TRANSLATION_SOURCE_PTW);

  assert(controller.stats().segment_lookup_attempts == 3);
  assert(controller.stats().segment_lookup_accepts == 3);
  assert(controller.stats().segment_hits == 1);
  assert(controller.stats().segment_misses == 2);
  assert(controller.stats().segment_l2_suppressed == 1);
  assert(controller.stats().segment_mshr_suppressed == 1);
  assert(controller.stats().l2_lookup_launches == 2);
  assert(controller.stats().mshr_allocations == 2);
  // `requester_completions` is a critical-path observation counter.  A
  // conventional MSHR waiter is sampled once when the walk wakes it and once
  // when its normal frontend retry returns READY; M3-G3.5 deliberately
  // relies on the former sample.  `completed` is the externally visible
  // exactly-once completion counter, so audit it separately rather than
  // silently redefining the accepted M3 latency telemetry.
  assert(controller.stats().completed == 3);
  assert(controller.stats().requester_completions ==
         controller.stats().completed +
             controller.stats().mshr_entries_completed);
  assert(controller.stats().pte_requests == controller.stats().pte_responses);
  assert(controller.stats().pte_l2_only_responses +
             controller.stats().pte_dram_responses ==
         controller.stats().pte_responses);
  assert(controller.object_attribution_conserves());
  assert(controller.begin_segment_revoke());
  assert(controller.acknowledge_segment_revoke(0));
  assert(controller.quiescent_invariants_hold());

  controller.print_stats(stdout);

  // Fill two leaves in one 16-leaf group, then force the first leaf through
  // the candidate L2 after a one-entry L1 eviction.  The saved second emitted
  // stats stream lets the Framework validator prove group/leaf accounting
  // without conflating it with the Segment-owned standard-L2 workload.
  vm_translation::translation_controller subentry(subentry_config());
  cycle = drive(&subentry, 64 * kPage + 5,
                vm_translation::TRANSLATION_ACCESS_READ, 11, cycle + 1, &pa,
                &source);
  assert(source == vm_translation::TRANSLATION_SOURCE_PTW);
  cycle = drive(&subentry, 65 * kPage + 7,
                vm_translation::TRANSLATION_ACCESS_READ, 12, cycle + 1, &pa,
                &source);
  assert(source == vm_translation::TRANSLATION_SOURCE_PTW);
  drive(&subentry, 64 * kPage + 9, vm_translation::TRANSLATION_ACCESS_READ,
        13, cycle + 1, &pa, &source);
  assert(source == vm_translation::TRANSLATION_SOURCE_L2_TLB_HIT);
  assert(subentry.stats().l2_lookup_launches == 3);
  assert(subentry.l2_subentries().stats().hits == 1);
  assert(subentry.l2_subentries().stats().misses == 2);
  assert(subentry.l2_subentries().valid_subentries() == 2);
  assert(subentry.l2_subentries().subentry_stats().group_fills == 1);
  assert(subentry.l2_subentries().subentry_stats().existing_group_fills == 1);
  assert(subentry.object_attribution_conserves());
  assert(subentry.quiescent_invariants_hold());
  FILE *subentry_stats = fopen(kSubentryStats, "w");
  assert(subentry_stats != 0);
  subentry.print_stats(subentry_stats);
  assert(fclose(subentry_stats) == 0);

  assert(remove(kRegistration) == 0);
  assert(remove(kObjectMap) == 0);
  return 0;
}
