#include "dtc-l1-common.h"
#include <csignal>
#include <cstdio>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace dtc_l1;

static config small(bool on, bool finite = true, unsigned physical = 4) {
  config c;
  c.selected_mode = mode::PAPER_OO;
  c.reclaimable_victim_fallback = on;
  c.finite_service = finite;
  c.logical_sets = 2;
  c.logical_ways = 3;
  c.physical_lines = physical;  // 4 physical < 6 directory positions
  c.oo_pib_entries = 32;
  return c;
}

static void witness(const char *name, const oo_frontend &f) {
  f.assert_shadow_refs();
  std::printf("{\"case\":\"%s\",\"status\":\"PASS\","
              "\"fallbacks\":%llu,\"no_free_events\":%llu,"
              "\"width_events\":%llu,\"allocated\":%zu,\"refs\":%llu,"
              "\"map_loss_count\":%llu,\"map_loss_sum\":%llu}\n", name,
              (unsigned long long)f.reclaimable_victim_fallbacks(),
              (unsigned long long)f.no_free_physical_events(),
              (unsigned long long)f.allocation_width_limited_events(),
              f.allocated_lines(), (unsigned long long)f.active_refs(),
              (unsigned long long)f.map_loss_to_free_count(),
              (unsigned long long)f.map_loss_to_free_sum());
}

template <class Action> static void must_abort(Action action) {
  const pid_t child = fork();
  assert(child >= 0);
  if (!child) { action(); _exit(3); }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

// A is the oldest set0 mapping with live consumers and a pending return.
// B, C are newer ready Ref0 set0 mappings; X is a ready Ref0 set1 mapping.
// No private-state edits, artificial free, ordinary MSHR, or simulator cycle.
struct fixture {
  oo_frontend f;
  physical_identity a, b, c, x;
  unsigned refs;
  fixture(bool on, bool finite = true, unsigned physical = 4,
          bool keep_bc_live = false, unsigned high_refs = 3)
      : f(small(on, finite, physical)), refs(high_refs) {
    f.advance_service(1);
    assert(f.admit(1));
    a = f.access(1, 1, 0).physical;
    for (unsigned uid = 2; uid <= refs; ++uid) {
      assert(f.admit(uid));
      assert(f.access(1, uid, 0).kind == io_access_kind::PENDING_HIT);
    }
    f.advance_service(2);
    assert(f.admit(10)); b = f.access(2, 10, 256).physical; f.complete(b);
    f.advance_service(3);
    if (!keep_bc_live) assert(f.retire_ready_uid(3, 10));
    assert(f.admit(11)); c = f.access(3, 11, 512).physical; f.complete(c);
    f.advance_service(4);
    if (!keep_bc_live) assert(f.retire_ready_uid(4, 11));
    assert(f.admit(12)); x = f.access(4, 12, 128).physical; f.complete(x);
    f.advance_service(5);
    assert(f.retire_ready_uid(5, 12));
    assert(f.ref_count(a) == refs);
    assert(f.allocated_lines() == 4);
  }
  io_access_result request(uint64_t cycle = 6, uint64_t address = 768) {
    f.advance_service(cycle); assert(f.admit(20));
    return f.access(cycle, 20, address);
  }
};

static void opportunity(bool on, bool finite) {
  fixture t(on, finite);
  const auto r = t.request();
  if (on) {
    assert(r.kind == io_access_kind::NEW_MISS);
    assert(r.physical.id == t.b.id && r.physical.generation == t.b.generation+1);
    assert(!t.f.is_allocated(t.b));
    assert(t.f.reclaimable_victim_fallbacks() == 1);
    assert(t.f.immediate_reclaims() == 1 && !t.f.deferred_reclaims());
    assert(t.f.map_loss_to_free_count() == (finite ? 1U : 0U));
    assert(t.f.map_loss_to_free_sum() == 0);
  } else {
    assert(r.kind == io_access_kind::NO_FREE_LINE);
    assert(t.f.no_free_physical_events() == 1 && !t.f.tag_evictions());
    assert(t.f.tag_visible(t.b));
  }
  // A's consumers/old identity and the younger safe C/other-set X survive.
  assert(t.f.is_allocated(t.a) && t.f.tag_visible(t.a) && t.f.ref_count(t.a)==3);
  assert(t.f.is_allocated(t.c) && t.f.tag_visible(t.c) && !t.f.ref_count(t.c));
  assert(t.f.is_allocated(t.x) && t.f.tag_visible(t.x) && !t.f.ref_count(t.x));
  const auto count = t.f.reclaimable_victim_fallbacks();
  for (unsigned i=0; i<8; ++i) {
    assert(t.f.reclaimable_victim_fallbacks() == count);
    t.f.assert_shadow_refs();
  }
  witness(on ? (finite ? "AF01_ON_oldest_safe_same_set_finite" : "AF01_ON_nonfinite")
             : (finite ? "AF01_OFF_refuse_finite" : "AF01_OFF_refuse_nonfinite"), t.f);
}

static void no_candidate(bool on) {
  fixture t(on, true, 4, true);
  assert(t.request().kind == io_access_kind::NO_FREE_LINE);
  // X in the other set is reclaimable, but cannot be used here.
  assert(t.f.tag_visible(t.x) && t.f.ref_count(t.x)==0);
  assert(!t.f.reclaimable_victim_fallbacks() && !t.f.tag_evictions());
  assert(t.f.no_free_physical_events()==1);
  witness(on ? "AF02_ON_all_target_live_no_cross_set" : "AF02_OFF_all_target_live", t.f);
}

static void free_pool(bool on) {
  fixture t(on, true, 5);
  const auto r=t.request();
  assert(r.kind==io_access_kind::NEW_MISS && r.physical.id!=t.b.id);
  assert(t.f.is_allocated(t.a) && !t.f.tag_visible(t.a) && t.f.ref_count(t.a)==3);
  assert(t.f.tag_visible(t.b) && t.f.deferred_reclaims()==1);
  assert(!t.f.reclaimable_victim_fallbacks());
  witness(on ? "AF03_ON_free_pool_original_LRU" : "AF03_OFF_free_pool_original_LRU",t.f);
}

static void original_safe(bool on) {
  fixture t(on);
  t.f.complete(t.a);
  for (unsigned uid=1;uid<=3;++uid) {
    t.f.advance_service(5+uid); assert(t.f.retire_ready_uid(5+uid,uid));
  }
  const auto r=t.request(9);
  assert(r.kind==io_access_kind::NEW_MISS && r.physical.id==t.a.id);
  assert(t.f.tag_visible(t.b) && !t.f.reclaimable_victim_fallbacks());
  witness(on ? "AF04_ON_original_safe_LRU_unchanged" : "AF04_OFF_original_safe_LRU",t.f);
}

static void invalid_original(bool on) {
  config cfg=small(on); cfg.physical_lines=3;
  oo_frontend f(cfg);
  f.advance_service(1);assert(f.admit(1));const auto a=f.access(1,1,0).physical;
  f.advance_service(2);assert(f.admit(2));const auto b=f.access(2,2,256).physical;f.complete(b);
  f.advance_service(3);assert(f.retire_ready_uid(3,2));
  assert(f.admit(3));const auto x=f.access(3,3,128).physical;
  f.advance_service(4);assert(f.admit(4));
  assert(f.access(4,4,512).kind==io_access_kind::NO_FREE_LINE);
  assert(f.is_allocated(a) && f.is_allocated(x) && f.tag_visible(b));
  assert(!f.reclaimable_victim_fallbacks());
  witness(on ? "AF05_ON_invalid_original_no_fallback" : "AF05_OFF_invalid_original",f);
}

static void retry_and_quota() {
  fixture t(true);
  // One same-bank grant; rejected grant cannot call the allocation policy.
  assert(t.f.try_serve_tag(6,768));
  assert(!t.f.try_serve_tag(6,1024));
  assert(t.f.tag_conflicts()==1 && !t.f.reclaimable_victim_fallbacks());
  service_entry grant;grant.tag_grant_cycle=6;
  assert(!grant.tag_can_commit(6) && grant.tag_can_commit(7));
  const auto r=t.request(7);
  assert(r.kind==io_access_kind::NEW_MISS);
  assert(t.f.access(7,20,768).kind==io_access_kind::PENDING_HIT);
  assert(t.f.ref_count(r.physical)==1);  // same admitted ref never double-attaches
  assert(t.f.admit(21));
  const auto hit=t.f.access(7,21,768);
  assert(hit.kind==io_access_kind::PENDING_HIT && hit.physical.id==r.physical.id);
  assert(t.f.ref_count(r.physical)==2 && t.f.reclaimable_victim_fallbacks()==1);
  assert(t.f.new_misses()==5);
  witness("AF06_relookup_same_line_and_tag_quota",t.f);

  config c=small(true);c.allocation_width=1;
  oo_frontend f(c);
  f.advance_service(1);assert(f.admit(1));assert(f.access(1,1,0).kind==io_access_kind::NEW_MISS);
  assert(f.admit(2));assert(f.access(1,2,256).kind==io_access_kind::NO_FREE_LINE);
  assert(f.allocation_width_limited_events()==1 && !f.no_free_physical_events());
  assert(!f.reclaimable_victim_fallbacks());
  for(unsigned uid=3;uid<=32;++uid) assert(f.admit(uid));
  assert(!f.admit(33));assert(!f.reclaimable_victim_fallbacks());
  witness("AF07_allocation_width_and_PIB_gate_not_opportunity",f);
}

static void pending_K1_return_and_WB() {
  fixture t(true,true,4,false,6);
  const auto next=t.request();assert(next.kind==io_access_kind::NEW_MISS);
  must_abort([&]{t.f.complete(t.b);}); // recycled physical generation
  t.f.advance_service(7);
  const auto delta=t.f.invalidate_lookup(7);
  assert(delta.changed==4 && delta.retained==2 && delta.reclaimed==2);
  assert(!t.f.tag_visible(t.a) && t.f.ref_count(t.a)==6);
  assert(!t.f.tag_visible(next.physical) && t.f.ref_count(next.physical)==1);
  assert(!t.f.invalidate_lookup(7).changed && !t.f.invalidate_lookup(8).changed);
  // Saved identities, independent sector masks: no address relookup on fill.
  uint8_t old_mask=0,new_mask=0;
  for(unsigned s:{2U,0U,3U,1U}) {
    assert(response_sector_is_new(0xF,old_mask,s));old_mask|=1U<<s;
    assert(!response_sector_is_new(0xF,old_mask,s));
  }
  assert(!response_sector_is_new(0x1,0,3));
  for(unsigned s:{3U,1U,0U,2U}) { assert(response_sector_is_new(0xF,new_mask,s));new_mask|=1U<<s; }
  assert(old_mask==0xF && new_mask==0xF);
  t.f.complete(t.a);t.f.complete(next.physical);
  must_abort([&]{t.f.complete(next.physical);}); // duplicate queued completion
  t.f.advance_service(8);
  assert(t.f.wakeups()==7); // B,C,X=3 plus first4 A waiters
  assert(!t.f.entry_ready_for(1));
  t.f.advance_service(8);assert(t.f.wakeups()==7);
  t.f.advance_service(9);assert(t.f.wakeups()==10); // remaining2 A and new1
  assert(t.f.entry_ready_for(1) && t.f.entry_ready_for(20));
  must_abort([&]{t.f.complete(t.a);}); // duplicate ready-line completion
  service_entry collect;service_quota port(4);collect.observe_ready(9,4);
  assert(!collect.collect(9,port));assert(!collect.collect(10,port));
  assert(collect.collect(11,port));assert(collect.collect(11,port));
  // Controlled WB denial: production LDST returns before frontend retire.
  // This header fixture checks that no caller retirement means no Ref/free.
  assert(t.f.ref_count(t.a)==6 && t.f.ref_count(next.physical)==1);
  assert(t.f.map_loss_to_free_count()==3 && t.f.map_loss_to_free_sum()==0);
  for(unsigned uid=1;uid<=6;++uid) {
    t.f.advance_service(11+uid);assert(t.f.retire_ready_uid(11+uid,uid));
    if(uid<6) assert(t.f.is_allocated(t.a));
    assert(!t.f.retire_ready_uid(11+uid,20)); // one retire/cycle
  }
  t.f.advance_service(18);assert(t.f.retire_ready_uid(18,20));
  assert(!t.f.is_allocated(t.a) && !t.f.is_allocated(next.physical));
  assert(t.f.active_refs()==0 && t.f.allocated_lines()==0 && t.f.occupancy()==0);
  assert(t.f.map_loss_to_free_count()==5 && t.f.map_loss_to_free_sum()==21);
  assert(t.f.reclaimable_victim_fallbacks()==1);
  witness("AF08_K1_late_return_generation_sectors_finite_WB_guard",t.f);
}

int main() {
  rlimit limit{0,0};assert(setrlimit(RLIMIT_CORE,&limit)==0);
  assert(!config{}.reclaimable_victim_fallback);
  for(bool finite:{false,true}) for(bool on:{false,true}) opportunity(on,finite);
  for(bool on:{false,true}) {no_candidate(on);free_pool(on);original_safe(on);invalid_original(on);}
  retry_and_quota();pending_K1_return_and_WB();
}
