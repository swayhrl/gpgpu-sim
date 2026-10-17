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

struct p2_fixture {
  gpgpu_context context;
  gpgpu_sim_config config;
  exec_gpgpu_sim *gpu;
  const memory_config *memory;
  partition_mf_allocator *allocator;
  l2_cache *cache;
  lower_port port;
  unsigned time;
  p2_fixture(const char *base, const char *overlay, unsigned mode, unsigned c, unsigned sets=1,unsigned ports=1)
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
  ~p2_fixture() { delete cache; delete allocator; delete gpu; }
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



static void arbiter(unsigned mode,unsigned ports) {
  using P=ep_l2_payload_store;
  for(unsigned bad:{0u,3u}) {
    bool refused=false;try{P invalid(2,mode,16,false,bad);}catch(const std::invalid_argument&){refused=true;}assert(refused);
  }
  bool refused=false;try{P invalid(2,mode,8,false,2);}catch(const std::invalid_argument&){refused=true;}assert(refused);
  refused=false;try{P invalid(2,1,16,true,2);}catch(const std::invalid_argument&){refused=true;}assert(refused);
  for(unsigned pattern=0;pattern<3;++pattern) {
    P p(2,mode,16,false,ports);
    std::vector<P::payload_handle> h;
    for(unsigned id:{0u,4u,8u,1u,5u,9u})h.push_back(p.reserve(id,P::RESIDENT,128*(id+1),P::RESIDENT_VALID));
    bool w0=pattern!=0,w1=pattern==1;
    assert(p.request(h[0],w0,100)==P::GRANTED);
    auto second=p.request(h[1],w1,100);assert(second==(ports==2?P::GRANTED:P::BANK_TRUE_CONTENTION));
    assert(p.request(h[2],false,100)==P::BANK_TRUE_CONTENTION);
    assert(p.request(h[3],false,100)==P::GRANTED);
    assert(p.request(h[4],true,100)==(ports==2?P::GRANTED:P::BANK_TRUE_CONTENTION));
    // Cancellation retires a pending/selected identity without a consumed
    // port being refunded. Its stale generation remains invalid after reuse.
    p.cancel_request(h[2],false);
    assert(p.request(h[5],false,100)==P::BANK_TRUE_CONTENTION);
    p.cancel_request(h[5],false);
    if(ports==1){p.cancel_request(h[4],true);assert(p.request(h[1],w1,101)==P::GRANTED);}
    assert(!p.pending_operations());
    assert(p.bank_peak(0)==ports && p.bank_peak(1)==ports);
    // A denied retry can retain its selection for several cycles while
    // independent-bank traffic advances the same physical arbiter clock.
    assert(p.request(h[2],true,101)==(ports==2?P::GRANTED:P::BANK_TRUE_CONTENTION));
    if(ports==2)assert(p.request(h[1],true,101)==P::GRANTED);
    assert(p.request(h[0],false,101)==P::BANK_TRUE_CONTENTION);
    if(ports==1)p.cancel_request(h[2],true);
    assert(p.request(h[3],false,102)==P::GRANTED);
    assert(p.request(h[3],false,103)==P::GRANTED);
    assert(p.request(h[0],false,104)==P::GRANTED);
    assert(!p.pending_operations());
    assert(p.request(h[0],false,105)==P::GRANTED);
    assert(p.request(h[1],false,105)==(ports==2?P::GRANTED:P::BANK_TRUE_CONTENTION));
    assert(p.request(h[2],false,105)==P::BANK_TRUE_CONTENTION);
    auto before_cancel=p.bank_grants(0);
    assert(p.request(h[3],false,106)==P::GRANTED); // selects older bank0 work
    p.cancel_request(h[1],false);p.cancel_request(h[2],false);
    assert(!p.pending_operations() && p.bank_grants(0)==before_cancel);
    assert(p.request(h[2],false,106)==P::GRANTED); // new identity after cancel
    p.release(h[2]);assert(!p.handle_live(h[2]));
    auto fresh=p.reserve(8,P::RESIDENT,8888,P::RESIDENT_VALID);assert(fresh.generation!=h[2].generation);
    for(unsigned bad=0;bad<2;++bad) {
      pid_t child=fork();assert(child>=0);
      if(!child){if(bad==0)p.request(h[2],false,107);else p.request(fresh,false,1);_exit(0);}
      int status;assert(waitpid(child,&status,0)==child && WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
    }
    printf("P%u arbiter mode%u pattern%u true grants=%llu logical=%llu retries=%llu wait=%llu peaks=%u/%u PASS\n",ports,mode,pattern,p.bank_grants(),p.bank_logical_ops(),p.bank_retry_attempts(),p.bank_wait_cycles(),p.bank_peak(0),p.bank_peak(1));
  }
}
int main(int argc,char **argv) {
  assert(argc==5);setvbuf(stdout,NULL,_IOLBF,0);
  unsigned mode=std::stoul(argv[3]),ports=std::stoul(argv[4]);arbiter(mode,ports);
  p2_fixture f(argv[1],argv[2],mode,16,64,ports);
  std::map<new_addr_type,std::array<unsigned long long,4>> ref;
  const new_addr_type stride=64*128;
  for(unsigned i=0;i<16;++i) {
    auto w=f.make(i*stride,true);assert(f.access(w));
    ref[i*stride]={{(unsigned long long)w->get_request_uid()+1,0,0,0}};delete w;
  }
  std::vector<mem_fetch *> reads;
  for(unsigned s=0;s<3;++s){auto r=f.make(128*(s+1),false,GLOBAL_ACC_R);assert(f.access(r));reads.push_back(r);}
  f.port.blocked=false;for(unsigned n=0;n<100 && f.port.packets.size()<3;++n)f.cycle();
  assert(f.port.packets.size()==3);f.port.blocked=true;
  std::vector<mem_fetch *> returns;while(!f.port.packets.empty()){returns.push_back(f.port.packets.front());f.port.packets.pop_front();}
  std::array<unsigned long long,4> data={{101,0,0,0}};for(auto r:returns)r->set_ft_wb_data(data);
  std::vector<bool> filled(3,false);bool committed=false;unsigned fills=0,responses=0,peak=0;
  std::set<unsigned> response_uids;
  auto w=f.make(16*stride,true);std::list<cache_event> events;
  for(unsigned n=0;n<100 && (!committed || fills<3 || responses<3);++n) {
    unsigned long long before[4];for(unsigned b=0;b<4;++b)before[b]=f.cache->ep_l2_payload_bank_grants(b);
    for(unsigned i=0;i<3;++i)if(!filled[i] && f.cache->ep_l2_payload_fill_request(returns[i],f.time)==ep_l2_payload_store::GRANTED) {
      f.cache->fill(returns[i],100001+1000*n);filled[i]=true;++fills;
    }
    if(!committed)committed=f.cache->access(w->get_addr(),w,200001+1000*n,events)!=RESERVATION_FAIL;
    if(f.cache->access_ready() && f.cache->ft_response_ready(f.time)) {
      auto r=f.cache->peek_next_access();assert(r->get_ft_wb_data()==data && response_uids.insert(r->get_request_uid()).second);
      f.cache->commit_next_access();++responses;
    }
    for(unsigned b=0;b<4;++b){auto d=f.cache->ep_l2_payload_bank_grants(b)-before[b];assert(d<=ports);peak=std::max(peak,(unsigned)d);}
    f.cycle();
  }
  assert(committed && fills==3 && responses==3 && peak==ports);
  for(auto r:reads)delete r;delete w;
  assert(f.cache->ft_wb_staged()==1 && f.cache->ep_l2_wad_occupancy()==1);
  // Real lower readout and a same-bank native resident hit write share one
  // physical cycle: P2 admits both, P1 waits. Native timestamps do not refill.
  f.port.blocked=false;
  auto before=f.cache->ep_l2_payload_bank_grants();f.cache->cycle();
  assert(f.port.packets.size()==1 && !f.cache->ft_wb_staged() && f.cache->ep_l2_wad_occupancy()==1);
  auto hit=f.make((mode==1?3:4)*stride,true);
  auto result=f.cache->access(hit->get_addr(),hit,9999999,events);
  assert((result!=RESERVATION_FAIL)==(ports==2));
  if(result==RESERVATION_FAIL){f.cycle();assert(f.cache->access(hit->get_addr(),hit,19999999,events)!=RESERVATION_FAIL);}
  else assert(f.cache->ep_l2_payload_bank_grants()-before==2);
  delete hit;
  auto packet=f.port.packets.front();f.port.packets.pop_front();assert(packet->get_ft_wb_data()==ref.at(packet->get_addr()));
  f.cache->ep_l2_wad_complete(packet->get_addr(),f.time);delete packet;
  assert(f.cache->ft_drained() && !f.cache->ep_l2_wad_occupancy() && !f.cache->ep_l2_payload_pending_operations());
  assert(f.cache->ep_l2_payload_bank_fill_write()==3 && f.cache->ep_l2_payload_bank_resident_hit_read()==3);
  assert(f.cache->ep_l2_payload_bank_wb_readout()==(mode==1?2:1));
  for(unsigned b=0;b<4;++b)assert(f.cache->ep_l2_payload_for_test().bank_peak(b)<=ports);
  f.cache->ft_print(stdout,0);
  printf("P%u real mode%u fill/response/resident-write/copy/WB contention native-time-varied physical budget peak%u exact-once/data/drain PASS\n",ports,mode,peak);
}
