#ifndef EP_L2_ALL_DATA_REFERENCE_H
#define EP_L2_ALL_DATA_REFERENCE_H
#include <cstdio>
#include <cstdint>
#include <map>
#include <vector>
#include <cassert>
#include "ep-l2-ideal-reference.h"

// Operational reference, outside all finite cache/NoC/memory resources.
// A transaction is eligible on real issue except for older overlapping stores.
// Snapshots are captured at eligibility; readiness is exactly one physical tick
// later, independently of SM register-port consumption or other responses.
namespace ep_l2_all_data {
class Service {
 public:
  using Mask=ep_l2_ideal::ReferenceMemory::Mask;
  using Data=ep_l2_ideal::ReferenceMemory::Data;
  using Snapshot=std::vector<ep_l2_ideal::Byte>;
  struct Instruction {
    uint64_t original,expected,issued=0,consumed=0,committed=0;
    bool store;
    unsigned space;
  };
  struct Request {
    uint64_t id,instruction,domain,line,issued,eligible=0,ready=0,original_access=0;
    bool store,eligible_valid=false,ready_valid=false,known=false;
    Mask mask;
    Data data{};
    Snapshot snapshot;
  };
  uint64_t begin(uint64_t original,unsigned expected,bool store,unsigned space) {
    const auto id=++instruction_sequence_;
    instructions_.emplace(id,Instruction{original,expected,0,0,0,store,space});
    ++instruction_count; ++space_instructions[space];
    if (store) ++store_instructions;
    return id;
  }
  uint64_t issue(uint64_t instruction,uint64_t domain,uint64_t address,
                 const Mask &mask,uint64_t now,const Data *known=nullptr,uint64_t original_access=0) {
    advance(now);
    auto &inst=instructions_.at(instruction);
    assert(inst.issued<inst.expected && (address&127)==0);
    bool any=false;for (bool bit:mask) any|=bit;assert(any);
    Request req{};req.id=++request_sequence_;req.instruction=instruction;
    req.domain=domain;req.line=address;req.issued=now;req.store=inst.store;req.mask=mask;
    req.original_access=original_access;
    if (known) {assert(req.store);req.known=true;req.data=*known;}
    auto pos=requests_.emplace(req.id,req).first;
    ++inst.issued;++requests_issued;
    if (req.store) ++store_fragments;
    make_eligible(pos->second,now);
    return req.id;
  }
  void advance(uint64_t now) {
    assert(!clock_valid_ || now>=clock_);
    if (clock_valid_ && now==clock_) return;
    clock_valid_=true;clock_=now;
    for (auto &item:requests_) {
      auto &r=item.second;
      if (r.eligible_valid && !r.ready_valid && r.ready<=now) {
        assert(r.ready==now && "reference service missed a physical tick");
        r.ready_valid=true;++requests_ready;
        assert(r.ready==r.eligible+1);
        if (r.store) {
          memory_[r.domain].write(r.line,r.mask,r.known?&r.data:nullptr);
          ++store_fragment_commits;
          auto &i=instructions_.at(r.instruction);++i.committed;
          assert(i.committed<=i.expected);
          if (i.committed==i.expected) ++store_reference_commits;
        }
      }
      if (!r.eligible_valid) make_eligible(r,now);
    }
  }
  bool ready(uint64_t id) const {return requests_.at(id).ready_valid;}
  const Request &inspect(uint64_t id) const {return requests_.at(id);}
  Snapshot consume(uint64_t id,uint64_t now) {
    auto it=requests_.find(id);assert(it!=requests_.end());auto &r=it->second;
    assert(r.ready_valid && now>=r.ready);
    Snapshot value=r.snapshot; // Value snapshot, never a mutable memory alias.
    consume_delay_cycles+=now-r.ready;++requests_consumed;
    auto &inst=instructions_.at(r.instruction);++inst.consumed;
    assert(inst.consumed<=inst.expected);
    requests_.erase(it);return value;
  }
  void finish(uint64_t id) {
    auto &i=instructions_.at(id);
    assert(i.issued==i.expected && i.consumed==i.expected);
    assert(!i.store || i.committed==i.expected);
    if (i.store && i.expected==0) ++store_reference_commits;
    ++instructions_consumed;instructions_.erase(id);
  }
  uint64_t new_shared_epoch(unsigned sm,unsigned cta) {
    auto key=std::make_pair(sm,cta);auto old=shared_.find(key);
    if (old!=shared_.end()) {
      for (const auto &x:requests_) assert(x.second.domain!=old->second);
      memory_.erase(old->second);
    }
    return shared_[key]=++domain_sequence_;
  }
  uint64_t shared_domain(unsigned sm,unsigned cta) const {return shared_.at({sm,cta});}
  bool host_prefill(uint64_t address,uint64_t count) {
    if (!drained()) return false;
    memory_[0].host_prefill_range(address,count);return true;
  }
  void seed(uint64_t domain,uint64_t address,const std::vector<uint8_t> &data) {
    assert(drained());memory_[domain].seed(address,data);
  }
  bool drained() const {return requests_.empty() && instructions_.empty();}
  void print(FILE *fp) const {
    fprintf(fp,"EPL2MEMREFV1|scope=cumulative|mode=I_MEM_ALL_DATA|instruction_path=NATIVE|"
      "data_hierarchy=NOT_APPLICABLE|instructions=%llu|instructions_consumed=%llu|"
      "stores=%llu|store_reference_commits=%llu|requests=%llu|ready=%llu|consumed=%llu|"
      "store_fragments=%llu|store_fragment_commits=%llu|dependency_cycles=%llu|"
      "ready_to_consume_cycles=%llu|sm_register_wait_attempts=%llu|"
      "memory_resource_wait=0|ready_latency_violations=0|pending_requests=%zu|pending_instructions=%zu|drained=%u\n",
      (unsigned long long)instruction_count,(unsigned long long)instructions_consumed,
      (unsigned long long)store_instructions,(unsigned long long)store_reference_commits,
      (unsigned long long)requests_issued,(unsigned long long)requests_ready,(unsigned long long)requests_consumed,
      (unsigned long long)store_fragments,(unsigned long long)store_fragment_commits,
      (unsigned long long)dependency_cycles,(unsigned long long)consume_delay_cycles,
      (unsigned long long)sm_register_wait_attempts,requests_.size(),instructions_.size(),drained());
    for (const auto &s:space_instructions)
      fprintf(fp,"EPL2MEMREFV1|SPACE|space=%u|instructions=%llu\n",s.first,(unsigned long long)s.second);
  }
  uint64_t instruction_count=0,instructions_consumed=0,store_instructions=0,store_reference_commits=0;
  uint64_t requests_issued=0,requests_ready=0,requests_consumed=0,store_fragments=0,store_fragment_commits=0;
  uint64_t dependency_cycles=0,consume_delay_cycles=0,sm_register_wait_attempts=0;
  std::map<unsigned,uint64_t> space_instructions;
 private:
  bool older_producer(const Request &r) const {
    for (const auto &x:requests_) {
      const auto &p=x.second;if (p.id>=r.id) break;
      if (!p.store || p.ready_valid || p.domain!=r.domain || p.line!=r.line) continue;
      for (unsigned b=0;b<128;++b) if (p.mask[b] && r.mask[b]) return true;
    }
    return false;
  }
  void make_eligible(Request &r,uint64_t now) {
    if (older_producer(r)) return;
    r.eligible_valid=true;r.eligible=now;r.ready=now+1;
    dependency_cycles+=now-r.issued;
    if (!r.store) r.snapshot=memory_[r.domain].read(r.line,r.mask);
  }
  bool clock_valid_=false;uint64_t clock_=0,instruction_sequence_=0,request_sequence_=0,domain_sequence_=0;
  std::map<uint64_t,Instruction> instructions_;
  std::map<uint64_t,Request> requests_;
  std::map<uint64_t,ep_l2_ideal::ReferenceMemory> memory_;
  std::map<std::pair<unsigned,unsigned>,uint64_t> shared_;
};
}
#endif
