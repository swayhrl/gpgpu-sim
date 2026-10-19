// Compile only against A's r2-adapted public header. Source candidate, NOT_RUN.
// --legacy must expose the old same-handle alias (exit21); --extended must drain.
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../../src/gpgpu-sim/gpu-cache.h"

int main(int argc,char **argv) {
  assert(argc==2);
  using P=ep_l2_payload_store;
  const bool extended=!std::strcmp(argv[1],"--extended");
  assert(extended || !std::strcmp(argv[1],"--legacy"));
  P ram(2,2,16);ram.set_tvd_identity(extended);
  auto wb=ram.reserve(0,P::WB,0x1000,P::WB_STAGED);
  auto other=ram.reserve(4,P::RESIDENT,0x2000,P::RESIDENT_VALID);
  assert(ram.request(other,false,1)==P::GRANTED);
  assert(ram.request(wb,false,1,~0ull,P::WB_READOUT,P::LOWER_WB,101)==P::BANK_TRUE_CONTENTION);
  assert(ram.request(wb,false,1,~0ull,P::TVD_READ,P::FRONTEND,202)==P::BANK_TRUE_CONTENTION);
  if(ram.pending_operations()!=2) {
    printf("OLD_IDENTITY_ALIAS|pending=%u|expected=2|same_handle_clients_merged=1\n",ram.pending_operations());
    return 21;
  }
  // A younger requester cannot consume the older selected grant, even when
  // it calls first in C++. No ready-oracle or extra bank operation is granted.
  assert(ram.request(wb,false,2,~0ull,P::TVD_READ,P::FRONTEND,202)==P::BANK_TRUE_CONTENTION);
  assert(ram.request(wb,false,2,~0ull,P::WB_READOUT,P::LOWER_WB,101)==P::GRANTED);
  assert(ram.pending_operations()==1);
  assert(ram.request(wb,false,3,~0ull,P::TVD_READ,P::FRONTEND,202)==P::GRANTED);
  assert(!ram.pending_operations() && ram.bank_logical_ops()==3 && ram.bank_grants()==3);
  assert(ram.bank_peak(0)==1);
  // Selected-but-unconsumed logical tokens are explicitly cancelled by UID;
  // cancellation consumes neither a bank operation nor response-buffer space.
  assert(ram.request(other,false,3)==P::BANK_TRUE_CONTENTION);
  assert(ram.pending_operations()==1);
  ram.cancel_request(other,false);assert(!ram.pending_operations());
  assert(ram.request(wb,false,3,~0ull,P::TVD_READ,P::FRONTEND,303)==P::BANK_TRUE_CONTENTION);
  ram.cancel_transaction(303);assert(!ram.pending_operations());
  assert(ram.request(other,false,4)==P::GRANTED);
  assert(ram.request(wb,false,4,~0ull,P::TVD_READ,P::FRONTEND,404)==P::BANK_TRUE_CONTENTION);
  // Advancing another bank selects404 without granting its real RAM service.
  auto bank1=ram.reserve(1,P::RESIDENT,0x3000,P::RESIDENT_VALID);
  assert(ram.request(bank1,false,5)==P::GRANTED);
  ram.cancel_transaction(404);assert(!ram.pending_operations());
  ram.release(bank1);
  ram.release(wb);ram.release(other);
  const auto reused=ram.reserve(wb.payload_id,P::WB,0x4000,P::WB_STAGED);
  assert(reused.generation!=wb.generation && !ram.handle_live(wb));
  assert(ram.request(reused,false,6,~0ull,P::TVD_READ,P::FRONTEND,202)==P::GRANTED);
  ram.release(reused);
  assert(ram.free_slots()==1168 && ram.ownership_consistent());
  puts("TVD_BANK_IDENTITY|status=PASS|two_logical_reads=1|P1=1|drain=1|oldest_priority=1");
}
