#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>

#include "gpgpu-sim/vm_translation.h"

static const char *kRegistration = "/tmp/vm_c10a_registration_v2.tsv";
static const uint64_t kPage = 64ULL * 1024ULL;

static void write_registration() {
  std::ofstream out(kRegistration);
  assert(out.good());
  out << "M4B_WEIGHT_SEGMENT_REGISTRATION_V2\n";
  out << "roi\tunit\n";
  out << "source_sha256\t"
      << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  out << "archive_sha256\t"
      << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  out << "object_map_sha256\t"
      << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  out << "provisioned_asid\t7\n";
  out << "epoch\t11\n";
  // VPN 16..17 map to non-identity PPN 256..257.
  out << "descriptor\t7\t11\t16\t17\t256\t1\t0\n";
  // A separate physical extent is legal and retains the same ASID/epoch.
  out << "descriptor\t7\t11\t32\t32\t512\t1\t0\n";
}

int main() {
  write_registration();
  vm_translation::weight_segment_map map(kRegistration);
  assert(map.enabled() && map.registered_v2() && map.size() == 2);
  assert(map.active_asid() == 7 && map.active_epoch() == 11);
  uint64_t ppn = 0;
  vm_translation::weight_segment_map::fallback_reason reason =
      vm_translation::weight_segment_map::SEGMENT_FALLBACK_NONE;
  const vm_translation::translation_key key(7, 16, kPage);
  assert(map.translate(key, 16 * kPage + 19, 32, true, &ppn, &reason));
  assert(ppn == 256 && ppn != key.vpn);
  uint64_t conventional = 0;
  assert(map.registered_ppn(key, &conventional) && conventional == ppn);
  assert(!map.translate(vm_translation::translation_key(8, 16, kPage),
                        16 * kPage + 19, 32, true, &ppn, &reason));
  assert(reason == vm_translation::weight_segment_map::SEGMENT_FALLBACK_ASID);
  assert(!map.translate(key, 16 * kPage + kPage - 8, 16, true, &ppn,
                        &reason));
  assert(reason == vm_translation::weight_segment_map::SEGMENT_FALLBACK_BOUNDARY);
  assert(!map.translate(key, 16 * kPage + 19, 32, false, &ppn, &reason));
  assert(reason == vm_translation::weight_segment_map::SEGMENT_FALLBACK_RIGHTS);
  assert(remove(kRegistration) == 0);
  printf("vm_c10a_registered_segment_test PASS\n");
  return 0;
}
