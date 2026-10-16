#include "ideal_test_fixture.h"

static void check_known(const std::vector<ep_l2_ideal::Byte> &v,
                        uint64_t address, unsigned value, unsigned count=32) {
  assert(v.size() == count);
  for (unsigned i = 0; i < count; ++i)
    assert(v[i].address == address+i && v[i].value_known && v[i].value == value);
}

int main(int argc, char **argv) {
  assert(argc == 3);
  {
    Fixture f(argv[1], argv[2]);
    f.sp->ep_l2_ideal_seed(0x1000, std::vector<uint8_t>(128, 17));
    auto r = f.make(0x1020); f.push(r);
    for (unsigned i = 0; i < 5; ++i) { f.cycle(); assert(f.sp->ep_l2_ideal_reads_served()==0); }
    auto value = f.read_response(r); check_known(value,0x1020,17);
    assert(f.gpu->gpu_sim_cycle >= 6); // same nonzero ROP delay as normal hit
    assert(f.sp->ep_l2_lower_read_issue_count() == 0);
    // Cold data is a declared symbol, not a fabricated numeric zero.
    r = f.make(0x2000); f.push(r); value = f.read_response(r);
    for (auto b : value) assert(!b.value_known && b.version == 0);
    f.drain();
  }
  {
    Fixture f(argv[1],argv[2]);
    auto a=f.make(0x3000), b=f.make(0x4000); f.push(a); f.push(b);
    f.until_reply();
    for(unsigned i=0;i<20;++i)f.cycle();
    assert(f.sp->ep_l2_ideal_reads_served()==1);
    assert(f.sp->ep_l2_icntl2_occupancy()==1);
    f.read_response(a); f.read_response(b); f.drain();
  }
  {
    Fixture f(argv[1],argv[2]);
    f.sp->ep_l2_ideal_seed(0x5000,std::vector<uint8_t>(128,3));
    auto w=f.make(0x5000,true,16); auto r=f.make(0x5000);
    ep_l2_ideal::ReferenceMemory::Data data{}; data.fill(99);
    f.sp->ep_l2_ideal_known_write(w,data); f.push(w); f.push(r);
    f.until_reply(); assert(f.sp->top()==w);
    for(unsigned i=0;i<20;++i)f.cycle();
    // An older producer waiting for its real completion cannot be erased.
    assert(f.sp->ep_l2_ideal_reads_served()==0);
    assert(f.sp->ep_l2_ideal_dependency_waits()>0);
    assert(f.sp->pop()==w); delete w;
    auto value=f.read_response(r); assert(value.size()==32);
    for(unsigned i=0;i<32;++i) {
      assert(value[i].value_known && value[i].value==(i<16?99:3));
      assert((value[i].version>0)==(i<16));
    }
    // A younger write cannot retroactively change an already served read.
    auto before=f.make(0x5000), younger=f.make(0x5000,true);
    f.push(before); f.push(younger); f.until_reply();
    auto snapshot=f.sp->ep_l2_ideal_response(before);
    for(unsigned i=0;i<10;++i)f.cycle();
    assert(snapshot==f.sp->ep_l2_ideal_response(before));
    assert(f.sp->pop()==before); delete before;
    f.until_reply(); assert(f.sp->pop()==younger); delete younger;
    r=f.make(0x5000);f.push(r);value=f.read_response(r);
    for(auto byte:value) assert(!byte.value_known && byte.version>0);
    f.drain();
  }
  {
    // The exact production arbiter enforces a real same-bank fill/read
    // conflict even though the ideal read has no resident payload owner.
    ep_l2_payload_store p(2);
    assert(p.request(ep_l2_payload_store::RESIDENT,0,true,1)==ep_l2_payload_store::GRANTED);
    assert(p.request(ep_l2_payload_store::IDEAL_REFERENCE,4,false,1)==ep_l2_payload_store::BANK_TRUE_CONTENTION);
    assert(p.request(ep_l2_payload_store::IDEAL_REFERENCE,4,false,2)==ep_l2_payload_store::GRANTED);
    assert(p.bank_true_conflict_ops()==1 && p.pending_operations()==0);
  }
  std::puts("Lane4 I_READ_HIT production latency/backpressure/byte-version/RAW/WAR/bank tests: PASS");
}
