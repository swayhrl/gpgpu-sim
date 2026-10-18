#include "dtc-l1-collect-wb.h"
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

template<class Function> void must_abort(Function function) {
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    rlimit limit{0, 0}; setrlimit(RLIMIT_CORE, &limit);
    function(); _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

int main() {
  dtc_l1::collect_wb_pipeline pipe;
  pipe.configure(640);
  assert(pipe.bank_capacity_sectors * 32 == 80 * 1024);
  pipe.claim(1, 2560);
  for (unsigned cycle = 1; cycle <= 640; ++cycle) pipe.consumed(cycle, 4);
  pipe.promote();
  const unsigned old_result_bank = pipe.result_bank;
  pipe.claim(2, 2560);
  for (unsigned cycle = 641; cycle <= 1280; ++cycle) pipe.consumed(cycle, 4);
  assert(pipe.collecting_valid && pipe.result_valid);
  assert(pipe.collecting_sectors + pipe.result_sectors == 5120);
  assert(pipe.peak_buffered_sectors * 32 == 160 * 1024);
  pipe.attempted(1281, false);
  assert(pipe.result_valid && pipe.result_uid == 1 && pipe.result_bank == old_result_bank);
  pipe.attempted(1282, true);
  pipe.retired(1282, 1, true);
  pipe.promote();
  assert(pipe.result_uid == 2 && pipe.result_bank != old_result_bank);
  pipe.attempted(1283, true);
  pipe.retired(1283, 2, true);
  assert(!pipe.result_valid && !pipe.collecting_valid);
  // Same absolute-cycle retirement after the collection sample stays in bins.
  pipe.claim(3, 4);
  pipe.consumed(1284, 4);
  pipe.promote();
  pipe.sample(1285, 1, 0, false, true);
  pipe.attempted(1285, true);
  pipe.retired(1285, 3, true);
  assert(pipe.retire_hist[1] == 1 && pipe.retire_hist[0] == 0);
  must_abort([] { dtc_l1::collect_wb_pipeline p; p.configure(640); p.claim(1, 2561); });
  must_abort([] { dtc_l1::collect_wb_pipeline p; p.configure(640); p.claim(1, 4); p.claim(2, 4); });
  must_abort([] { dtc_l1::collect_wb_pipeline p; p.configure(640); p.claim(1, 8); p.consumed(1, 4); p.consumed(1, 1); });
  must_abort([] { dtc_l1::collect_wb_pipeline p; p.configure(640); p.retired(1, 1, false); p.retired(1, 2, false); });
  must_abort([] { dtc_l1::collect_wb_pipeline p; p.configure(640); p.claim(1, 4); p.consumed(1, 4); p.promote(); p.attempted(2, false); p.attempted(2, true); });
  // Existing physical generation guard remains hard, not relaxed for a slot.
  must_abort([] {
    dtc_l1::config cfg; cfg.logical_sets = cfg.logical_ways = cfg.physical_lines = 1;
    dtc_l1::oo_frontend front(cfg);
    assert(front.admit(1)); const auto old = front.access(1,1,0).physical;
    front.complete(old); assert(front.retire_ready_uid(2,1));
    assert(front.admit(2)); const auto next = front.access(3,2,128).physical;
    assert(old.id == next.id && old.generation != next.generation);
    assert(!front.fill_identity_matches(old)); front.complete(old);
  });
  puts("PASS_BOUNDED_TWO_BANK_160KiB_RESOURCE_MODEL_AND_6_HARD_NEGATIVES");
}
