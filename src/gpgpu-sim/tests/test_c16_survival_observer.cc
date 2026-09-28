#include "../c16_survival_observer.h"

#include <assert.h>
#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

using c16_survival_observer::generation_observer;
using c16_survival_observer::report;

int main() {
  const char *off_path = "/tmp/c16_survival_observer_off.tsv";
  remove(off_path);
  generation_observer off;
  off.configure(false, off_path, 1, 1, 2, 3, 4);
  off.on_line_allocate(0x1000, true, 1);
  off.on_fill(0x1000);
  assert(off.generation(0x1000) == 0);
  std::ifstream absent(off_path);
  assert(!absent.good());

  const char *path = "/tmp/c16_survival_observer.tsv";
  generation_observer observer;
  observer.configure(true, path, 1, 1, 2, 3, 4);
  const uint64_t old = 0x1000;
  const uint64_t ordinary = 0x2000;
  const uint64_t other_class = 0x3000;
  observer.on_line_allocate(old, true, 1);
  observer.on_fill(old);
  observer.on_line_allocate(ordinary, false, 1);
  observer.on_fill(ordinary);
  observer.on_line_allocate(other_class, true, 2);
  observer.on_fill(other_class);
  assert(observer.generation(old) == 1);
  observer.kernel_end(1);  // D1 L0 snapshot
  report value = observer.current_report();
  assert(value.snapshot_population == 1);
  assert(value.exact_old_generation_survivors == 1);
  assert(value.current_absent == 0 && value.accounting_closed);

  observer.kernel_begin(2);  // D2 L0 before
  observer.on_true_hit(old);
  value = observer.current_report();
  assert(value.old_generation_d2_hits == 1);

  observer.on_evict(old);
  value = observer.current_report();
  assert(value.exact_old_generation_survivors == 0);
  assert(value.current_absent == 1 && value.accounting_closed);

  observer.on_line_allocate(old, true, 1);
  observer.on_fill(old);
  assert(observer.generation(old) == 4);
  value = observer.current_report();
  assert(value.current_new_generation_residents == 1);
  assert(value.evicted_then_refilled_same_address == 1);
  assert(value.old_generation_d2_reloads == 1 && value.accounting_closed);

  observer.on_evict(old);
  observer.on_line_allocate(old, true, 1);
  observer.on_fill(old);
  assert(observer.generation(old) == 5);
  value = observer.current_report();
  assert(value.evicted_then_refilled_same_address == 1);
  assert(value.old_generation_d2_reloads == 2);
  assert(value.current_new_generation_residents == 1 &&
         value.accounting_closed);
  observer.kernel_begin(3);
  observer.kernel_end(4);

  std::ifstream input(path);
  std::stringstream contents;
  contents << input.rdbuf();
  const std::string text = contents.str();
  assert(text.find("D1_L0_SNAPSHOT\t1\t1\t1") != std::string::npos);
  assert(text.find("D2_L0_BEFORE\t2\t1") != std::string::npos);
  assert(text.find("D2_L0_MID\t3\t1") != std::string::npos);
  assert(text.find("D2_L0_END\t4\t1") != std::string::npos);
  remove(path);
  puts("C16_OLD_ADDRESS_SURVIVAL_OBSERVER_TEST_PASS");
  return 0;
}
