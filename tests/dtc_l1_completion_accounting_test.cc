#include <cassert>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "dtc-l1-common.h"

static void close_exactly_once(unsigned dependencies) {
  dtc_l1::completion_accounting accounting;
  accounting.register_dependencies(dependencies);
  accounting.own_pib_dependencies(dependencies);
  accounting.mark_ready(dependencies);
  assert(accounting.close_once(dependencies) == dependencies);
  assert(accounting.registered() == dependencies);
  assert(accounting.pib_owned() == dependencies);
  assert(accounting.closed() == dependencies);
  assert(accounting.state() == dtc_l1::completion_accounting::lifecycle::CLOSED);
}

int main() {
  // C01: the production completion state machine closes each exact 128B
  // cardinality once, without re-deriving it from a later cache event.
  for (const unsigned dependencies : std::vector<unsigned>{1, 2, 4, 32})
    close_exactly_once(dependencies);

  // C02: multiple 32B sectors of one 128B line are one completion dependency.
  const std::vector<dtc_l1::sector_access> sectors = {
      {0, 0x1}, {32, 0x2}, {64, 0x4}, {96, 0x8}};
  const auto references = dtc_l1::group_128b_references(sectors);
  assert(references.size() == 1);
  close_exactly_once(static_cast<unsigned>(references.size()));

  // C03: a second dynamic close of the same UID's accounting object is a hard
  // invariant violation, not a second scoreboard release.
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    dtc_l1::completion_accounting accounting;
    accounting.register_dependencies(2);
    accounting.own_pib_dependencies(2);
    accounting.mark_ready(2);
    accounting.close_once(2);
    accounting.close_once(2);  // Must assert.
    _exit(1);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
  return 0;
}
