#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <string>

#include "gpgpu-sim/gpu-cache.h"
#include "gpgpu-sim/memory_telemetry.h"
#include "gpgpu-sim/vm_translation.h"

static const char *kMapPath = "/tmp/vm_m4c_object_attribution_map.tsv";

static void write_map() {
  std::ofstream output(kMapPath);
  assert(output.good());
  output << "M4C_OBJECT_MAP_V1\n";
  output << "roi\ttest\n";
  output << "source_sha256\t"
         << "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n";
  output << "archive_sha256\t"
         << "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n";
  output << "sidecar_sha256\t"
         << "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\n";
  output << "range\tWEIGHT\t0x10000\t0x1ffff\n";
  output << "range\tKV_CACHE\t0x30000\t0x3ffff\n";
}

static vm_translation::translation_config config(const char *map) {
  const uint64_t page = 64ULL * 1024ULL;
  return vm_translation::translation_config(
      1, page, vm_translation::tlb_config(1, 1, 1),
      vm_translation::tlb_config(1, 1, 1), 8, 8, 1, 1,
      vm_translation::page_table_config(), 0,
      vm_translation::pwc_config(vm_translation::PWC_OFF, 0, 1), 0, 0, map);
}

static void run(vm_translation::translation_controller *controller) {
  const uint64_t page = 64ULL * 1024ULL;
  const uint64_t addresses[] = {page + 4, 3 * page + 8, 5 * page + 12};
  for (unsigned index = 0; index < 3; ++index) {
    uint64_t pa = 0;
    vm_translation::translation_source source =
        vm_translation::TRANSLATION_SOURCE_UNOBSERVED;
    const uint64_t uid = 100 + index;
    const uint64_t start = index * 16;
    const vm_translation::lookup_result initial = controller->translate(
        0, 0, addresses[index], 32, start, uid, &pa);
    // The accepted zero-latency diagnostic API may surface a synchronous
    // capacity status after it has already admitted the MSHR.  It is not a
    // second probe and it must not alter the later ready result.
    assert(initial != vm_translation::READY);
    for (uint64_t cycle = start; cycle < start + 8; ++cycle)
      controller->cycle(cycle);
    const vm_translation::lookup_result complete = controller->translate(
        0, 0, addresses[index], 32, start + 8, uid, &pa, &source);
    assert(complete == vm_translation::READY);
    assert(pa == addresses[index]);
    assert(source == vm_translation::TRANSLATION_SOURCE_PTW);
  }
  assert(controller->quiescent_invariants_hold());
}

static std::string read_file(FILE *file) {
  assert(file != 0);
  assert(fseek(file, 0, SEEK_END) == 0);
  const long size = ftell(file);
  assert(size >= 0);
  assert(fseek(file, 0, SEEK_SET) == 0);
  std::string result(static_cast<size_t>(size), '\0');
  if (size != 0)
    assert(fread(&result[0], 1, static_cast<size_t>(size), file) ==
           static_cast<size_t>(size));
  return result;
}

static void telemetry_schema_test() {
  m4c_memory_telemetry telemetry;
  telemetry.configure(2, 2);
  telemetry.record_l1(M4C_DATA_WEIGHT, MISS, 128, 0,
                      M4C_TRANSLATION_PTW);
  telemetry.record_l2(M4C_DATA_WEIGHT, MISS, 128, 0,
                      M4C_TRANSLATION_PTW, MISS);
  telemetry.record_dram(M4C_DATA_WEIGHT, 128, false, 0);
  telemetry.record_l1(M4C_PTE_L0, HIT, 32, 0);
  telemetry.record_l2(M4C_PTE_L0, HIT, 32, 0);
  telemetry.record_dram(M4C_PTE_L0, 32, true, 0);
  telemetry.record_l2_replacement(M4C_PTE_L0, M4C_DATA_WEIGHT, 0);
  telemetry.record_l2_queue(1, 2, 3, 4, 0);
  FILE *output = tmpfile();
  assert(output != 0);
  telemetry.print(output, "m4c_test_kernel");
  const std::string text = read_file(output);
  assert(text.find("m4c_telemetry_schema = M4C_MEMORY_TELEMETRY_V1") !=
         std::string::npos);
  assert(text.find("m4c_telemetry_kernel_index = 0") != std::string::npos);
  assert(text.find("m4c_telemetry_bytes\tKERNEL\tm4c_test_kernel") !=
         std::string::npos);
  assert(text.find("m4c_telemetry_dram_rw\tKERNEL\tm4c_test_kernel") !=
         std::string::npos);
  assert(text.find("m4c_telemetry_l2_replacement\tKERNEL\tm4c_test_kernel\t0\tPTE_L0\tDATA_WEIGHT\t1") !=
         std::string::npos);
  assert(text.find("m4c_telemetry_cross_l1_l2\tKERNEL\tm4c_test_kernel\t0\tDATA_WEIGHT\tPTW\tMISS\tMISS\t1") !=
         std::string::npos);
  assert(text.find("m4c_telemetry\tFIXED_WINDOW\tm4c_test_kernel\t0") !=
         std::string::npos);
  fclose(output);
}

int main() {
  write_map();
  vm_translation::object_range_map map(kMapPath);
  assert(map.enabled());
  assert(map.classify(0x10004, 32) == vm_translation::OBJECT_WEIGHT);
  assert(map.classify(0x30008, 32) == vm_translation::OBJECT_KV_CACHE);
  assert(map.classify(0x50008, 32) == vm_translation::OBJECT_UNKNOWN);
  // A known-range boundary crossing is conservatively UNKNOWN, never a
  // fabricated full WEIGHT classification.
  assert(map.classify(0x1fff0, 32) == vm_translation::OBJECT_UNKNOWN);

  vm_translation::translation_controller no_map(config(""));
  vm_translation::translation_controller with_map(config(kMapPath));
  run(&no_map);
  run(&with_map);

  const vm_translation::translation_stats &baseline = no_map.stats();
  const vm_translation::translation_stats &observed = with_map.stats();
  // The map changes only metadata; all accepted VM behavior counters match.
  assert(baseline.lookup_requests == observed.lookup_requests);
  assert(baseline.l1_lookup_launches == observed.l1_lookup_launches);
  assert(baseline.l2_lookup_launches == observed.l2_lookup_launches);
  assert(baseline.mshr_allocations == observed.mshr_allocations);
  assert(baseline.mshr_merges == observed.mshr_merges);
  assert(baseline.walk_starts == observed.walk_starts);
  assert(baseline.completed == observed.completed);
  assert(no_map.l2().stats().accesses == with_map.l2().stats().accesses);
  assert(no_map.l2().stats().hits == with_map.l2().stats().hits);
  assert(no_map.l2().stats().misses == with_map.l2().stats().misses);
  assert(no_map.l2().stats().evictions == with_map.l2().stats().evictions);

  assert(observed.object_attribution_enabled);
  // A completed MSHR waiter re-enters the accepted controller once to receive
  // its now-resident SimPA.  Requester accounting therefore follows the
  // existing L1-launch definition, while unique-key accounting remains one.
  assert(observed.object[vm_translation::OBJECT_WEIGHT].translation_requesters ==
         observed.object[vm_translation::OBJECT_WEIGHT].l1_lookup_launches);
  assert(observed.object[vm_translation::OBJECT_KV_CACHE]
             .translation_requesters ==
         observed.object[vm_translation::OBJECT_KV_CACHE].l1_lookup_launches);
  assert(observed.object[vm_translation::OBJECT_UNKNOWN].translation_requesters ==
         observed.object[vm_translation::OBJECT_UNKNOWN].l1_lookup_launches);
  assert(with_map.object_attribution_conserves());
  telemetry_schema_test();
  assert(remove(kMapPath) == 0);
  printf("vm_m4c_object_attribution_test PASS\n");
  return 0;
}
