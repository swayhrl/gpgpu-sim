// Native allocator component. Actual cache-response evidence is a separate
// primary partition fixture. This source has NOT been compiled/executed.
#include <cassert>
#include <cstdio>
#include <vector>
#include "../../../src/gpgpu-sim/gpu-cache.h"
using P=ep_l2_payload_store;
using H=ep_l2::PayloadHandle;

static std::vector<H> fill_other(P &ram) {
  std::vector<H> v;
  for(unsigned i=0;i<1104;++i) {
    const auto role=i<1024?P::RESIDENT:P::WB;
    auto h=ram.preview_free(role);assert(h.valid());
    h=ram.reserve(h.payload_id,role,128ull*(i+1),i<1024?P::RESIDENT_VALID:P::WB_STAGED);
    v.push_back(h);
  }
  return v;
}

static void reciprocal_reservation_and_rollback() {
  P ram(2,2,16);ram.set_fixed_victim_entries(64);
  auto allocated=fill_other(ram);
  assert(ram.free_slots()==64 && ram.tvd_used()==0);
  // Critical distinction from a victim-count cap: even with ZERO victims,
  // every unused reserved slot is unavailable to every other data role.
  for(auto role:{P::RESIDENT,P::WB,P::BYPASS})assert(!ram.preview_free(role).valid());
  for(unsigned b=0;b<4;++b) {
    assert(ram.fixed_charged_other(b)==276 && ram.fixed_charged_victims(b)==0);
    assert(ram.preview_free(P::TVD,b).valid());
  }
  const auto old=allocated.front();const auto saved=ram.inspect(old);
  assert(ram.reserve_fixed_retention(old));
  auto next=ram.preview_free(P::RESIDENT,old.payload_id%4);assert(next.valid());
  next=ram.reserve(next.payload_id,P::RESIDENT,0x100000,P::RESIDENT_FILL_PENDING);
  assert(ram.fixed_retention_reserved() && ram.fixed_quota_consistent());
  // Legal resource rollback: unexposed landing released, original ownership
  // restored, retention credit returned, no extra data carrier/slot granted.
  ram.release(next);ram.restore(old.payload_id,saved);
  assert(!ram.fixed_retention_reserved() && ram.free_slots()==64 && ram.tvd_used()==0);
  assert(ram.fixed_quota_consistent() && !ram.preview_free(P::RESIDENT).valid());
  assert(ram.reserve_fixed_retention(old));
  next=ram.preview_free(P::RESIDENT,old.payload_id%4);
  next=ram.reserve(next.payload_id,P::RESIDENT,0x100000,P::RESIDENT_VALID);
  ram.transfer(old,P::TVD,P::TVD_VALID); // Atomic in-place role commit.
  assert(!ram.fixed_retention_reserved() && ram.tvd_used()==1 && ram.fixed_quota_consistent());
  // A cross-role mandatory transition stays in the non-victim bucket and
  // cannot falsely fail at a full quota.
  ram.transfer(next,P::WB,P::WB_STAGED);
  ram.transfer(next,P::RESIDENT,P::RESIDENT_VALID);
  assert(!ram.fixed_promotion_room(old,H()));
  assert(ram.fixed_promotion_room(old,next));
  ram.release(next);ram.transfer(old,P::RESIDENT,P::RESIDENT_VALID);
  assert(!ram.tvd_used() && ram.fixed_quota_consistent());
  for(auto h:allocated)ram.release(h);
  assert(ram.free_slots()==1168 && !ram.pending_operations());
  puts("FIXED_PARTITION|case=unused_quota_not_lent_credit_rollback_promotion|status=PASS");
}

static void per_bank_static_capacity_and_dynamic_control() {
  P fixed(2,2,16);fixed.set_fixed_victim_entries(64);
  auto other=fill_other(fixed);std::vector<H> victims;
  for(unsigned i=0;i<64;++i) {
    auto h=fixed.preview_free(P::TVD);assert(h.valid());
    victims.push_back(fixed.reserve(h.payload_id,P::TVD,0x200000ull+128*i,P::TVD_VALID));
  }
  assert(!fixed.preview_free(P::TVD).valid() && !fixed.free_slots());
  assert(!fixed.reserve_fixed_retention(other.front()));
  for(unsigned b=0;b<4;++b)assert(fixed.fixed_charged_victims(b)==16 && fixed.fixed_charged_other(b)==276);
  for(auto h:victims)fixed.release(h);
  for(auto h:other)fixed.release(h);
  P dynamic(2,2,16);std::vector<H> all;
  for(unsigned i=0;i<1168;++i) {
    auto h=dynamic.preview_free(P::WB);assert(h.valid());
    all.push_back(dynamic.reserve(h.payload_id,P::WB,128ull*i,P::WB_STAGED));
  }
  assert(!dynamic.free_slots());for(auto h:all)dynamic.release(h);
  puts("FIXED_PARTITION|case=64_1104_static_vs1168_dynamic_capacity|status=PASS");
}
int main() {reciprocal_reservation_and_rollback();per_bank_static_capacity_and_dynamic_control();}
