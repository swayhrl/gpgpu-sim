// Production regression source; NOT_RUN until exact R1.1 libraries/admission.
#include "ideal_test_fixture.h"

static mem_access_sector_mask_t sector(unsigned s) {
  mem_access_sector_mask_t m; m.set(s); return m;
}

static bool warm(Fixture &f, uint64_t address, unsigned s) {
  unsigned long long grants[4];
  for (unsigned b=0;b<4;++b) grants[b]=f.sp->ft_bank_grants_for_test(b);
  const unsigned offset=f.sp->ft_memcpy_offset_for_test();
  bool accepted=f.sp->ft_try_force_l2_tag_update(address,
      f.gpu->gpu_sim_cycle+f.gpu->gpu_tot_sim_cycle,sector(s));
  for (unsigned b=0;b<4;++b) assert(grants[b]==f.sp->ft_bank_grants_for_test(b));
  assert(f.sp->ft_memcpy_offset_for_test()==offset+unsigned(accepted));
  return accepted;
}

static void unknown(const std::vector<ep_l2_ideal::Byte> &value) {
  assert(value.size()==32);
  for (auto b:value) assert(b.version==0 && !b.value_known);
}

static std::vector<ep_l2_ideal::Byte> read(Fixture &f,uint64_t address) {
  auto r=f.make(address);f.push(r);auto value=f.read_response(r);f.drain();return value;
}

int main(int argc,char **argv) {
  assert(argc==3);Fixture f(argv[1],argv[2]);
  const uint64_t a=0x100004000ULL, conflict=a+0x1000;
  f.sp->ep_l2_ideal_seed(a+64,std::vector<uint8_t>(32,17));
  assert(warm(f,a,0));assert(warm(f,a+32,1));
  unknown(read(f,a));unknown(read(f,a+32));
  auto w=f.make(a,true);ep_l2_ideal::ReferenceMemory::Data data{};data.fill(91);
  f.sp->ep_l2_ideal_known_write(w,data);f.push(w);
  f.until_reply();assert(f.sp->top()==w);assert(f.sp->pop()==w);delete w;f.drain();
  auto produced=read(f,a);
  for (auto b:produced) assert(b.version>0 && b.value_known && b.value==91);
  // Dirty refusal is before reference mutation; old GPU producer remains visible.
  assert(!warm(f,a,0));assert(read(f,a)==produced);
  // An ordinary allocation read bypasses only the ideal read-service entry.
  // It really evicts dirty A, performs WB/set_done and fills a clean victim.
  active_mask_t active;active.set(0);mem_access_byte_mask_t bytes;
  for (unsigned b=0;b<32;++b) bytes.set(b);
  auto native=f.alloc->alloc(conflict,L1_WR_ALLOC_R,active,bytes,sector(0),32,false,
      f.gpu->gpu_sim_cycle,0,0,0,nullptr,0);
  f.push(native);f.until_reply();assert(f.sp->top()==native);
  assert(f.sp->pop()==native);delete native;f.drain();
  assert(f.sp->ft_cache_for_test()->ft_copy_lines()==1);
  assert(f.sp->ft_cache_for_test()->ft_lower_accepts()==1);
  // Later clean/quiescent prefill is allowed, even after a completed producer.
  assert(warm(f,a+32,1));assert(read(f,a)==produced);
  assert(warm(f,a,0));unknown(read(f,a));unknown(read(f,a+32));
  assert(warm(f,a,0));unknown(read(f,a));
  // A held real response blocks prefill and preserves its snapshot.
  auto held=f.make(a);f.push(held);f.until_reply();
  const auto snapshot=f.sp->ep_l2_ideal_response(held);
  assert(!warm(f,a,0));assert(snapshot==f.sp->ep_l2_ideal_response(held));
  assert(f.sp->pop()==held);delete held;f.drain();assert(warm(f,a,0));
  // Unselected byte/sector state is not globally reset by host initialization.
  auto untouched=read(f,a+64);
  for (auto b:untouched) assert(b.version==0 && b.value_known && b.value==17);
  f.drain();std::puts("IDEAL_HOST_PREFILL_REAL_GPU_WRITE_WB_REINIT_REFUSAL_RESPONSE PASS");
}
