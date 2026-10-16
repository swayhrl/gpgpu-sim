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
  fixture(const char *base, const char *overlay, unsigned mode, unsigned c, unsigned sets=1)
      : config(&context), gpu(NULL), memory(NULL), allocator(NULL), cache(NULL), time(1) {
    option_parser_t p = option_parser_create();
    context.ptx_reg_options(p); context.func_sim->ptx_opcocde_latency_options(p);
    icnt_reg_options(p); config.reg_options(p);
    option_parser_cfgfile(p,base); option_parser_cfgfile(p,overlay);
    std::string mode_s=std::to_string(mode), c_s=std::to_string(c);
    const char *args[]={"ft-test","-gpgpu_ep_l2_ft_mode",mode_s.c_str(),"-gpgpu_ep_l2_wbuf_entries",c_s.c_str()};
    option_parser_cmdline(p,5,args);
    std::string geometry="S:"+std::to_string(sets)+":128:1,L:B:m:L:L,A:128:1,256:0,32";
    const char *geoargs[]={"ft-test","-gpgpu_cache:dl2",geometry.c_str()};
    option_parser_cmdline(p,3,geoargs); config.init(); option_parser_destroy(p);
    gpu = new exec_gpgpu_sim(config,&context); memory = gpu->getMemoryConfig();
    cache_config &cc = const_cast<memory_config *>(memory)->m_L2_config;

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
    for(unsigned n=0;n<10000 && (staged() || !cache->miss_queue_empty() || !port.packets.empty());++n) {
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
    std::array<unsigned long long,4> expected={{(unsigned long long)w->get_request_uid()+1,0,0,0}};
    reference[i*128]=expected;
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
    ref[i*128]={{(unsigned long long)w->get_request_uid()+1,0,0,0}};delete w;
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
  mem_fetch *ordered_write=f.make(0x1000,true);
  assert(!f.access(ordered_write,4));delete ordered_write;
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
  while(f.cache->access_ready()) {
    if(f.cache->ft_response_ready(f.time)) { f.cache->peek_next_access();f.cache->commit_next_access();++replies; }
    f.cycle();
  }
  assert(replies==3 && pool.inspect(h).pins==0);
  assert(f.access(replacement));delete replacement;delete a;delete b;delete merged;
  assert(!f.cache->ft_reservation_active());
  puts("FT sector identity, merge, fill and response-backpressure pins PASS");
}

static void same_bank_pool_full(const char *base,const char *overlay) {
  fixture f(base,overlay,2,16,1024);
  std::map<new_addr_type,std::array<unsigned long long,4>> ref;
  for(unsigned i=0;i<1024;++i) {
    mem_fetch *w=f.make(i*128,true);assert(f.access(w));
    ref[i*128]={{(unsigned long long)w->get_request_uid()+1,0,0,0}};delete w;
  }
  // Fill all 36 spare bank-0 landings with real production dirty victims.
  const unsigned stride=1024*128;
  for(unsigned i=1;i<=36;++i) {
    mem_fetch *w=f.make(i*stride,true);assert(f.access(w));
    ref[i*stride]={{(unsigned long long)w->get_request_uid()+1,0,0,0}};delete w;
  }
  assert(f.staged()==36 && f.wad()==36);
  auto h=f.resident();auto before=f.cache->ep_l2_payload_for_test().inspect(h);
  mem_fetch *w=f.make(37*stride,true);
  assert(!f.access(w,8));assert(f.resident()==h);
  assert(f.cache->ep_l2_payload_for_test().inspect(h).versions==before.versions);
  assert(!f.cache->ft_reservation_active());
  f.drain_wb(ref); // send requires no new landing even when bank 0 was full
  assert(f.access(w));delete w;f.drain_wb(ref);
  puts("FT production same-bank pool-full, nonmutating refusal, WB progress PASS");
}

static void read_version_reference(const char *base,const char *overlay) {
  fixture f(base,overlay,2,16);
  std::map<new_addr_type,std::array<unsigned long long,4>> memory;
  std::array<unsigned long long,4> expected={{0,0,0,0}};
  for(unsigned sector=0;sector<2;++sector) {
    mem_fetch *w=f.make(0x1000+32*sector,true);assert(f.access(w));
    expected[sector]=(unsigned long long)w->get_request_uid()+1;delete w;
  }
  memory[0x1000]=expected;
  mem_fetch *w=f.make(0x2000,true);assert(f.access(w));
  memory[0x2000]={{(unsigned long long)w->get_request_uid()+1,0,0,0}};delete w;
  f.drain_wb(memory);
  mem_fetch *a=f.make(0x1000,false,GLOBAL_ACC_R), *b=f.make(0x1020,false,GLOBAL_ACC_R);
  assert(f.access(a));assert(f.access(b));
  // Independently retained lower-memory versions are attached by the memory
  // fixture. The production fill and response paths must recover them exactly.
  for(unsigned n=0;n<128 && (f.cache->mshr_entries_used() || !f.cache->miss_queue_empty());++n) {
    f.cycle();
    while(!f.port.packets.empty()) {
      mem_fetch *mf=f.port.packets.front();f.port.packets.pop_front();
      if(mf->get_access_type()==L2_WRBK_ACC) {
        assert(mf->get_ft_wb_data()==memory.at(mf->get_addr()));
        f.cache->ep_l2_wad_complete(mf->get_addr(),f.time);delete mf;
      } else {
        mf->set_ft_wb_data(memory.at(mf->get_addr() & ~127ull));
        while(f.cache->ep_l2_payload_fill_request(mf,f.time)!=ep_l2_payload_store::GRANTED) f.cycle();
        f.cache->fill(mf,f.time);f.cycle();
      }
    }
    while(f.cache->access_ready()) {
      if(!f.cache->ft_response_ready(f.time)) { f.cycle();continue; }
      mem_fetch *r=f.cache->peek_next_access();
      unsigned sector=(r->get_addr()>>5)&3;
      assert(r->get_ft_wb_data()[sector]==expected[sector]);
      f.cache->commit_next_access();f.cycle();
    }
  }
  assert(!f.cache->mshr_entries_used());
  mem_fetch *hit=f.make(0x1000,false,GLOBAL_ACC_R);assert(f.access(hit));
  assert(hit->get_ft_wb_data()==expected);delete hit;delete a;delete b;
  puts("FT independent store/WB/lower/fill/merged-response version reference PASS");
}

int main(int argc,char **argv) {
  setvbuf(stdout,NULL,_IOLBF,0);
  if(argc==5) { fixture invalid(argv[1],argv[2],std::stoul(argv[3]),std::stoul(argv[4]));
    fputs("INVALID CONFIG WAS ACCEPTED\n",stderr);return 1; }
  assert(argc==3);
  for(unsigned mode=1;mode<=2;++mode) for(unsigned c: {8u,16u}) capacity_and_versions(argv[1],argv[2],mode,c);
  wad_full(argv[1],argv[2]);response_pins_and_sectors(argv[1],argv[2]);
  same_bank_pool_full(argv[1],argv[2]);read_version_reference(argv[1],argv[2]);
  puts("FT production directed suite PASS");
}
