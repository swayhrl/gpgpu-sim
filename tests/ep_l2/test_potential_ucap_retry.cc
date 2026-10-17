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

struct retry_fixture {
  gpgpu_context context;
  gpgpu_sim_config config;
  exec_gpgpu_sim *gpu;
  const memory_config *memory;
  partition_mf_allocator *allocator;
  l2_cache *cache;
  lower_port port;
  unsigned time;
  retry_fixture(const char *base, const char *overlay, unsigned mode, unsigned c, unsigned sets=1)
      : config(&context), gpu(NULL), memory(NULL), allocator(NULL), cache(NULL), time(1) {
    option_parser_t p = option_parser_create();
    context.ptx_reg_options(p); context.func_sim->ptx_opcocde_latency_options(p);
    icnt_reg_options(p); config.reg_options(p);
    option_parser_cfgfile(p,base); option_parser_cfgfile(p,overlay);
    std::string mode_s=std::to_string(mode), c_s=std::to_string(c);
    const char *args[]={"ft-test","-gpgpu_ep_l2_ft_mode",mode_s.c_str(),"-gpgpu_ep_l2_wbuf_entries",c_s.c_str(),"-gpgpu_ep_l2_wb_local_cap","16"};
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
  ~retry_fixture() { delete cache; delete allocator; delete gpu; }
  mem_fetch *make(new_addr_type a, bool wr, mem_access_type type=GLOBAL_ACC_W) {
    active_mask_t active; active.set(0);
    mem_access_byte_mask_t bytes;
    unsigned sector=(a >> 5)&3;
    for(unsigned b=sector*32;b<sector*32+32;++b) bytes.set(b);
    mem_access_sector_mask_t mask;mask.set(sector);
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


int main(int argc,char **argv) {
  assert(argc==3);setvbuf(stdout,NULL,_IOLBF,0);retry_fixture f(argv[1],argv[2],2,16,64);
  std::map<new_addr_type,std::array<unsigned long long,4>> ref;
  const new_addr_type stride=64*128;
  for(unsigned i=0;i<16;++i) {
    auto w=f.make(i*stride,true);assert(f.access(w));
    ref[i*stride]={{(unsigned long long)w->get_request_uid()+1,0,0,0}};delete w;
  }
  // Three real lower read transactions for distinct bank3 residents. Production fill admission queues two older operations before
  // the dirty frontend's prospective resident write requests that bank.
  std::vector<mem_fetch *> reads;
  for(unsigned s=0;s<3;++s){auto r=f.make(128*(s+1),false,GLOBAL_ACC_R);assert(f.access(r));reads.push_back(r);}
  f.port.blocked=false;
  for(unsigned n=0;n<100 && f.port.packets.size()<3;++n)f.cycle();
  assert(f.port.packets.size()==3);f.port.blocked=true;
  std::vector<mem_fetch *> returns;while(!f.port.packets.empty()){returns.push_back(f.port.packets.front());f.port.packets.pop_front();}
  std::array<unsigned long long,4> data={{101,0,0,0}};
  for(auto r:returns)r->set_ft_wb_data(data);
  assert(f.cache->ep_l2_payload_fill_request(returns[0],f.time)==ep_l2_payload_store::GRANTED);f.cache->fill(returns[0],f.time);
  for(unsigned i=1;i<3;++i)assert(f.cache->ep_l2_payload_fill_request(returns[i],f.time)==ep_l2_payload_store::BANK_TRUE_CONTENTION);
  auto w=f.make(16*stride,true);std::list<cache_event> events;
  assert(f.cache->access(w->get_addr(),w,700001,events)==RESERVATION_FAIL);
  assert(f.cache->ft_wb_quota_reserved()==1 && !f.cache->ft_wb_quota_live());
  unsigned index=f.cache->ft_pending_index_for_test();
  for(unsigned i=1;i<3;++i) {
    f.cycle();
    assert(f.cache->ep_l2_payload_fill_request(returns[i],f.time)==ep_l2_payload_store::GRANTED);f.cache->fill(returns[i],f.time);
    assert(f.cache->access(w->get_addr(),w,700001+900*i,events)==RESERVATION_FAIL);
    assert(f.cache->ft_pending_index_for_test()==index && f.cache->ft_wb_quota_reserved()==1 && !f.cache->ft_wb_quota_live());
  }
  f.cycle();assert(f.cache->access(w->get_addr(),w,999999,events)!=RESERVATION_FAIL);delete w;
  assert(!f.cache->ft_wb_quota_reserved() && f.cache->ft_wb_quota_live()==1 && f.cache->ft_wb_quota_retries()>=3);
  unsigned replies=0;
  for(unsigned n=0;n<100 && replies<3;++n) {
    if(f.cache->access_ready() && f.cache->ft_response_ready(f.time)) {
      auto r=f.cache->peek_next_access();assert(r->get_ft_wb_data()==data);f.cache->commit_next_access();++replies;
    }
    f.cycle();
  }
  assert(replies==3);for(auto r:reads)delete r;
  f.drain_wb(ref);assert(f.cache->ft_drained() && !f.cache->ft_wb_quota_live() && !f.cache->ft_wb_quota_reserved());
  f.cache->ft_print(stdout,0);
  puts("U_CAP16 real three-line fill contention: multi-cycle native-time-varied retries one reserved token/one commit/exact data/drain PASS");
}
