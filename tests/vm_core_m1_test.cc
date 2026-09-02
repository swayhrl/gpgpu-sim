#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_core.h"

int main() {
  const uint64_t page = vm_core::kDefaultBasePageSize;
  assert(vm_core::valid_page_size(page));
  assert(!vm_core::valid_page_size(0));
  assert(!vm_core::valid_page_size(3));

  const uint64_t sim_va = 0x12340000ULL + 0x4321ULL;
  assert(vm_core::page_offset(sim_va, page) == 0x4321ULL);
  assert(vm_core::identity_translate(sim_va, page) == sim_va);
  assert(!vm_core::transaction_crosses_page(0x10000ULL, 128, page));
  assert(vm_core::transaction_crosses_page(page - 64, 128, page));

  printf("vm_core_m1_test PASS page=%llu simva=0x%llx\n",
         (unsigned long long)page, (unsigned long long)sim_va);
  return 0;
}
