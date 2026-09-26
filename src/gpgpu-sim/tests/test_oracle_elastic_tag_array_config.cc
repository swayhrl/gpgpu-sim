#include "../gpu-cache.h"

#include <assert.h>
#include <stdio.h>

#include <fstream>
#include <iostream>

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
  sector_config.init_oracle_elastic(1);
  exposed_tag_array sectors(sector_config, 0, 0);
  sectors.install_protected_sector_line(0);
  assert(sectors.oracle_protected_occupancy() == 1);
  assert(sectors.get_block(0)->oracle_protected());
  sectors.fill_second_sector_without_new_admission(0);
  assert(sectors.oracle_protected_occupancy() == 1);
  assert(sectors.get_block(0)->oracle_protected());
  sectors.oracle_verify_invariant();

  remove(sidecar);
  std::cout << "ORACLE_ELASTIC_TAG_ARRAY_CONFIG_TEST_PASS\n";
  return 0;
}
