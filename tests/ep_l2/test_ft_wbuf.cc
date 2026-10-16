// Production l2_cache access/issue/fill/response tests against symbolic data versions.
#include <cassert>
#include <cstdio>
#include <deque>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/option_parser.h"

struct lower_port : mem_fetch_interface {
  bool blocked;
  std::deque<mem_fetch *> packets;
  lower_port() : blocked(true) {}
  bool full(unsigned, bool) const override { return blocked; }
  void push(mem_fetch *mf) override {
    if (mf->get_access_type() == L2_WRBK_ACC) assert(mf->has_ft_wb_data());
    packets.push_back(mf);
  }
};

struct fixture {
  gpgpu_context context;
  gpgpu_sim_config config;
  exec_gpgpu_sim *gpu;
  const memory_config *memory;
  partition_mf_allocator *allocator;
  l2_cache *cache;
  lower_port port;
  unsigned time;
  fixture(const char *base, const char *overlay, unsigned mode, unsigned c)
      : config(&context), gpu(NULL), memory(NULL), allocator(NULL), cache(NULL), time(1) {
    option_parser_t p = option_parser_create();
    context.ptx_reg_options(p); context.func_sim->ptx_opcocde_latency_options(p);
    icnt_reg_options(p); config.reg_options(p);
    option_parser_cfgfile(p,base); option_parser_cfgfile(p,overlay); config.init(); option_parser_destroy(p);
    gpu = new exec_gpgpu_sim(config,&context); memory = gpu->getMemoryConfig();
    cache_config &cc = const_cast<memory_config *>(memory)->m_L2_config;
    cc.m_ep_l2_ft_mode = mode; cc.m_ep_l2_wbuf_entries = c;
    allocator = new partition_mf_allocator(memory);
    cache = new l2_cache("FT_TEST",cc,0,0,&port,allocator,IN_PARTITION_L2_MISS_QUEUE,gpu,L2_GPU_CACHE);
  }
  ~fixture() { delete cache; delete allocator; delete gpu; }
  mem_fetch *make(new_addr_type a, bool wr, mem_access_type type=GLOBAL_ACC_W) {
    active_mask_t active; active.set(0);
    mem_access_byte_mask_t bytes;
    unsigned sector=(a >> 5)&3;
    for(unsigned b=sector*32;b<sector*32+32;++b) bytes.set(b);
    mem_access_sector_mask_t mask;mask.set(sector);
    return allocator->alloc(a,type,active,bytes,mask,32,wr,time,0,0,0,NULL,0);
  }
  void cycle() { gpu->gpu_sim_cycle=time++; cache->cycle(); }
  bool access(mem_fetch *mf, unsigned limit=32) {
    for(unsigned n=0;n<limit;++n) {
      std::list<cache_event> events;
      auto r=cache->access(mf->get_addr(),mf,time,events); cycle();
      if(r!=RESERVATION_FAIL) return true;
    }
    return false;
  }
  ep_l2::PayloadHandle resident() { return cache->ep_l2_tag_payload_handle_for_test(0); }
  unsigned staged() { return cache->ft_wb_staged(); }
  unsigned wad() { return cache->ep_l2_wad_occupancy(); }
  void drain_wb(std::map<new_addr_type,std::array<unsigned long long,4>> &reference) {
    port.blocked=false;
    for(unsigned n=0;n<10000 && (staged() || !cache->miss_queue_empty());++n) {
      cycle();
      while(!port.packets.empty()) {
        mem_fetch *mf=port.packets.front();port.packets.pop_front();
        assert(mf->get_access_type()==L2_WRBK_ACC);
        assert(mf->get_ft_wb_data()==reference.at(mf->get_addr()));
        // Actual lower acceptance has freed staging, while WAD remains live.
        assert(cache->ep_l2_wad_contains(mf->get_addr()));
        cache->ep_l2_wad_complete(mf->get_addr(),time);delete mf;
      }
    }
    assert(!staged() && !wad() && cache->miss_queue_empty());
    assert(cache->ep_l2_payload_for_test().wb_used()==0);
    assert(cache->ep_l2_payload_pending_operations()==0);
  }
};

static void capacity_and_versions(const char *base,const char *overlay,unsigned mode,unsigned c) {
  fixture f(base,overlay,mode,c);
  std::map<new_addr_type,std::array<unsigned long long,4>> reference;
  unsigned n = mode==1 ? c+1 : c+25;
  for(unsigned i=0;i<n;++i) {
    mem_fetch *w=f.make(i*128,true, i%2 ? L1_WRBK_ACC : GLOBAL_ACC_W);
    assert(f.access(w));
    auto h=f.resident(); auto &slot=f.cache->ep_l2_payload_for_test().inspect(h);
    assert(h.payload_id%4==0 && slot.versions[0]==(unsigned long long)w->get_request_uid()+1);
    reference[i*128]=slot.versions;
    delete w;
  }
  assert(f.staged()==n-1 && f.wad()==n-1);
  if(mode==1) {
    auto h=f.resident();auto before=f.cache->ep_l2_payload_for_test().inspect(h);
    mem_fetch *blocked=f.make(n*128,true);
    assert(!f.access(blocked,16));
    assert(f.resident()==h && f.cache->ep_l2_payload_for_test().inspect(h).versions==before.versions);
    assert(!f.cache->ft_reservation_active());delete blocked;
  } else assert(f.staged()>c); // genuine borrowing, no C-entry control ceiling
  auto &pool=f.cache->ep_l2_payload_for_test();
  assert(pool.free_slots()+pool.resident_used()+pool.bypass_used()+pool.wb_used()==1152+c);
  f.drain_wb(reference);
  f.cache->ft_print(stdout,0);
  printf("FT production capacity/version/drain mode=%u C=%u PASS\n",mode,c);
}

static void wad_full(const char *base,const char *overlay) {
  fixture f(base,overlay,2,16);
  std::map<new_addr_type,std::array<unsigned long long,4>> ref;
  for(unsigned i=0;i<129;++i) {
    mem_fetch *w=f.make(i*128,true);assert(f.access(w));
    ref[i*128]=f.cache->ep_l2_payload_for_test().inspect(f.resident()).versions;delete w;
  }
  assert(f.wad()==128 && f.staged()==128);
  auto h=f.resident();mem_fetch *blocked=f.make(129*128,true);
  assert(!f.access(blocked));assert(f.resident()==h && !f.cache->ft_reservation_active());delete blocked;
  f.drain_wb(ref);puts("FT WAD128 pre-mutation refusal and drain PASS");
}

static void response_pins_and_sectors(const char *base,const char *overlay) {
  fixture f(base,overlay,2,16); f.port.blocked=false;
  mem_fetch *a=f.make(0x1000,false,GLOBAL_ACC_R);assert(f.access(a));
  mem_fetch *merged=f.make(0x1000,false,GLOBAL_ACC_R);assert(f.access(merged));
  mem_fetch *b=f.make(0x1020,false,GLOBAL_ACC_R);assert(f.access(b));
  auto h=f.resident();auto &pool=f.cache->ep_l2_payload_for_test();
  assert(pool.inspect(h).pins==3 && pool.inspect(h).pending_sector_mask.count()==2);
  for(unsigned n=0;n<16 && f.port.packets.size()<2;++n) f.cycle();
  assert(f.port.packets.size()==2);
  while(!f.port.packets.empty()) {
    mem_fetch *mf=f.port.packets.front();f.port.packets.pop_front();
    for(unsigned n=0;n<16;++n) {
      auto result=f.cache->ep_l2_payload_fill_request(mf,f.time);f.cycle();
      if(result==ep_l2_payload_store::GRANTED) { f.cache->fill(mf,f.time);break; }
      assert(n<15);
    }
  }
  assert(pool.inspect(h).pending_sector_mask.none() && pool.inspect(h).pins==3);
  mem_fetch *replacement=f.make(0x2000,true);assert(!f.access(replacement,8));
  assert(f.resident()==h);
  unsigned replies=0;
  while(f.cache->access_ready()) { f.cache->commit_next_access();++replies; }
  assert(replies==3 && pool.inspect(h).pins==0);
  assert(f.access(replacement));delete replacement;delete a;delete b;delete merged;
  assert(!f.cache->ft_reservation_active());
  puts("FT sector identity, merge, fill and response-backpressure pins PASS");
}

int main(int argc,char **argv) {
  assert(argc==3);
  for(unsigned mode=1;mode<=2;++mode) for(unsigned c: {8u,16u}) capacity_and_versions(argv[1],argv[2],mode,c);
  wad_full(argv[1],argv[2]);response_pins_and_sectors(argv[1],argv[2]);
  puts("FT production directed suite PASS");
}
