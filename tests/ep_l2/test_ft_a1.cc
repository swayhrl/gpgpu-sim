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
  unsigned lower_wb = 0, completed_wb = 0;
  unsigned long long bank_max = 0;
  fixture(char **argv):cfg(&ctx) {
    option_parser_t p=option_parser_create();
    ctx.ptx_reg_options(p);ctx.func_sim->ptx_opcocde_latency_options(p);icnt_reg_options(p);cfg.reg_options(p);
    option_parser_cfgfile(p,argv[1]);option_parser_cfgfile(p,argv[2]);
    const char *args[]={"a1","-gpgpu_ep_l2_ft_mode",argv[3],"-gpgpu_ep_l2_wbuf_entries",argv[4],
      "-gpgpu_cache:dl2","S:64:128:16,L:B:m:L:L,A:128:1,256:0,32",
      "-gpgpu_dram_partition_queues","512:2:64:64"};
    option_parser_cmdline(p,9,args);cfg.init();option_parser_destroy(p);
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
    for(unsigned b=0;b<4;++b) {
      auto n=cache->ep_l2_payload_bank_grants(b)-before[b];assert(n<=1);bank_max=std::max(bank_max,n);
    }
    while(auto r=sp->pop()) {
      assert(expected.count(r->get_request_uid()) && replies.insert(r->get_request_uid()).second);
      if(!r->get_is_write()) {
        assert(r->has_ft_wb_data());unsigned sec=(r->get_addr()>>5)&3;
        assert(r->get_ft_wb_data()[sec]==expected.at(r->get_request_uid())[sec]);
      }
      delete r;
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

int main(int argc,char **argv) {
  assert(argc==5);setvbuf(stdout,NULL,_IOLBF,0);fixture f(argv);
  unsigned mode=std::stoul(argv[3]),c=std::stoul(argv[4]);
  const new_addr_type stride=64*128, victim=12*stride, incoming=16*stride, other=128;
  // Invalid-way selection is descending: read0->way15, dirty12->way3;
  // all other fifteen ways retain real lower read sectors RESERVED.
  for(unsigned i=0;i<16;++i) {
    auto r=f.make(i*stride,i==12);
    if(i==12) f.memory[victim]=f.expected.at(r->get_request_uid());
    f.send(r);f.until_frontend_empty();
  }
  assert(f.cache->ft_reserved_ways_for_test(0)==15 && f.reads.size()==15);
  auto held=f.cache->ep_l2_tag_payload_handle_for_test(3);
  assert(f.cache->ep_l2_payload_for_test().inspect(held).versions==f.memory.at(victim));
  // For U_WB, a real ready response from another set (also bank3) consumes
  // the port before frontend admission, creating its cross-cycle retry.
  if(mode==2) {
    auto r=f.make(other,false);f.send(r);f.until_frontend_empty();
  }
  auto request=f.make(incoming,true);unsigned request_uid=request->get_request_uid();
  unsigned first=(unsigned)-1;assert(f.cache->ft_raw_probe_for_test(request,first)==MISS && first==3);
  f.send(request);
  if(mode==2) { f.return_read(other);f.step();assert(f.cache->access_ready()); }
  // push has a one-cycle ROP-to-frontend boundary. For U ensure the ready
  // response is consumed on the actual admission cycle by pre-queueing request.
  for(unsigned n=0;n<8 && !f.cache->ft_reservation_active();++n) f.step();
  assert(f.cache->ft_reservation_active() && f.cache->ft_pending_index_for_test()==3);
  assert(mode==2 || f.cache->ft_pending_phase_for_test()==1);
  assert(f.cache->ep_l2_wad_contains(victim));
  printf("A1 armed mode=%u C=%u native16way=1 reserved=15 pending=3 phase=%u victim_id=%u generation=%u\n",
      mode,c,f.cache->ft_pending_phase_for_test(),held.payload_id,held.generation);
  // This real old read enters the native ReturnQ after phase0/denial and
  // before the next frontend retry. Production fill precedes frontend probe.
  f.return_read(0);
  f.step(); // Old source must fail its pending.tag_index==reprobe.index assert.
  // Remaining fills and real lower FIFO accept; deliberately hold set_done
  // until staging is gone, proving WAD's distinct lower lifetime.
  for(unsigned n=0;n<1000 && !f.replies.count(request_uid);++n) f.step();
  assert(f.replies.count(request_uid));
  for(auto &entry:f.reads) { (void)entry; } // snapshot keys before consuming
  std::vector<new_addr_type> remaining;for(auto &entry:f.reads)remaining.push_back(entry.first);
  for(auto addr:remaining){f.return_read(addr);f.step();}
  for(unsigned n=0;n<1000 && (f.cache->ft_wb_staged() || !f.cache->miss_queue_empty() || !f.sp->L2_dram_queue_empty());++n)f.step();
  assert(f.lower_wb==1 && f.cache->ft_wb_staged()==0 && f.cache->ep_l2_wad_contains(victim));
  for(auto w:f.wb){f.sp->set_done(w);delete w;++f.completed_wb;}f.wb.clear();
  for(unsigned n=0;n<1000 && f.part->busy();++n)f.step();
  assert(f.completed_wb==1 && f.replies.size()==f.expected.size());
  assert(f.sp->l2_char_no_resource_leak() && f.cache->ft_drained() && !f.part->busy());
  assert(f.cache->ep_l2_payload_pending_operations()==0 && !f.cache->ep_l2_wad_occupancy());
  assert(f.bank_max==1 && f.cache->ep_l2_payload_for_test().physical_slots()==1152+c);
  f.cache->ft_print(stdout,0);
  printf("A1 production 16way cross-cycle fill/exact-once/versions/lower-accept/set_done/bank/drain mode=%u C=%u PASS\n",mode,c);
}
