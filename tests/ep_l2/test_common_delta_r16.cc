// Real production l2_cache/MSHR/Tag calls; lower returns are deliberately
// scheduled by the fixture. No extracted/reimplemented cache algorithm.
#include <cassert>
#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/option_parser.h"

static void require(bool ok,const char *reason) {
  if(!ok){fprintf(stderr,"COMMON_DELTA_FAIL|%s\n",reason);exit(42);}
}
static unsigned callbacks=0;
static void atomic_callback(const inst_t *,ptx_thread_info *) {++callbacks;}
struct delta_port:mem_fetch_interface {
  bool blocked=true;std::deque<mem_fetch*> packets;
  bool full(unsigned,bool)const override{return blocked;}
  void push(mem_fetch *mf)override{packets.push_back(mf);}
};
struct inspected_cache:l2_cache {
  using l2_cache::l2_cache;
  cache_block_t *block(unsigned index=0){return m_tag_array->get_block(index);}
};
struct delta_fixture {
  gpgpu_context ctx;gpgpu_sim_config cfg;exec_gpgpu_sim *gpu;
  partition_mf_allocator *alloc;inspected_cache *cache;delta_port port;
  unsigned now=1,mode;std::set<unsigned> responses;
  delta_fixture(const char *base,const char *overlay,unsigned ft=2,unsigned sets=1,unsigned ways=1)
      :cfg(&ctx),mode(ft) {
    auto p=option_parser_create();ctx.ptx_reg_options(p);
    ctx.func_sim->ptx_opcocde_latency_options(p);icnt_reg_options(p);cfg.reg_options(p);
    option_parser_cfgfile(p,base);option_parser_cfgfile(p,overlay);
    const auto ms=std::to_string(ft);
    const char *args[]={"common-delta","-gpgpu_ep_l2_ft_mode",ms.c_str(),
      "-gpgpu_ep_l2_wbuf_entries",ft?"16":"0","-gpgpu_ep_l2_payload_mode",ft?"2":"0"};
    option_parser_cmdline(p,7,args);
    const auto geometry="S:"+std::to_string(sets)+":128:"+std::to_string(ways)+",L:B:m:L:L,A:128:1,256:0,32";
    const char *geo[]={"common-delta","-gpgpu_cache:dl2",geometry.c_str()};
    option_parser_cmdline(p,3,geo);cfg.init();option_parser_destroy(p);
    gpu=new exec_gpgpu_sim(cfg,&ctx);gpu->gpu_sim_cycle=now;
    const auto memory=gpu->getMemoryConfig();alloc=new partition_mf_allocator(memory);
    cache=new inspected_cache("COMMON_DELTA",const_cast<memory_config*>(memory)->m_L2_config,
      0,0,&port,alloc,IN_PARTITION_L2_MISS_QUEUE,gpu,L2_GPU_CACHE);
  }
  ~delta_fixture(){delete cache;delete alloc;delete gpu;}
  mem_fetch *make(new_addr_type address,bool atomic=false,bool write=false) {
    active_mask_t active;active.set(0);mem_access_sector_mask_t sectors;sectors.set((address>>5)&3);
    mem_access_byte_mask_t bytes;unsigned first=address&127;
    // Atomic touches four bytes, ordinary reads/writes complete a sector.
    for(unsigned n=0;n<(atomic?4u:32u);++n)bytes.set(first+n);
    if(atomic){
      warp_inst_t inst(gpu->getShaderCoreConfig());inst.initiation_interval=1;
      inst.space=memory_space_t(global_space);inst.issue(active,0,now,0,0,0);
      // An opaque non-null cookie is never dereferenced by this callback.
      inst.add_callback(0,atomic_callback,nullptr,reinterpret_cast<ptx_thread_info*>(&callbacks),true);
      mem_access_t access(GLOBAL_ACC_R,address,32,false,active,bytes,sectors,&ctx);
      auto mf=new mem_fetch(access,&inst,0,8,0,0,0,gpu->getMemoryConfig(),now);
      assert(mf->isatomic());return mf;
    }
    return alloc->alloc(address,write?GLOBAL_ACC_W:GLOBAL_ACC_R,active,bytes,sectors,32,write,now,0,0,0,nullptr,0);
  }
  void tick(){cache->cycle();gpu->gpu_sim_cycle=++now;}
  bool access(mem_fetch *mf,unsigned limit=100){
    for(unsigned n=0;n<limit;++n){std::list<cache_event> events;
      auto status=cache->access(mf->get_addr(),mf,now,events);tick();
      if(status!=RESERVATION_FAIL)return true;
    }return false;
  }
  std::map<unsigned,mem_fetch*> lower(unsigned count){
    port.blocked=false;std::map<unsigned,mem_fetch*> result;
    for(unsigned n=0;n<1000 && result.size()<count;++n){tick();
      while(!port.packets.empty()){
        auto mf=port.packets.front();port.packets.pop_front();
        require(mf->get_access_type()!=L2_WRBK_ACC,"unexpected WB during lower collection");
        result[(mf->get_addr()>>5)&3]=mf;
      }
    }require(result.size()==count,"lower issue count");return result;
  }
  void fill(mem_fetch *mf,const std::array<unsigned long long,4>&versions){
    mf->set_ft_wb_data(versions);
    while(cache->ep_l2_payload_fill_request(mf,now)!=ep_l2_payload_store::GRANTED)tick();
    cache->fill(mf,now);tick();
  }
  void respond(unsigned count){
    for(unsigned n=0;n<count;++n){
      require(cache->access_ready(),"missing production descriptor response");
      while(!cache->ft_response_ready(now))tick();
      auto mf=cache->peek_next_access();
      require(responses.insert(mf->get_request_uid()).second,"duplicate response UID");
      cache->commit_next_access();
      // Cache does not execute the ALU callback. Like subpartition::pop(),
      // the simulated consumer executes it after real response retirement.
      if(mf->isatomic())mf->do_atomic();tick();
    }
  }
  void wb(unsigned expected_mask,const mem_access_byte_mask_t&bytes,
          const std::array<unsigned long long,4>&versions){
    auto w=make(128,false,true);require(access(w),"eviction request accepted");delete w;
    port.blocked=false;bool seen=false;
    for(unsigned n=0;n<1000 && !seen;++n){tick();
      if(port.packets.empty())continue;
      auto mf=port.packets.front();port.packets.pop_front();
      require(mf->get_access_type()==L2_WRBK_ACC,"expected actual WB packet");
      printf("WB|sector_mask=%lu|dirty_bytes=%zu|versions=%llu,%llu,%llu,%llu\n",
        mf->get_access_sector_mask().to_ulong(),mf->get_access_byte_mask().count(),
        mf->get_ft_wb_data()[0],mf->get_ft_wb_data()[1],mf->get_ft_wb_data()[2],mf->get_ft_wb_data()[3]);
      require(mf->get_access_sector_mask().to_ulong()==expected_mask,"WB sector mask contaminated");
      require(mf->get_access_byte_mask()==bytes,"WB byte mask contaminated");
      require(mf->has_ft_wb_data()&&mf->get_ft_wb_data()==versions,"WB stale symbolic atomic version");
      require(cache->ep_l2_wad_contains(0),"WAD survives lower accept");
      cache->ep_l2_wad_complete(0,now);delete mf;seen=true;
    }
    require(seen&&cache->ft_drained()&&!cache->mshr_entries_used()&&!cache->ep_l2_wad_occupancy(),"WB/MSHR/pin drain");
  }
};

static void mixed(const char *base,const char *overlay,bool ordinary_first,bool retire_atomic_early,unsigned mode,bool check_versions,bool prior_dirty=false){
  delta_fixture f(base,overlay,mode);callbacks=0;
  mem_access_byte_mask_t prior_bytes;unsigned long long prior_version=400;
  if(prior_dirty){auto w=f.make(96,false,true);require(f.access(w),"prior dirty sector write");
    prior_bytes=w->get_access_byte_mask();prior_version=(unsigned long long)w->get_request_uid()+1;delete w;}
  auto atomic=f.make(0,true),read=f.make(32);
  require(f.access(atomic)&&f.access(read),"same-line atomic/read production admission");
  auto packets=f.lower(2);std::array<unsigned long long,4> memory{{100,200,300,prior_version}};
  const unsigned first=ordinary_first?1:0,second=1-first;
  f.fill(packets.at(first),memory);
  auto dirty=f.cache->block()->get_dirty_sector_mask().to_ulong();
  printf("FILL1|order=%s|dirty=%lu|callbacks=%u\n",ordinary_first?"read-first":"atomic-first",dirty,callbacks);
  require(dirty==((prior_dirty?8u:0u)|(ordinary_first?0u:1u)),"ordinary sector incorrectly marked atomic dirty");
  require(callbacks==0,"fill must not execute atomic ALU callback");
  if(retire_atomic_early){f.respond(1);require(callbacks==1,"atomic callback at consumer exactly once");}
  f.fill(packets.at(second),memory);
  dirty=f.cache->block()->get_dirty_sector_mask().to_ulong();
  printf("FILL2|dirty=%lu|dirty_bytes=%zu\n",dirty,f.cache->block()->get_dirty_byte_mask().count());
  require(dirty==(prior_dirty?9u:1u),"late ordinary fill contaminated dirty sectors");
  require(f.cache->block()->get_dirty_byte_mask()==(atomic->get_access_byte_mask()|prior_bytes),"dirty bytes must come from atomic requester");
  f.respond(retire_atomic_early?1:2);require(callbacks==1,"one callback for one atomic response");
  if(mode){
    const auto h=f.cache->ep_l2_tag_payload_handle_for_test(0);
    const auto got=f.cache->ep_l2_payload_for_test().inspect(h).versions;
    printf("VERSION|atomic_uid=%u|got=%llu|expected=%llu|ordinary=%llu\n",atomic->get_request_uid(),got[0],(unsigned long long)atomic->get_request_uid()+1,got[1]);
    if(check_versions)require(got[0]==(unsigned long long)atomic->get_request_uid()+1 && got[1]==200,"atomic miss missing symbolic completion version");
    auto expected=memory;expected[0]=(unsigned long long)atomic->get_request_uid()+1;
    f.wb(prior_dirty?9:1,atomic->get_access_byte_mask()|prior_bytes,check_versions?expected:got);
  }
  require(!f.cache->mshr_entries_used()&&!f.cache->access_ready(),"all descriptors retired");
  delete atomic;delete read;
  puts("PASS B1 real MSHR/Tag mixed sector fill/response/WB");
}
static void version_only(const char *base,const char *overlay){
  delta_fixture f(base,overlay,2);callbacks=0;auto a=f.make(0,true);
  require(f.access(a),"single atomic miss admission");auto packets=f.lower(1);
  std::array<unsigned long long,4> memory{{100,0,0,0}};f.fill(packets.at(0),memory);f.respond(1);
  const auto h=f.cache->ep_l2_tag_payload_handle_for_test(0);
  const auto got=f.cache->ep_l2_payload_for_test().inspect(h).versions;
  printf("VERSION_ONLY|got=%llu|expected=%llu|callbacks=%u\n",got[0],(unsigned long long)a->get_request_uid()+1,callbacks);
  require(got[0]==(unsigned long long)a->get_request_uid()+1,"atomic miss missing symbolic completion version");
  require(callbacks==1,"atomic callback belongs to response consumer");
  memory[0]=(unsigned long long)a->get_request_uid()+1;f.wb(1,a->get_access_byte_mask(),memory);
  delete a;puts("PASS B1 atomic miss completion version and later WB");
}
static void multi(const char *base,const char *overlay){
  // FT deliberately serializes atomics on a pinned native line; descriptor
  // mode without FT allows multiple atomic waiters and must also be correct.
  delta_fixture f(base,overlay,0);callbacks=0;
  auto a=f.make(0,true),b=f.make(64,true),r=f.make(32);
  require(f.access(a)&&f.access(b)&&f.access(r),"multiatomic native admission");
  auto packets=f.lower(3);std::array<unsigned long long,4> memory{{10,20,30,40}};
  f.fill(packets.at(2),memory);f.respond(1);require(callbacks==1,"first atomic retired");
  f.fill(packets.at(1),memory);
  require(f.cache->block()->get_dirty_sector_mask().to_ulong()==4,"ordinary late sector after retired atomic must remain clean");
  f.fill(packets.at(0),memory);f.respond(2);
  require(callbacks==2,"two atomic consumers exactly once");
  require(f.cache->block()->get_dirty_sector_mask().to_ulong()==5,"two atomic dirty sectors only");
  require(f.cache->block()->get_dirty_byte_mask()==(a->get_access_byte_mask()|b->get_access_byte_mask()),"multiple atomic byte masks");
  require(!f.cache->mshr_entries_used(),"multiatomic descriptor drain");
  delete a;delete b;delete r;puts("PASS B1 real native multiatomic late ordinary sector");
}
int main(int argc,char **argv){
  setvbuf(stdout,nullptr,_IOLBF,0);assert(argc==4);std::string test=argv[3];
  if(test=="b1-read-first")mixed(argv[1],argv[2],true,false,2,true);
  else if(test=="b1-atomic-first")mixed(argv[1],argv[2],false,true,2,true);
  else if(test=="b1-multi")multi(argv[1],argv[2]);
  else if(test=="b1-b16")mixed(argv[1],argv[2],true,false,1,true);
  else if(test=="b1-dirty")mixed(argv[1],argv[2],true,false,2,true,true);
  else if(test=="b1-version-only")version_only(argv[1],argv[2]);
  else return 78;
}
