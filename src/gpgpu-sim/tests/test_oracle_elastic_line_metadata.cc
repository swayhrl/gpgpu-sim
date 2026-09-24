#include "../gpu-cache.h"

#include <assert.h>
#include <iostream>

int main() {
  mem_access_sector_mask_t sector0;
  sector0.set(0);
  mem_access_byte_mask_t bytes;

  line_cache_block line;
  assert(line.is_invalid_line());
  assert(!line.oracle_protected() && !line.oracle_pending_protected());
  line.allocate(1, 0x1000, 1, sector0);
  line.oracle_set_pending(true, 7);
  assert(line.is_reserved_line() && line.oracle_pending_protected());
  assert(!line.oracle_protected());
  line.fill(2, sector0, bytes);
  line.oracle_commit_pending();
  assert(line.is_valid_line() && line.oracle_protected());
  assert(line.oracle_target_class() == 7);
  line.oracle_clear_metadata();
  assert(!line.oracle_protected() && !line.oracle_pending_protected());

  sector_cache_block sector;
  sector.allocate(2, 0x2000, 3, sector0);
  sector.oracle_set_pending(true, 9);
  sector.fill(4, sector0, bytes);
  sector.oracle_commit_pending();
  assert(!sector.is_invalid_line() && sector.oracle_protected());
  mem_access_sector_mask_t sector1;
  sector1.set(1);
  sector.allocate_sector(5, sector1);
  assert(sector.oracle_protected());  // sector miss is a hit on line metadata.
  sector.fill(6, sector1, bytes);
  assert(sector.oracle_protected());
  sector.oracle_clear_metadata();
  assert(!sector.oracle_protected());

  std::cout << "ORACLE_ELASTIC_LINE_METADATA_TEST_PASS\n";
  return 0;
}
