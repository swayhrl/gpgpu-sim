#include "dtc-l1-common.h"

#include <cassert>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

// O13 negative harness.  A child injects a delayed lower fill carrying the
// generation of a recycled physical allocation.  The parent passes only when
// the front-end completion invariant terminates that child with SIGABRT.
int main() {
  dtc_l1::config cfg;
  cfg.selected_mode = dtc_l1::mode::PAPER_OO;
  cfg.logical_sets = 1;
  cfg.logical_ways = 1;
  cfg.physical_lines = 1;
  cfg.oo_pib_entries = 1;
  dtc_l1::oo_frontend front_end(cfg);
  front_end.admit(1);
  const dtc_l1::io_access_result old = front_end.access(1, 1, 0);
  front_end.complete(old.physical);
  front_end.retire_one_ready(2);
  front_end.admit(2);
  const dtc_l1::io_access_result replacement =
      front_end.access(3, 2, dtc_l1::kLogicalLineBytes);
  (void)replacement;
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    front_end.complete(old.physical);  // Must assert: stale generation.
    _exit(1);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
  return 0;
}
