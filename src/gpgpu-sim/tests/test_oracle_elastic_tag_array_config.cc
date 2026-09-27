#include "../gpu-cache.h"

#include <assert.h>
#include <stdio.h>

#include <fstream>
#include <iostream>
#include <string>

class exposed_tag_array : public tag_array {
 public:
  exposed_tag_array(cache_config &config, int core_id, int type_id)
      : tag_array(config, core_id, type_id) {}

  void install_protected_sector_line(unsigned index) {
    mem_access_sector_mask_t sector0;
    sector0.set(0);
    mem_access_byte_mask_t bytes;
    cache_block_t *block = get_block(index);
    block->allocate(1, 0x1000, 1, sector0);
    block->oracle_set_pending(true, 1);
    ++m_oracle_pending_protected;
    block->fill(2, sector0, bytes);
    oracle_commit_fill(index);
  }

  void fill_second_sector_without_new_admission(unsigned index) {
    mem_access_sector_mask_t sector1;
    sector1.set(1);
    mem_access_byte_mask_t bytes;
    sector_cache_block *block =
        static_cast<sector_cache_block *>(get_block(index));
    block->allocate_sector(3, sector1);
    block->fill(4, sector1, bytes);
    oracle_commit_fill(index);
  }

  void install_ordinary_sector_line(unsigned index) {
    mem_access_sector_mask_t sector0;
    sector0.set(0);
    mem_access_byte_mask_t bytes;
    cache_block_t *block = get_block(index);
    block->allocate(2, 0x2000, 1, sector0);
    block->oracle_set_pending(false);
    ++m_oracle_ordinary_valid;
    block->fill(2, sector0, bytes);
    oracle_commit_fill(index);
  }

  uint64_t ordinary_valid() const { return m_oracle_ordinary_valid; }
};

int main() {
  const char *sidecar = "/tmp/c16_oracle_tag_array_sidecar.tsv";
  std::ofstream file(sidecar, std::ios::binary);
  file << "ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1\t1\tMODELED_L2_GET_ADDR\n";
  file << "0x1000\t0x2000\ttarget\t1\n";
  file.close();
  std::string digest =
      oracle_elastic_residency::oracle_config::sha256_file(sidecar);

  char cache_string[] = "N:1:128:2,L:B:m:L:L,A:4:4,4:4,32";
  cache_config config;
  config.m_config_string = cache_string;
  config.init(cache_string, FuncCachePreferNone);
  config.m_oracle_elastic_enable = true;
  config.m_oracle_elastic_quota_bytes = 1000;
  config.m_oracle_elastic_quota_lines = 0;
  config.m_oracle_elastic_sidecar = const_cast<char *>(sidecar);
  config.m_oracle_elastic_sidecar_sha256 = const_cast<char *>(digest.c_str());
  config.m_oracle_elastic_diagnostics = false;
  config.init_oracle_elastic(3);
  assert(config.oracle_elastic_global_lines() == 8);

  tag_array first(config, 0, 0);
  tag_array second(config, 1, 0);
  tag_array third(config, 2, 0);
  assert(first.oracle_protected_quota() == 3);
  assert(second.oracle_protected_quota() == 3);
  assert(third.oracle_protected_quota() == 2);
  first.oracle_verify_invariant();
  second.oracle_verify_invariant();
  third.oracle_verify_invariant();

  char sector_string[] = "S:1:128:2,L:B:m:L:L,A:4:4,4:4,32";
  cache_config sector_config;
  sector_config.m_config_string = sector_string;
  sector_config.init(sector_string, FuncCachePreferNone);
  sector_config.m_oracle_elastic_enable = true;
  sector_config.m_oracle_elastic_quota_bytes = 0;
  sector_config.m_oracle_elastic_quota_lines = 1;
  sector_config.m_oracle_elastic_sidecar = const_cast<char *>(sidecar);
  sector_config.m_oracle_elastic_sidecar_sha256 =
      const_cast<char *>(digest.c_str());
  sector_config.m_oracle_elastic_diagnostics = true;
  sector_config.init_oracle_elastic(1);
  exposed_tag_array sectors(sector_config, 0, 0);
  sectors.install_protected_sector_line(0);
  assert(sectors.oracle_protected_occupancy() == 1);
  assert(sectors.get_block(0)->oracle_protected());
  sectors.fill_second_sector_without_new_admission(0);
  assert(sectors.oracle_protected_occupancy() == 1);
  assert(sectors.get_block(0)->oracle_protected());
  sectors.oracle_verify_invariant();
  FILE *stats = tmpfile();
  assert(stats != NULL);
  unsigned accesses = 0, misses = 0;
  sectors.print(stats, accesses, misses);
  rewind(stats);
  std::string text;
  char buffer[4096];
  while (fgets(buffer, sizeof(buffer), stats)) text += buffer;
  fclose(stats);
  assert(text.find("oracle_elastic_l2_class_occupancy") != std::string::npos);
  assert(text.find("class_1=1") != std::string::npos);
  assert(text.find("class_2=0") != std::string::npos);

  exposed_tag_array ordinary(sector_config, 0, 0);
  ordinary.install_ordinary_sector_line(0);
  ordinary.fill_second_sector_without_new_admission(0);
  assert(ordinary.ordinary_valid() == 1);
  mem_access_sector_mask_t sector0;
  sector0.set(0);
  ordinary.oracle_invalidate_line(0, sector0);
  assert(ordinary.ordinary_valid() == 1);
  mem_access_sector_mask_t sector1;
  sector1.set(1);
  ordinary.oracle_invalidate_line(0, sector1);
  assert(ordinary.ordinary_valid() == 0);
  ordinary.oracle_verify_invariant();

  const char *m1f_sidecar = "/tmp/c16_oracle_m1f_tag_array_sidecar.tsv";
  std::ofstream m1f_file(m1f_sidecar, std::ios::binary);
  m1f_file << "ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1\t1\tMODELED_L2_GET_ADDR\n";
  const unsigned long long m1f_region_bytes = 33947648ULL;
  const unsigned long long m1f_stride = m1f_region_bytes + 128;
  const unsigned long long m1f_base = 0x100000000ULL;
  for (unsigned target_class = 1; target_class <= 28; ++target_class) {
    const unsigned long long begin =
        m1f_base + (target_class - 1) * m1f_stride;
    m1f_file << "0x" << std::hex << begin << "\t0x"
             << begin + m1f_region_bytes << std::dec << "\tlayer"
             << target_class - 1 << ".up_proj.qweight\t"
             << target_class << "\n";
  }
  m1f_file.close();
  std::string m1f_digest =
      oracle_elastic_residency::oracle_config::sha256_file(m1f_sidecar);
  cache_config m1f_config;
  m1f_config.m_config_string = sector_string;
  m1f_config.init(sector_string, FuncCachePreferNone);
  m1f_config.m_oracle_elastic_enable = true;
  m1f_config.m_oracle_elastic_quota_bytes = 0;
  m1f_config.m_oracle_elastic_quota_lines = 131072;
  m1f_config.m_oracle_elastic_sidecar = const_cast<char *>(m1f_sidecar);
  m1f_config.m_oracle_elastic_sidecar_sha256 =
      const_cast<char *>(m1f_digest.c_str());
  m1f_config.m_oracle_elastic_diagnostics = false;
  m1f_config.m_oracle_m1f_stable_admission_enable = true;
  m1f_config.init_oracle_elastic(16);
  assert(m1f_config.oracle_m1f_stable_admission_enabled());
  assert(m1f_config.oracle_elastic_global_lines() == 131072);
  bool found_selected = false;
  bool found_filtered = false;
  for (unsigned line = 0; line < 4096; ++line) {
    const unsigned long long address = m1f_base + line * 128ULL;
    unsigned target_class = 0;
    uint64_t line_index = ~uint64_t(0);
    uint64_t hash = 0;
    const bool selected =
        m1f_config.oracle_elastic_address_protection_eligible(
            address, &target_class, &line_index, &hash);
    assert(target_class == 1 && line_index == line);
    assert(selected ==
           m1f_config.oracle_elastic_address_protection_eligible(address + 32));
    found_selected |= selected;
    found_filtered |= !selected;
  }
  assert(found_selected && found_filtered);
  tag_array m1f_array(m1f_config, 0, 0);
  assert(m1f_array.oracle_protected_quota() == 8192);
  m1f_array.oracle_verify_invariant();

  remove(sidecar);
  remove(m1f_sidecar);
  std::cout << "ORACLE_ELASTIC_TAG_ARRAY_CONFIG_TEST_PASS\n";
  return 0;
}
