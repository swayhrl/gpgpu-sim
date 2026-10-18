#include <cassert>
#include <map>
#include "../../src/gpgpu-sim/gpu-cache.h"
int main() {
  assert(inst_t::ep_l2_plain_ldg({"LDG","E","64"}));
  for(auto modifier:{"STRONG","ACQUIRE","RELEASE","VOLATILE","SYS","GPU","CV","UNKNOWN"})
    assert(!inst_t::ep_l2_plain_ldg({"LDG","E",modifier}));
  assert(!inst_t::ep_l2_plain_ldg({"ATOM"}) && !inst_t::ep_l2_plain_ldg({"LDGSTS"}));
  using P=ep_l2_payload_store;
  for(unsigned mode:{1u,2u})for(bool s64:{false,true}) {
    if(mode==2 && s64)continue;
    P p(2,mode,16,false,s64,1);p.set_rg1_enabled(true);
    std::map<unsigned,bool> ready;
    p.bind_ready_source([&](unsigned,unsigned uid){return ready.at(uid);});
    auto a=p.reserve(0,P::RESIDENT,0,P::RESIDENT_VALID);
    auto b=p.reserve(4,P::RESIDENT,128,P::RESIDENT_VALID);
    auto c=p.reserve(8,P::RESIDENT,256,P::RESIDENT_VALID);
    auto d=p.reserve(12,P::RESIDENT,384,P::RESIDENT_VALID);
    auto req=[&](P::payload_handle h,bool w,unsigned long long cy,unsigned client,unsigned uid){
      return p.request(h,w,cy,~0ull,w?P::FILL_WRITE:P::RESIDENT_HIT_READ,client,uid);};
    ready[1]=ready[10]=ready[11]=ready[12]=true;
    assert(req(a,true,100,P::FRONTEND,1)==P::GRANTED);
    assert(req(b,false,100,P::RESPONSE,10)==P::BANK_TRUE_CONTENTION);
    assert(req(c,true,100,P::FILL,11)==P::BANK_TRUE_CONTENTION);
    ready[10]=false;p.observe_ready_cycle(101);
    assert(req(c,true,101,P::FILL,11)==P::GRANTED);
    assert(p.rg1_recovered(0)==1 && p.pending_operations()==1);
    // Same-cycle readiness change cannot refund a consumed P1 service.
    ready[10]=true;
    assert(req(b,false,101,P::RESPONSE,10)==P::BANK_TRUE_CONTENTION);
    // A newly called younger client cannot steal the resumed old owner's age.
    assert(req(d,true,102,P::FILL,12)==P::BANK_TRUE_CONTENTION);
    assert(req(b,false,102,P::RESPONSE,10)==P::GRANTED);
    assert(req(d,true,102,P::FILL,12)==P::BANK_TRUE_CONTENTION);
    assert(req(d,true,103,P::FILL,12)==P::GRANTED);
    assert(!p.pending_operations());
    // Same slot/read but different logical response identities never merge.
    ready[20]=ready[21]=true;
    assert(req(a,true,104,P::FRONTEND,1)==P::GRANTED);
    assert(req(b,false,104,P::RESPONSE,20)==P::BANK_TRUE_CONTENTION);
    assert(req(b,false,104,P::FRONTEND,21)==P::BANK_TRUE_CONTENTION);
    assert(p.pending_operations()==2);
    assert(req(b,false,105,P::FRONTEND,21)==P::BANK_TRUE_CONTENTION);
    assert(req(b,false,105,P::RESPONSE,20)==P::GRANTED);
    assert(req(b,false,106,P::FRONTEND,21)==P::GRANTED);
    // Explicit cancellation retires only one transaction, not another actor.
    assert(req(a,true,107,P::FRONTEND,1)==P::GRANTED);
    assert(req(b,false,107,P::RESPONSE,20)==P::BANK_TRUE_CONTENTION);
    assert(req(c,true,107,P::FILL,11)==P::BANK_TRUE_CONTENTION);
    p.cancel_transaction(20);assert(p.pending_operations()==1);
    assert(req(c,true,108,P::FILL,11)==P::GRANTED);
    // Generation reuse cancels old ownership before a new handle can retry.
    assert(req(a,true,109,P::FRONTEND,1)==P::GRANTED);
    ready[20]=false;assert(req(b,false,109,P::RESPONSE,20)==P::BANK_TRUE_CONTENTION);
    p.release(b);auto fresh=p.reserve(4,P::RESIDENT,512,P::RESIDENT_VALID);
    assert(fresh.generation!=b.generation && !p.pending_operations());
    ready[20]=true;assert(req(fresh,false,110,P::RESPONSE,20)==P::GRANTED);
    assert(p.bank_peak(0)==1 && !p.pending_operations());
    // A ready actor later in native C++ order retains ownership/age even if
    // not called for several cycles; other-bank traffic cannot erase it.
    auto e=p.reserve(1,P::RESIDENT,640,P::RESIDENT_VALID);
    assert(req(a,true,200,P::FRONTEND,1)==P::GRANTED);
    assert(req(fresh,false,200,P::RESPONSE,20)==P::BANK_TRUE_CONTENTION);
    for(unsigned cy=201;cy<205;++cy){p.observe_ready_cycle(cy);assert(req(e,true,cy,P::FILL,11)==P::GRANTED);}
    assert(req(d,true,205,P::FRONTEND,12)==P::BANK_TRUE_CONTENTION);
    assert(req(fresh,false,205,P::RESPONSE,20)==P::GRANTED);
    assert(req(d,true,206,P::FRONTEND,12)==P::GRANTED);
    assert(!p.pending_operations());
    p.rg1_print(stdout,0);
    printf("RG1 bounded arbiter mode%u s64%u ready-change/late-retry/age/identity/cancel/generation/P1 PASS\n",mode,s64);
  }
}
