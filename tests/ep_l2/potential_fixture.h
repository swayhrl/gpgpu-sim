#pragma once
// Real production memory_sub_partition/cache/tag paths. The external lower-memory
// fixture controls response order/latency through the real lower and Return FIFOs;
// it never mutates Tags, payloads, pending transactions or the bank arbiter.
#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

using versions = std::array<unsigned long long,4>;
struct fixture {
  gpgpu_context ctx;
  gpgpu_sim_config cfg;
  exec_gpgpu_sim *gpu;
  memory_stats_t *stats;
  memory_partition_unit *part;
  memory_sub_partition *sp;
  partition_mf_allocator *alloc;
  l2_cache *cache;
  std::map<new_addr_type,mem_fetch *> reads;
  std::map<new_addr_type,versions> memory;
  std::map<unsigned,versions> expected;
  std::set<unsigned> replies;
  std::vector<mem_fetch *> wb;
  bool block_lower = false;
  unsigned lower_wb = 0, completed_wb = 0, same_bank_copy = 0, cross_bank_copy = 0;
  std::map<new_addr_type,unsigned> source_banks;
  unsigned long long bank_max = 0;
  fixture(char **argv, const char *queues="512:2:64:64",
      const char *geometry="S:64:128:16,L:B:m:L:L,A:128:1,256:0,32"):cfg(&ctx) {
    option_parser_t p=option_parser_create();
    ctx.ptx_reg_options(p);ctx.func_sim->ptx_opcocde_latency_options(p);icnt_reg_options(p);cfg.reg_options(p);
    option_parser_cfgfile(p,argv[1]);option_parser_cfgfile(p,argv[2]);
    const char *args[]={"a1","-gpgpu_ep_l2_ft_mode",argv[3],"-gpgpu_ep_l2_wbuf_entries",argv[4],
      "-gpgpu_cache:dl2",geometry,
      "-gpgpu_dram_partition_queues",queues,
      "-gpgpu_ep_l2_static_s64",argv[5],
      "-gpgpu_ep_l2_wb_local_cap",argv[6],"-gpgpu_ep_l2_bank_ops",argv[7]};
    option_parser_cmdline(p,15,args);cfg.init();option_parser_destroy(p);
    gpu=new exec_gpgpu_sim(cfg,&ctx);gpu->gpu_sim_cycle=1;
    auto mc=gpu->getMemoryConfig();
    assert(mc->m_L2_config.get_num_lines()==1024 && mc->m_L2_config.get_assoc()==16);
    stats=new memory_stats_t(cfg.num_shader(),gpu->getShaderCoreConfig(),mc,gpu);
    part=new memory_partition_unit(0,mc,stats,gpu);sp=part->get_sub_partition(0);
    cache=sp->ft_cache_for_test();alloc=new partition_mf_allocator(mc);
  }
  ~fixture(){delete alloc;delete part;delete stats;delete gpu;}
  mem_fetch *make(new_addr_type addr,bool write) {
    active_mask_t active;active.set(0);mem_access_byte_mask_t bytes;
    unsigned sec=(addr>>5)&3;for(unsigned i=32*sec;i<32*(sec+1);++i)bytes.set(i);
    mem_access_sector_mask_t sectors;sectors.set(sec);
    auto mf=alloc->alloc(addr,write?GLOBAL_ACC_W:GLOBAL_ACC_R,active,bytes,sectors,32,
        write,gpu->gpu_sim_cycle,0,0,0,NULL,0);
    versions v={{0,0,0,0}};
    if(write) v[sec]=(unsigned long long)mf->get_request_uid()+1;
    else {
      auto line=addr&~127ull;
      if(!memory.count(line)) memory[line]={{line+11,line+22,line+33,line+44}};
      v[sec]=memory[line][sec];
    }
    expected[mf->get_request_uid()]=v;
    return mf;
  }
  void step() {
    unsigned long long before[4];for(unsigned b=0;b<4;++b)before[b]=cache->ep_l2_payload_bank_grants(b);
    part->cache_cycle(gpu->gpu_sim_cycle);
    cache->ft_assert_quota();
    for(unsigned b=0;b<4;++b) {
      auto n=cache->ep_l2_payload_bank_grants(b)-before[b];assert(n<=cache->ep_l2_payload_for_test().bank_service_limit());bank_max=std::max(bank_max,n);
    }
    while(auto r=sp->pop()) {
      assert(expected.count(r->get_request_uid()) && replies.insert(r->get_request_uid()).second);
      if(!r->get_is_write()) {
        assert(r->has_ft_wb_data());unsigned sec=(r->get_addr()>>5)&3;
        assert(r->get_ft_wb_data()[sec]==expected.at(r->get_request_uid())[sec]);
      }
      delete r;
    }
    for(unsigned t=0;t<1024;++t) {
      auto h=cache->ep_l2_tag_payload_handle_for_test(t);
      if(h.valid() && cache->ep_l2_payload_for_test().handle_live(h)) {
        auto &slot=cache->ep_l2_payload_for_test().inspect(h);
        source_banks[slot.owner]=h.payload_id%4;
      }
    }
    if(!block_lower) drain_lower();
    ++gpu->gpu_sim_cycle;
  }
  void drain_lower() {
    while(!sp->L2_dram_queue_empty()) {
      auto r=sp->L2_dram_queue_top();sp->L2_dram_queue_pop();
      if(r->get_access_type()==L2_WRBK_ACC) {
        assert(r->has_ft_wb_data() && r->get_ft_wb_data()==memory.at(r->get_addr()));
        assert(cache->ep_l2_wad_contains(r->get_addr()));
        assert(source_banks.count(r->get_addr()));
        if(source_banks.at(r->get_addr())==r->get_ep_l2_payload_id()%4) ++same_bank_copy;
        else ++cross_bank_copy;
        ++lower_wb;wb.push_back(r);
      } else assert(reads.insert({r->get_addr(),r}).second);
    }
  }
  void send(mem_fetch *r) {assert(!sp->full());sp->push(r,gpu->gpu_sim_cycle);}
  void return_read(new_addr_type addr) {
    auto it=reads.find(addr);assert(it!=reads.end());auto r=it->second;reads.erase(it);
    assert(!sp->dram_L2_queue_full());r->set_ft_wb_data(memory.at(addr&~127ull));r->set_reply();
    sp->dram_L2_queue_push(r);
  }
  void until_frontend_empty() {
    for(unsigned i=0;i<1000;++i) {step();if(!sp->ep_l2_icntl2_occupancy()) {step();return;}}
    assert(false);
  }
};

