#include "../gpu-cache.h"

#include <assert.h>
#include <stdio.h>

#include <fstream>
#include <iostream>

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

  remove(sidecar);
  std::cout << "ORACLE_ELASTIC_TAG_ARRAY_CONFIG_TEST_PASS\n";
  return 0;
}
