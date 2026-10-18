// CPU-only production-header state machine checks; no payload/value oracle.
#include "dtc-l1-common.h"
#include <csignal>
#include <functional>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace dtc_l1;
static config settings(bool enabled = true) {
  config c;
  c.selected_mode = mode::PAPER_OO;
  c.finite_service = true;
  c.progressive_wakeup = enabled;
  c.oo_pib_entries = 16;
  c.physical_lines = 2;
  c.logical_sets = c.logical_ways = 1;
  return c;
}
static void rejects(const std::function<void()> &f) {
  const pid_t pid = fork(); assert(pid >= 0);
  if (!pid) { f(); _exit(0); }
  int status; assert(waitpid(pid, &status, 0) == pid);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}
static void fanout(unsigned n, bool on) {
  oo_frontend f(settings(on));
  f.advance_service(10);
  physical_identity id{};
  for (unsigned uid = 1; uid <= n; ++uid) {
    assert(f.admit(uid)); id = f.access(10, uid, 0).physical;
  }
  f.complete(id);
  assert(!f.entry_ready_for(1));
  f.advance_service(10); // complete->advance cannot collapse the +1 boundary
  assert(!f.entry_ready_for(1) && f.wakeups() == 0);
  for (unsigned edge = 1; edge <= (n + 3) / 4; ++edge) {
    f.advance_service(10 + edge);
    const unsigned notified = std::min(n, edge * 4);
    assert(f.wakeups() == notified && f.ref_count(id) == n);
    for (unsigned uid = 1; uid <= n; ++uid)
      assert(f.entry_ready_for(uid) == (on ? uid <= notified : notified == n));
    f.advance_service(10 + edge);
    assert(f.wakeups() == notified); // no same-cycle quota reuse
  }
  for (unsigned uid = 1; uid <= n; ++uid) {
    assert(f.retire_ready_uid(20 + uid, uid));
    if (uid != n) assert(!f.retire_ready_uid(20 + uid, uid + 1));
  }
  assert(f.ref_count(id) == 0 && f.is_allocated(id));
  assert(f.invalidate_lookup(50).reclaimed == 1 && !f.is_allocated(id));
  std::printf("PASS fanout=%u progressive=%u quota=4 plus1=preserved\n", n, on);
}
static void lifecycle() {
  oo_frontend f(settings());
  f.advance_service(1);
  physical_identity old{};
  for (unsigned uid = 1; uid <= 9; ++uid) {
    assert(f.admit(uid)); old = f.access(1, uid, 0).physical;
  }
  f.complete(old); f.advance_service(2);
  const auto slot = f.entry_slot(1); const auto gen = f.entry_slot_generation(1);
  assert(f.retire_ready_uid(2, 1)); // leaves processed stale waiter slot behind
  assert(f.admit(10) && f.entry_slot(10) == slot && f.entry_slot_generation(10) > gen);
  assert(f.access(2, 10, 0).kind == io_access_kind::PENDING_HIT);
  // Repeated same-ref admission is not a second waiter/Ref.
  f.access(2, 10, 0);
  assert(!f.entry_ready_for(10) && f.ref_count(old) == 9);
  f.advance_service(2); assert(f.wakeups() == 4 && !f.entry_ready_for(10));
  const auto lost = f.invalidate_lookup(2);
  assert(lost.changed == 1 && lost.retained == 1 && !f.tag_visible(old));
  assert(f.admit(11));
  const auto newer = f.access(3, 11, 0).physical;
  assert(newer.id != old.id && f.is_allocated(old));
  f.complete(newer);
  f.advance_service(3); assert(f.wakeups() == 8 && !f.entry_ready_for(9) && !f.entry_ready_for(10));
  f.advance_service(4); assert(f.wakeups() == 11 && f.entry_ready_for(10) && f.entry_ready_for(11));
  for (unsigned uid = 2; uid <= 10; ++uid) assert(f.retire_ready_uid(10 + uid, uid));
  assert(!f.is_allocated(old) && f.is_allocated(newer));
  assert(f.retire_ready_uid(21, 11));
  assert(f.admit(12));
  assert(f.access(22, 12, 0).kind == io_access_kind::VALID_HIT);
  assert(f.entry_ready_for(12) && f.wakeups() == 11); // after end: no new broadcast
  f.add_external_dependency(12); assert(!f.entry_ready_for(12));
  f.complete_external_dependency(12); assert(f.entry_ready_for(12));
  f.invalidate_lookup(23); f.advance_service(24); assert(f.retire_ready_uid(24, 12));
  assert(f.allocated_lines() == 0 && f.active_refs() == 0);
  // Recycle the physical slot. An old return must fail, never wake new owners.
  assert(f.admit(13)); f.access(25, 13, 128);
  assert(f.admit(14)); f.access(26, 14, 256);
  assert(!f.fill_identity_matches(old));
  rejects([&]{ f.complete(old); });
  f.print_progressive_summary(stdout, 0);
  std::puts("PASS late_merge slot_reuse generation K1 old_return final_ref external_dependency");
}
static void multi_reference_and_replacement() {
  oo_frontend f(settings()); f.advance_service(1);
  assert(f.admit(1)); auto a = f.access(1, 1, 0).physical;
  auto b = f.access(1, 1, 128).physical; // tag victim retains old physical Ref
  assert(!f.tag_visible(a) && f.ref_count(a) == 1);
  assert(f.admit(2));
  assert(f.access(1, 2, 256).kind == io_access_kind::NO_FREE_LINE); // partial admission: no rollback
  assert(f.ref_count(a) == 1 && f.ref_count(b) == 1);
  f.complete(a); f.advance_service(2); assert(!f.entry_ready_for(1));
  f.complete(b); f.advance_service(3); assert(f.entry_ready_for(1));
  // Successful retirement is the only ref decrement; readiness did not free a.
  assert(f.ref_count(a) == 1 && f.is_allocated(a));
  f.advance_service(4);
  assert(f.retire_ready_uid(4, 1) && !f.is_allocated(a));
  assert(f.access(4, 2, 256).kind == io_access_kind::NEW_MISS);
  f.assert_shadow_refs();
  std::puts("PASS multiple_allocations partial_allocation tag_victim last_ref");
}
int main() {
  rlimit limit{0, 0}; setrlimit(RLIMIT_CORE, &limit);
  for (unsigned n : {4, 5, 8, 9}) for (bool on : {false, true}) fanout(n, on);
  lifecycle(); multi_reference_and_replacement();
  for (unsigned selected = 0; selected <= 4; ++selected)
    for (bool finite : {false, true}) {
      if (selected == 3 && finite) continue;
      rejects([=]{ check_progressive_wakeup_mode(true, selected, finite); });
    }
  rejects([]{ config c=settings(); io_frontend wrong(c); });
  rejects([]{ config c=settings(); sector_oo_frontend wrong(c); });
  rejects([]{ config c=settings(); paper_frontend wrong(c); });
  rejects([]{ oo_frontend f(settings()); f.advance_service(1); f.admit(1);
    auto id=f.access(1,1,0).physical; f.complete(id); f.complete(id); });
  assert(!response_sector_is_new(15, 1, 0));
  assert(!response_sector_is_new(15, 0, 4));
  std::printf("PASS fail_closed sizeof_config=%zu sizeof_service_entry=%zu sizeof_oo=%zu\n",
      sizeof(config), sizeof(service_entry), sizeof(oo_frontend));
}
