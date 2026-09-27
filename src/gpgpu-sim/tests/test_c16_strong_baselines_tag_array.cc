#include "../gpu-cache.h"

#include <assert.h>

#include <fstream>
#include <iostream>
#include <string>

class exposed_c16_tag_array : public tag_array {
 public:
  explicit exposed_c16_tag_array(cache_config &config)
      : tag_array(config, 0, 0) {}

  void install(unsigned index, new_addr_type address, unsigned rrpv,
               bool priority) {
    mem_access_sector_mask_t sector;
    sector.set(0);
    mem_access_byte_mask_t bytes;
    cache_block_t *block = get_block(index);
    block->allocate(address, address, 1, sector);
    block->fill(2, sector, bytes);
    m_c16_rrpv[index] = rrpv;
    if (priority) {
      m_c16_priority_state[index] = 2;
      ++m_c16_diagnostics.priority_resident;
      m_c16_diagnostics.priority_peak_resident =
          m_c16_diagnostics.priority_resident;
      m_c16_diagnostics.priority_peak_resident_plus_pending =
          m_c16_diagnostics.priority_resident;
    }
    c16_baseline_verify();
  }

  cache_request_status probe_sector(new_addr_type address, unsigned sector,
                                    unsigned *index, bool probe_mode) const {
    mem_access_sector_mask_t mask;
    mask.set(sector);
    return c16_baseline_probe(address, *index, mask, false, probe_mode, NULL);
  }

  unsigned rrpv(unsigned index) const { return m_c16_rrpv[index]; }
  uint64_t resident() const { return m_c16_diagnostics.priority_resident; }
};

static std::string write_sidecar(const char *path) {
  std::ofstream out(path, std::ios::binary);
  out << "ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1\t1\tMODELED_L2_GET_ADDR\n";
  const uint64_t region_bytes = 265216ULL * 128;
  const uint64_t stride = region_bytes + 128;
  for (unsigned target_class = 1; target_class <= 28; ++target_class) {
    const uint64_t begin = 0x100000000ULL + (target_class - 1) * stride;
    out << "0x" << std::hex << begin << "\t0x" << begin + region_bytes
        << std::dec << "\tlayer" << target_class - 1 << "\t"
        << target_class << "\n";
  }
  out.close();
  return oracle_elastic_residency::oracle_config::sha256_file(path);
}

static cache_config make_config(unsigned policy, const char *sidecar,
                                const char *digest) {
  static char config_string[] = "S:64:128:4,L:B:m:L:L,A:8:8,8:8,32";
  cache_config config;
  config.m_config_string = config_string;
  config.init(config_string, FuncCachePreferNone);
  config.m_oracle_elastic_enable = false;
  config.m_oracle_elastic_sidecar = const_cast<char *>(sidecar);
  config.m_oracle_elastic_sidecar_sha256 = const_cast<char *>(digest);
  config.init_oracle_elastic(1);
  config.m_c16_baseline_enable = true;
  config.m_c16_baseline_policy = policy;
  config.m_c16_baseline_diagnostics = true;
  config.init_c16_strong_baseline(1);
  return config;
}

int main() {
  const char *sidecar = "/tmp/c16_strong_baseline_sidecar.tsv";
  const std::string digest = write_sidecar(sidecar);

  cache_config priority = make_config(
      c16_strong_baselines::POLICY_PRIORITY_STABLE, sidecar, digest.c_str());
  assert(priority.c16_baseline_enabled());
  assert(std::string(priority.c16_baseline_policy_name()) ==
         "PRIORITY_STABLE");
  exposed_c16_tag_array priority_tags(priority);
  priority_tags.install(0, 0x1000, 3, true);
  assert(priority_tags.resident() == 1);
  mem_access_sector_mask_t sector0;
  sector0.set(0);
  priority_tags.oracle_invalidate_line(0, sector0);
  assert(priority_tags.resident() == 0);

  cache_config drrip = make_config(c16_strong_baselines::POLICY_DRRIP,
                                   sidecar, digest.c_str());
  exposed_c16_tag_array tags(drrip);
  tags.install(0, 0x0000, 1, false);
  tags.install(1, 0x0080, 2, false);
  tags.install(2, 0x0100, 2, false);
  tags.install(3, 0x0180, 2, false);
  unsigned victim = 0;
  assert(tags.probe_sector(0x2000, 0, &victim, true) == MISS);
  assert(tags.rrpv(0) == 1 && tags.rrpv(1) == 2);
  assert(tags.probe_sector(0x2000, 0, &victim, false) == MISS);
  assert(victim == 1);
  assert(tags.rrpv(0) == 2 && tags.rrpv(1) == 3);

  // Same tag / different invalid sector is a sector miss and does not run
  // the new-line victim path or alter line-granular RRPV.
  unsigned index = 0;
  const unsigned before = tags.rrpv(0);
  assert(tags.probe_sector(0x0000, 1, &index, false) == SECTOR_MISS);
  assert(index == 0 && tags.rrpv(0) == before);

  FILE *stats = tmpfile();
  assert(stats != NULL);
  unsigned accesses = 0, misses = 0;
  tags.print(stats, accesses, misses);
  rewind(stats);
  std::string text;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), stats)) text += buffer;
  fclose(stats);
  assert(text.find("c16_strong_baseline_l2") != std::string::npos);
  assert(text.find("policy=DRRIP") != std::string::npos);
  assert(text.find("drrip_psel=512") != std::string::npos);

  remove(sidecar);
  std::cout << "C16_STRONG_BASELINES_TAG_ARRAY_TEST_PASS\n";
  return 0;
}
