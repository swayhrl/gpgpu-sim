// Drive actual GPU memcpy -> partition -> Tag/payload and normal WB/response.
#include <cassert>
#include <csignal>
#include <cstdio>
#include <set>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/option_parser.h"

struct test_gpu : exec_gpgpu_sim {
  test_gpu(const gpgpu_sim_config &c, gpgpu_context *ctx) : exec_gpgpu_sim(c, ctx) {}
  memory_partition_unit *partition() { return m_memory_partition_unit[0]; }
};

struct host_fixture {
  gpgpu_context ctx;
  gpgpu_sim_config cfg;
  test_gpu *gpu;
  memory_partition_unit *part;
  memory_sub_partition *sp;
  partition_mf_allocator *alloc;
  unsigned bank_violations = 0, maximum_grants = 0;
  std::set<unsigned long long> observed_wb;
  std::array<unsigned long long,4> expected_wb{{0,0,0,0}};
  bool verify_wb = false;
  host_fixture(const char *base, const char *overlay, const char *mode, const char *c)
      : cfg(&ctx) {
    auto op = option_parser_create();
    ctx.ptx_reg_options(op); ctx.func_sim->ptx_opcocde_latency_options(op);
    icnt_reg_options(op); cfg.reg_options(op);
    option_parser_cfgfile(op, base); option_parser_cfgfile(op, overlay);
    const char *args[] = {"prefill-test", "-gpgpu_ep_l2_ft_mode", mode,
      "-gpgpu_ep_l2_wbuf_entries", c, "-gpgpu_perf_sim_memcpy", "1",
      "-gpgpu_l2_char_dram_issue_hold_after_issues", "0",
      "-gpgpu_l2_char_dram_issue_hold_cycles", "200"};
    option_parser_cmdline(op, 11, args); cfg.init(); option_parser_destroy(op);
    gpu = new test_gpu(cfg, &ctx); part = gpu->partition(); sp = part->get_sub_partition(0);
    alloc = new partition_mf_allocator(gpu->getMemoryConfig());
  }
  ~host_fixture() { delete alloc; delete gpu; }
  l2_cache *cache() { return sp->ft_cache_for_test(); }
  mem_fetch *make(new_addr_type address, bool write = false) {
    active_mask_t active; active.set(0);
    mem_access_byte_mask_t bytes;
    unsigned s = (address / 32) % 4;
    for (unsigned b = s * 32; b < (s + 1) * 32; ++b) bytes.set(b);
    mem_access_sector_mask_t sectors; sectors.set(s);
    return alloc->alloc(address, write ? GLOBAL_ACC_W : GLOBAL_ACC_R, active,
        bytes, sectors, 32, write, gpu->gpu_sim_cycle + gpu->gpu_tot_sim_cycle,
        0,0,0,nullptr,0);
  }
  l2_access_plan plan(new_addr_type address) {
    auto mf = make(address); l2_access_plan p;
    cache()->preview_access(address, mf, p); delete mf; return p;
  }
  ep_l2::PayloadHandle handle(new_addr_type address) {
    auto p = plan(address); assert(p.probe_status == HIT);
    return cache()->ep_l2_tag_payload_handle_for_test(p.cache_index);
  }
  unsigned long long grants() {
    unsigned long long n = 0;
    for (unsigned b=0;b<4;++b) n += sp->ft_bank_grants_for_test(b);
    return n;
  }
  void warm(new_addr_type address, size_t count) {
    auto before = grants(); gpu->perf_memcpy_to_gpu(address, count);
    assert(grants() == before); // host prefill consumes no timed GPU bank grant
  }
  bool try_warm(new_addr_type address, unsigned sector) {
    mem_access_sector_mask_t mask; mask.set(sector);
    return sp->ft_try_force_l2_tag_update(address,
        gpu->gpu_sim_cycle + gpu->gpu_tot_sim_cycle, mask);
  }
  void step() {
    unsigned long long before[4];
    for(unsigned b=0;b<4;++b) before[b] = sp->ft_bank_grants_for_test(b);
    part->cache_cycle(gpu->gpu_sim_cycle);
    if (!sp->L2_dram_queue_empty()) {
      auto mf = sp->L2_dram_queue_top();
      if (mf->get_access_type() == L2_WRBK_ACC && verify_wb) {
        assert(mf->has_ft_wb_data() && mf->get_ft_wb_data() == expected_wb);
        observed_wb.insert(mf->get_request_uid());
      }
    }
    part->simple_dram_model_cycle();
    for (unsigned b=0;b<4;++b) {
      auto delta = sp->ft_bank_grants_for_test(b) - before[b];
      maximum_grants = std::max(maximum_grants, unsigned(delta));
      if (delta > 1) ++bank_violations;
    }
    ++gpu->gpu_sim_cycle;
  }
  void until_reply() {
    for(unsigned n=0;n<100000 && !sp->ep_l2_l2icnt_occupancy();++n) step();
    assert(sp->ep_l2_l2icnt_occupancy());
  }
  void consume(mem_fetch *expected, bool version_zero = false) {
    until_reply(); assert(sp->top() == expected);
    if (version_zero) {
      assert(expected->has_ft_wb_data());
      assert(expected->get_ft_wb_data()[(expected->get_addr()/32)%4] == 0);
    }
    assert(sp->pop() == expected); delete expected;
  }
  void issue(mem_fetch *mf) {
    sp->push(mf, gpu->gpu_sim_cycle + gpu->gpu_tot_sim_cycle);
  }
  void drain() {
    for(unsigned n=0;n<100000 && part->busy();++n) {
      step(); assert(!sp->ep_l2_l2icnt_occupancy());
    }
    assert(!part->busy() && sp->l2_char_no_resource_leak());
    assert(!bank_violations && maximum_grants <= 1);
  }
};

static void full_address_sectors(const char *b,const char *o,const char *m,const char *c) {
  host_fixture f(b,o,m,c); const new_addr_type a = 0x100004000ULL;
  assert(f.sp->ft_memcpy_offset_for_test() == 0);
  f.warm(a,0); assert(f.sp->ft_memcpy_offset_for_test() == 0);
  f.warm(a+31,2); assert(f.sp->ft_memcpy_offset_for_test() == 2);
  auto h = f.handle(a); assert(h == f.handle(a+32));
  assert(f.plan(a & 0xffffffffULL).probe_status == MISS); // no 32-bit alias
  auto &pool = f.cache()->ep_l2_payload_for_test();
  assert(pool.inspect(h).owner == a && pool.inspect(h).ready_sector_mask.to_ulong() == 3);
  f.warm(a+32,32); assert(h == f.handle(a));
  f.gpu->gpu_tot_sim_cycle += 1000000; // a quiescent later epoch, same ownership
  f.warm(a+96,32); assert(h == f.handle(a));
  assert(pool.inspect(h).ready_sector_mask.to_ulong() == 11);
  unsigned before = f.sp->ft_memcpy_offset_for_test();
  mem_access_sector_mask_t empty, multi; multi.set(0); multi.set(1);
  assert(!f.sp->ft_try_force_l2_tag_update(a,0,empty));
  assert(!f.sp->ft_try_force_l2_tag_update(a,0,multi));
  assert(f.sp->ft_memcpy_offset_for_test() == before && f.handle(a) == h);
  for (unsigned s : {0u,1u,3u}) { auto r=f.make(a+s*32);f.issue(r);f.consume(r,true); }
  assert(f.sp->ep_l2_lower_read_issue_count() == 0); f.drain();
  puts("PREFILL_FULL_ADDRESS_SECTOR_REPEAT_EPOCH_NATIVE_HIT PASS");
}

static void clean_eviction(const char *b,const char *o,const char *m,const char *c) {
  host_fixture f(b,o,m,c); f.warm(0x1000,32); f.warm(0x1040,32);
  auto old = f.handle(0x1000); f.warm(0x2000,32);auto next=f.handle(0x2000);
  assert(old != next && !f.cache()->ep_l2_payload_for_test().handle_live(old));
  assert(f.cache()->ep_l2_payload_for_test().inspect(next).ready_sector_mask.to_ulong()==1);
  auto r=f.make(0x1000);f.issue(r);f.consume(r,true);f.drain();
  auto live=f.handle(0x1000);f.warm(0x1020,32);assert(f.handle(0x1000)==live);
  assert(f.sp->ep_l2_lower_read_issue_count()==1);
  puts("PREFILL_CLEAN_VICTIM_REAL_MISS_FILL_QUIESCENT_OWNER_REUSE PASS");
}

static void dirty_victim_and_wb(const char *b,const char *o,const char *m,const char *c) {
  host_fixture f(b,o,m,c);f.warm(0x1000,32);f.warm(0x1040,32);
  auto w=f.make(0x1000,true);unsigned long long version=w->get_request_uid()+1;
  f.issue(w);f.consume(w);f.drain();auto h=f.handle(0x1000);
  auto before=f.cache()->ep_l2_payload_for_test().inspect(h);
  unsigned offset=f.sp->ft_memcpy_offset_for_test();auto grants=f.grants();
  assert(!f.try_warm(0x2000,0) && !f.try_warm(0x1000,0));
  assert(f.handle(0x1000)==h && f.sp->ft_memcpy_offset_for_test()==offset && f.grants()==grants);
  assert(f.cache()->ep_l2_payload_for_test().inspect(h).versions==before.versions);
  // The actual void production entry rejects too, rather than silently
  // discarding dirty data. Child copy permits checking the parent's invariants.
  pid_t child=fork();assert(child>=0);
  if (!child) {mem_access_sector_mask_t s;s.set(0);f.sp->force_l2_tag_update(0x2000,0,s);_exit(3);}
  int status=0;assert(waitpid(child,&status,0)==child && WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
  f.verify_wb=true;f.expected_wb={{version,0,0,0}};
  auto r=f.make(0x2000);f.issue(r);
  for(unsigned n=0;n<100 && !f.sp->ep_l2_wad_occupancy();++n)f.step();
  assert(f.sp->ep_l2_wad_occupancy());offset=f.sp->ft_memcpy_offset_for_test();
  assert(!f.try_warm(0x3000,0) && f.sp->ft_memcpy_offset_for_test()==offset);
  f.consume(r,true);f.drain();assert(f.observed_wb.size()==1 && !f.sp->ep_l2_wad_occupancy());
  f.warm(0x3000,32);r=f.make(0x3000);f.issue(r);f.consume(r,true);f.drain();
  puts("PREFILL_DIRTY_VICTIM_REFUSAL_UNMODIFIED_REAL_WB_SET_DONE_RETRY PASS");
}

static void response_guard(const char *b,const char *o,const char *m,const char *c) {
  host_fixture f(b,o,m,c);f.warm(0x1000,32);
  auto r=f.make(0x2000);f.issue(r);unsigned offset=f.sp->ft_memcpy_offset_for_test();
  assert(!f.try_warm(0x3000,0) && f.sp->ft_memcpy_offset_for_test()==offset);
  f.until_reply();for(unsigned n=0;n<20;++n)f.step();
  assert(!f.try_warm(0x3000,0) && f.sp->ft_memcpy_offset_for_test()==offset);
  f.consume(r,true);f.drain();assert(f.try_warm(0x3000,0));f.drain();
  puts("PREFILL_FRONTEND_PENDING_AND_RESPONSE_BACKPRESSURE_GUARD PASS");
}

int main(int argc,char **argv) {
  assert(argc==5);setvbuf(stdout,nullptr,_IOLBF,0);
  rlimit no_core{0,0};assert(setrlimit(RLIMIT_CORE,&no_core)==0);
  full_address_sectors(argv[1],argv[2],argv[3],argv[4]);
  clean_eviction(argv[1],argv[2],argv[3],argv[4]);
  dirty_victim_and_wb(argv[1],argv[2],argv[3],argv[4]);
  response_guard(argv[1],argv[2],argv[3],argv[4]);
  puts("FT_HOST_PREFILL_PRODUCTION_SUITE PASS");
}
