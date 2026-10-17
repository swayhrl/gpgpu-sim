#ifndef EP_L2_ALL_DATA_REFERENCE_H
#define EP_L2_ALL_DATA_REFERENCE_H
#include <cstdio>
#include <cstdint>
#include <map>
#include <set>
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
    uint64_t kernel=0,pc=0,stream=0;
    unsigned sm=0,warp=0,dynamic_warp=0;
  };
  struct Request {
    uint64_t id,instruction,domain,line,issued,eligible=0,ready=0,original_access=0;
    bool store,eligible_valid=false,ready_valid=false,known=false;
    Mask mask;
    Data data{};
    Snapshot snapshot;
    uint64_t snapshot_digest=0;
  };
  uint64_t begin(uint64_t original,unsigned expected,bool store,unsigned space) {
    assert(record_identity(original,seen_instructions_) && "duplicate original instruction issue");
    ++unique_original_instructions;
    const auto id=++instruction_sequence_;
    instructions_.emplace(id,Instruction{original,expected,0,0,0,store,space});
    issued_instruction_digest^=instruction_digest(instructions_.at(id));
    ++instruction_count; ++space_instructions[space];
    if (store) ++store_instructions;
    else if (space==0) ++barrier_instructions;
    else ++load_instructions;
    return id;
  }
  void annotate(uint64_t id,uint64_t kernel,uint64_t pc,uint64_t stream,
                unsigned sm,unsigned warp,unsigned dynamic_warp) {
    auto &i=instructions_.at(id);issued_instruction_digest^=instruction_digest(i);
    i.kernel=kernel;i.pc=pc;i.stream=stream;
    i.sm=sm;i.warp=warp;i.dynamic_warp=dynamic_warp;
    issued_instruction_digest^=instruction_digest(i);
  }
  void barrier_issue(unsigned sm,unsigned warp) {assert(barriers_.insert({sm,warp}).second);}
  bool barrier_pending(unsigned sm,unsigned warp) const {return barriers_.count({sm,warp});}
  void barrier_complete(unsigned sm,unsigned warp) {assert(barriers_.erase({sm,warp})==1);}
  void native_store_issue(uint64_t original,unsigned sm,unsigned warp) {
    assert(native_stores_.emplace(original,std::make_pair(sm,warp)).second);++native_stores_issued;
  }
  bool native_store_declared(uint64_t original) const {return native_stores_.count(original);}
  bool warp_stores_pending(unsigned sm,unsigned warp) const {
    for (const auto &s:native_stores_) if (s.second==std::make_pair(sm,warp)) return true;
    return false;
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
    if (original_access) {
      assert(record_identity(original_access,seen_accesses_) && "duplicate original native access issue");
      ++unique_native_accesses;
    }
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
        if (!r.store) {
          ready_snapshot_digest^=r.snapshot_digest^r.id;
          snapshot_bytes_ready+=r.snapshot.size();
          for (const auto &b:r.snapshot) {if (b.value_known)++known_bytes;else ++symbolic_bytes;}
        }
        if (r.store) {
          memory_[r.domain].write(r.line,r.mask,r.known?&r.data:nullptr);
          ++store_fragment_commits;
          auto &i=instructions_.at(r.instruction);++i.committed;
          assert(i.committed<=i.expected);
          if (i.committed==i.expected) {
            ++store_reference_commits;
            if (native_stores_.erase(i.original)) ++native_stores_committed;
          }
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
    if (!r.store) {
      assert(digest(value)==r.snapshot_digest);
      consumed_snapshot_digest^=r.snapshot_digest^r.id;
      snapshot_bytes_consumed+=value.size();
    }
    consume_delay_cycles+=now-r.ready;++requests_consumed;
    auto &inst=instructions_.at(r.instruction);++inst.consumed;
    assert(inst.consumed<=inst.expected);
    requests_.erase(it);return value;
  }
  void finish(uint64_t id) {
    auto &i=instructions_.at(id);
    assert(i.issued==i.expected && i.consumed==i.expected);
    assert(!i.store || i.committed==i.expected);
    if (i.store && i.expected==0) {
      ++store_reference_commits;
      if (native_stores_.erase(i.original)) ++native_stores_committed;
    }
    consumed_instruction_digest^=instruction_digest(i);
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
  bool drained() const {return requests_.empty() && instructions_.empty() && barriers_.empty() && native_stores_.empty();}
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
    fprintf(fp,"EPL2MEMREFV1|SNAPSHOTS|scope=cumulative|loads=%llu|barriers=%llu|bytes_ready=%llu|bytes_consumed=%llu|"
      "known_bytes=%llu|symbolic_bytes=%llu|ready_digest=%llu|consumed_digest=%llu|data_scope=BYTE_ADDRESS_PRODUCER_VERSION_SYMBOLS\n",
      (unsigned long long)load_instructions,(unsigned long long)barrier_instructions,
      (unsigned long long)snapshot_bytes_ready,(unsigned long long)snapshot_bytes_consumed,
      (unsigned long long)known_bytes,(unsigned long long)symbolic_bytes,
      (unsigned long long)ready_snapshot_digest,(unsigned long long)consumed_snapshot_digest);
    fprintf(fp,"EPL2MEMREFV1|IDENTITY|scope=cumulative|issued_instruction_digest=%llu|consumed_instruction_digest=%llu|pending_barriers=%zu|native_stores_issued=%llu|native_stores_committed=%llu|pending_native_stores=%zu\n",
      (unsigned long long)issued_instruction_digest,(unsigned long long)consumed_instruction_digest,barriers_.size(),
      (unsigned long long)native_stores_issued,(unsigned long long)native_stores_committed,native_stores_.size());
    fprintf(fp,"EPL2MEMREFV1|UNIQUENESS|original_instructions=%llu|native_accesses=%llu|instruction_uid_blocks=%zu|access_uid_blocks=%zu|duplicate_policy=FAIL_CLOSED\n",
      (unsigned long long)unique_original_instructions,(unsigned long long)unique_native_accesses,seen_instructions_.size(),seen_accesses_.size());
    for (const auto &s:space_instructions)
      fprintf(fp,"EPL2MEMREFV1|SPACE|space=%u|instructions=%llu\n",s.first,(unsigned long long)s.second);
  }
  uint64_t instruction_count=0,instructions_consumed=0,store_instructions=0,store_reference_commits=0;
  uint64_t requests_issued=0,requests_ready=0,requests_consumed=0,store_fragments=0,store_fragment_commits=0;
  uint64_t dependency_cycles=0,consume_delay_cycles=0,sm_register_wait_attempts=0;
  uint64_t load_instructions=0,barrier_instructions=0,snapshot_bytes_ready=0,snapshot_bytes_consumed=0;
  uint64_t known_bytes=0,symbolic_bytes=0,ready_snapshot_digest=0,consumed_snapshot_digest=0;
  uint64_t issued_instruction_digest=0,consumed_instruction_digest=0;
  uint64_t native_stores_issued=0,native_stores_committed=0;
  uint64_t unique_original_instructions=0,unique_native_accesses=0;
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
    if (!r.store) {r.snapshot=memory_[r.domain].read(r.line,r.mask);r.snapshot_digest=digest(r.snapshot);}
  }
  static uint64_t digest(const Snapshot &snapshot) {
    uint64_t d=1469598103934665603ULL;
    for (const auto &b:snapshot)
      for (uint64_t x:{b.address,b.version,uint64_t(b.value_known),uint64_t(b.value_known?b.value:0)})
        {d^=x;d*=1099511628211ULL;}
    return d;
  }
  static uint64_t instruction_digest(const Instruction &i) {
    uint64_t d=1469598103934665603ULL;
    for (uint64_t v:{i.original,i.expected,i.kernel,i.pc,i.stream,uint64_t(i.sm),uint64_t(i.warp),
                    uint64_t(i.dynamic_warp),uint64_t(i.store),uint64_t(i.space)}) {d^=v;d*=1099511628211ULL;}
    return d;
  }
  using IdBlocks=std::map<uint64_t,std::array<uint64_t,64>>;
  static bool record_identity(uint64_t uid,IdBlocks &seen) {
    auto &block=seen[uid/4096];auto &word=block[(uid%4096)/64];const uint64_t bit=uint64_t(1)<<(uid%64);
    if (word&bit) return false;
    word|=bit;return true;
  }
  bool clock_valid_=false;uint64_t clock_=0,instruction_sequence_=0,request_sequence_=0,domain_sequence_=0;
  std::map<uint64_t,Instruction> instructions_;
  std::map<uint64_t,Request> requests_;
  std::map<uint64_t,ep_l2_ideal::ReferenceMemory> memory_;
  std::map<std::pair<unsigned,unsigned>,uint64_t> shared_;
  std::set<std::pair<unsigned,unsigned>> barriers_;
  std::map<uint64_t,std::pair<unsigned,unsigned>> native_stores_;
  IdBlocks seen_instructions_,seen_accesses_;
};
}
#endif
