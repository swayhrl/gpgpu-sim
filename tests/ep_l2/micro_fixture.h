#pragma once
// Production l2_cache access/issue/fill/response tests against symbolic data versions.
#include <sys/wait.h>
#include <unistd.h>
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

struct micro_fixture {
  gpgpu_context context;
  gpgpu_sim_config config;
  exec_gpgpu_sim *gpu;
  const memory_config *memory;
  partition_mf_allocator *allocator;
  l2_cache *cache;
  lower_port port;
  unsigned time;
  micro_fixture(const char *base, const char *overlay, unsigned mode, unsigned c, unsigned sets=1,unsigned ports=1)
      : config(&context), gpu(NULL), memory(NULL), allocator(NULL), cache(NULL), time(1) {
    option_parser_t p = option_parser_create();
    context.ptx_reg_options(p); context.func_sim->ptx_opcocde_latency_options(p);
    icnt_reg_options(p); config.reg_options(p);
    option_parser_cfgfile(p,base); option_parser_cfgfile(p,overlay);
    std::string mode_s=std::to_string(mode), c_s=std::to_string(c), ports_s=std::to_string(ports);
    const char *args[]={"ft-test","-gpgpu_ep_l2_ft_mode",mode_s.c_str(),"-gpgpu_ep_l2_wbuf_entries",c_s.c_str(),"-gpgpu_ep_l2_bank_ops",ports_s.c_str()};
    option_parser_cmdline(p,7,args);
    std::string geometry="S:"+std::to_string(sets)+":128:16,L:B:m:L:L,A:128:1,256:0,32";
    const char *geoargs[]={"ft-test","-gpgpu_cache:dl2",geometry.c_str()};
    option_parser_cmdline(p,3,geoargs); config.init(); option_parser_destroy(p);
    gpu = new exec_gpgpu_sim(config,&context); memory = gpu->getMemoryConfig();
    gpu->gpu_sim_cycle = time;
    cache_config &cc = const_cast<memory_config *>(memory)->m_L2_config;

    allocator = new partition_mf_allocator(memory);
    cache = new l2_cache("FT_TEST",cc,0,0,&port,allocator,IN_PARTITION_L2_MISS_QUEUE,gpu,L2_GPU_CACHE);
  }
  ~micro_fixture() { delete cache; delete allocator; delete gpu; }
  mem_fetch *make(new_addr_type a, bool wr, mem_access_type type=GLOBAL_ACC_W, bool ordinary=false) {
    active_mask_t active; active.set(0);
    mem_access_byte_mask_t bytes;
    unsigned sector=(a >> 5)&3;
    for(unsigned b=sector*32;b<sector*32+32;++b) bytes.set(b);
    mem_access_sector_mask_t mask;mask.set(sector);
#ifdef EPL2_MICROOPT_CANDIDATE
    if(ordinary) {
      warp_inst_t inst(gpu->getShaderCoreConfig());inst.op=LOAD_OP;inst.pc=0x100;
      inst.memory_op=memory_load;inst.data_size=4;
      inst.space=memory_space_t(global_space);inst.cache_op=CACHE_ALL;
      inst.ep_l2_mq2_ordinary_read=inst_t::ep_l2_plain_ldg({"LDG","E"});
      inst.issue(active,0,time,0,0,0);
      mem_access_t access(type,a,32,wr,active,bytes,mask,&context);
      return new mem_fetch(access,&inst,0,8,0,0,0,memory,time);
    }
#endif
    return allocator->alloc(a,type,active,bytes,mask,32,wr,time,0,0,0,NULL,0);
  }
  void cycle() {
    // Every direct fill/response/access and cache lower callback must use the
    // same physical cycle; native fixture time must never run one cycle ahead.
    assert(gpu->gpu_sim_cycle == time && gpu->gpu_tot_sim_cycle == 0);
    cache->cycle(); gpu->gpu_sim_cycle = ++time;
  }
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



