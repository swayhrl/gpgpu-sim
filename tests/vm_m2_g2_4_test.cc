#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "gpgpu-sim/vm_translation.h"

// This is a narrow model of the ldst-unit contract: a coalesced transaction
// retains its UID while translation stalls, and downstream effects happen only
// once after READY.  It deliberately covers store and atomic equivalently,
// because both traverse ldst_unit::memory_cycle before a mem_fetch is made.
class replayed_transaction {
 public:
  replayed_transaction(vm_translation::translation_controller *vm, uint64_t va,
                       uint64_t uid)
      : m_vm(vm), m_va(va), m_uid(uid), m_translated(false), m_retired(false),
        m_data_effects(0) {}
  bool cycle(uint64_t cycle) {
    if (m_retired) return true;
    if (!m_translated) {
      uint64_t pa = 0;
      if (m_vm->translate(0, 0, m_va, cycle, m_uid, &pa) !=
          vm_translation::READY)
        return false;
      assert(pa == m_va);
      m_translated = true;
    }
    ++m_data_effects;
    m_retired = true;
    return true;
  }
  unsigned data_effects() const { return m_data_effects; }

 private:
  vm_translation::translation_controller *m_vm;
  uint64_t m_va;
  uint64_t m_uid;
  bool m_translated;
  bool m_retired;
  unsigned m_data_effects;
};

int main() {
  const uint64_t page = 64ULL * 1024ULL;
  vm_translation::translation_controller vm(vm_translation::translation_config(
      1, page, vm_translation::tlb_config(4, 4, 8),
      vm_translation::tlb_config(4, 4, 8), 4, 4, 1, 2));
  replayed_transaction store(&vm, page + 8, 101);
  replayed_transaction atomic(&vm, 2 * page + 8, 202);
  assert(!store.cycle(0));
  assert(!atomic.cycle(0));
  assert(store.data_effects() == 0 && atomic.data_effects() == 0);
  vm.cycle(0);
  vm.cycle(2);
  assert(store.cycle(3));
  vm.cycle(4);
  assert(atomic.cycle(5));
  assert(store.cycle(6));
  assert(atomic.cycle(6));
  assert(store.data_effects() == 1 && atomic.data_effects() == 1);
  assert(vm.quiescent_invariants_hold());
  assert(vm_core::transaction_crosses_page(page - 32, 64, page));
  printf("vm_m2_g2_4_test PASS\n");
  return 0;
}
