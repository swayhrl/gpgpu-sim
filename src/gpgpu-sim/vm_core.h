// Minimal VM-core helpers shared by the M1 address contract and later stages.
#ifndef GPGPU_SIM_VM_CORE_H
#define GPGPU_SIM_VM_CORE_H

#include <assert.h>
#include <stdint.h>

namespace vm_core {
static const uint64_t kDefaultBasePageSize = 64ULL * 1024ULL;
inline bool valid_page_size(uint64_t page_size) {
  return page_size != 0 && (page_size & (page_size - 1)) == 0;
}
inline uint64_t vpn(uint64_t sim_va, uint64_t page_size) {
  assert(valid_page_size(page_size));
  return sim_va / page_size;
}
inline uint64_t page_offset(uint64_t sim_va, uint64_t page_size) {
  assert(valid_page_size(page_size));
  return sim_va % page_size;
}
inline bool transaction_crosses_page(uint64_t sim_va, uint64_t size,
                                     uint64_t page_size) {
  assert(valid_page_size(page_size));
  return size != 0 && vpn(sim_va, page_size) !=
                          vpn(sim_va + size - 1, page_size);
}
inline uint64_t identity_translate(uint64_t sim_va, uint64_t page_size) {
  return vpn(sim_va, page_size) * page_size + page_offset(sim_va, page_size);
}
}  // namespace vm_core

#endif
