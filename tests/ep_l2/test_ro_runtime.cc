// Production l2_cache + real allocator/mshr/Tag/lower/response hooks.
#include <cassert>
#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/ep-l2-ro-sha256.h"
#include "../../src/option_parser.h"
struct lower : mem_fetch_interface {
  bool blocked=true;std::deque<mem_fetch*> packets;
  bool full(unsigned,bool)const override{return blocked;}
  void push(mem_fetch*m)override{packets.push_back(m);}
};
struct rig {
  gpgpu_context ctx;gpgpu_sim_config cfg;exec_gpgpu_sim *gpu;
  partition_mf_allocator *alloc;l2_cache *cache;lower port;
  unsigned time=1;std::set<unsigned> replies;bool validate_reference=true;
  rig(const char *base,const char *overlay,const char *binding,unsigned d,unsigned b,unsigned sets=1,bool activate=true):cfg(&ctx) {
    option_parser_t p=option_parser_create();ctx.ptx_reg_options(p);ctx.func_sim->ptx_opcocde_latency_options(p);icnt_reg_options(p);cfg.reg_options(p);
    option_parser_cfgfile(p,base);option_parser_cfgfile(p,overlay);option_parser_cfgfile(p,binding);
    std::string ds=std::to_string(d),bs=std::to_string(b),geometry="S:"+std::to_string(sets)+":128:1,L:B:m:L:L,A:128:32,1024:0,32";
    const char *a[]={"ro-test","-gpgpu_ep_l2_ft_mode","2","-gpgpu_ep_l2_wbuf_entries","16","-gpgpu_ep_l2_ro_tag_detach",ds.c_str(),"-gpgpu_ep_l2_ro_line_mshr_bypass",bs.c_str(),"-gpgpu_cache:dl2",geometry.c_str()};
    option_parser_cmdline(p,11,a);cfg.init();option_parser_destroy(p);
    gpu=new exec_gpgpu_sim(cfg,&ctx);auto mc=gpu->getMemoryConfig();alloc=new partition_mf_allocator(mc);
    cache=new l2_cache("RO_RUNTIME",const_cast<memory_config*>(mc)->m_L2_config,0,0,&port,alloc,IN_PARTITION_L2_MISS_QUEUE,gpu,L2_GPU_CACHE);
    if(activate){cache->ro_bind_commandlist(command.c_str());cache->ro_begin_epoch();}
  }
  static std::string command;
  ~rig(){delete cache;delete alloc;delete gpu;}
  mem_fetch*make(new_addr_type a,mem_access_type type=GLOBAL_ACC_R,bool wr=false){
    active_mask_t active;active.set(0);mem_access_byte_mask_t bytes;unsigned n=(a>>5)&3;
    for(unsigned b=32*n;b<32*n+32;++b)bytes.set(b);
    return alloc->alloc(a,type,active,bytes,mem_access_sector_mask_t(1<<n),32,wr,time,0,0,0,NULL,0);
  }
  void tick(){gpu->gpu_sim_cycle=time++;cache->cycle();}
  cache_request_status admit(mem_fetch*m) {
    gpu->gpu_sim_cycle=time;
    l2_access_plan plan;cache->preview_access(m->get_addr(),m,plan);
    std::list<cache_event> ev;auto r=cache->access(m->get_addr(),m,time,ev);tick();return r;
  }
  ep_l2::PayloadHandle tag(unsigned i=0){return cache->ep_l2_tag_payload_handle_for_test(i);}
  void issue(){port.blocked=false;for(unsigned n=0;n<2048&&!cache->miss_queue_empty();++n)tick();assert(cache->miss_queue_empty());}
  bool fill(mem_fetch*m){
    gpu->gpu_sim_cycle=time;
    bool done;
    if(m->ro_lower_packet()) {
      std::array<unsigned long long,4> v;for(unsigned n=0;n<4;++n)v[n]=0x1000+(m->get_addr()/128)*4+n;
      m->set_ft_wb_data(v);done=cache->ro_return(m,time);
    }
    else {done=cache->ep_l2_payload_fill_request(m,time)==ep_l2_payload_store::GRANTED;if(done)cache->fill(m,time);}
    tick();if(done&&m->ro_lower_packet())delete m;return done;
  }
  void consume(unsigned expected){
    for(unsigned n=0;n<10000&&replies.size()<expected;++n){
      gpu->gpu_sim_cycle=time;
      if(cache->access_ready()&&cache->ft_response_ready(time)){
        auto m=cache->peek_next_access();assert(replies.insert(m->get_request_uid()).second);
        if(validate_reference && cache->ro_enabled() && m->get_access_type()==GLOBAL_ACC_R && m->get_addr()<0x40000) {
          assert(m->has_ft_wb_data());for(unsigned n=0;n<4;++n)if(m->get_access_sector_mask().test(n))
            assert(m->get_ft_wb_data()[n]==0x1000+(m->get_addr()/128)*4+n);
        }
        // global cycle must be the read-grant/enqueue cycle.
        gpu->gpu_sim_cycle=time;cache->commit_next_access();delete m;
      }
      tick();
    }
    assert(replies.size()==expected);
  }
  void finish(){assert(cache->ft_drained());assert(!cache->ep_l2_descriptor_count_used());cache->ro_end_epoch();cache->ro_print(stdout,0);}
};
std::string rig::command;
static void modes(const char *base,const char *overlay,const char *binding,unsigned d,unsigned b){
  rig f(base,overlay,binding,d,b);auto a=f.make(0);assert(f.admit(a)!=RESERVATION_FAIL);auto h=f.tag();
  if(d||b){
    bool rejected=false;try{f.cache->ro_end_epoch();}catch(const std::logic_error&){rejected=true;}assert(rejected);
  }
  auto other=f.make(128);
  unsigned expect=1;
  if(d){
    assert(f.admit(other)!=RESERVATION_FAIL);auto a2=f.make(0);assert(f.admit(a2)!=RESERVATION_FAIL);expect=3;
    assert(b ? f.tag()!=h : f.cache->mshr_entries_used()==2);
    assert(f.cache->ep_l2_descriptor_count_used()==3);
  }else{assert(f.admit(other)==RESERVATION_FAIL);delete other;}
  if(b)assert(f.cache->mshr_entries_used()==0);
  f.issue();
  // Reverse lower-return order: old A may arrive after new A/reused Tag.
  while(!f.port.packets.empty()) {auto p=f.port.packets.back();f.port.packets.pop_back();while(!f.fill(p)){} }
  assert(f.cache->ep_l2_descriptor_count_used()==expect);
  // Model ICNT full: leave all responses uncommitted for several cycles.
  for(unsigned n=0;n<5;++n)f.tick();
  assert(f.cache->ep_l2_descriptor_count_used()==expect);
  f.consume(expect);f.finish();printf("RO production four-mode detach=%u bypass=%u PASS\n",d,b);
}
static void sectors(const char *base,const char *overlay,const char *binding){
  rig f(base,overlay,binding,1,1);auto a=f.make(0),b=f.make(96);assert(f.admit(a)!=RESERVATION_FAIL);auto h=f.tag();
  assert(f.admit(b)!=RESERVATION_FAIL);assert(f.tag()==h);auto victim=f.make(128);assert(f.admit(victim)==RESERVATION_FAIL);delete victim; // observed reuse protects Tag
  f.issue();assert(f.port.packets.size()==2);
  auto last=f.port.packets.back();f.port.packets.pop_back();
  auto &store=f.cache->ep_l2_payload_for_test();
  assert(store.request(h,false,f.time,~0ULL,ep_l2_payload_store::WB_READOUT)==ep_l2_payload_store::GRANTED);
  assert(!f.fill(last)); // same-bank return remains at its original queue/owner
  while(!f.fill(last)){}
  f.consume(1);assert(f.cache->ep_l2_descriptor_count_used()==1);
  auto first=f.port.packets.front();f.port.packets.pop_front();while(!f.fill(first)){}
  f.consume(2);f.finish();puts("RO actual attached merge, out-of-order sectors, waiter protection PASS");
}
static void capacity(const char *base,const char *overlay,const char *binding){
  rig f(base,overlay,binding,0,1,1024);
  for(unsigned i=0;i<384;++i)assert(f.admit(f.make(i*128))!=RESERVATION_FAIL);
  for(unsigned i=384;i<512;++i)assert(f.admit(f.make(0x400000+i*128))!=RESERVATION_FAIL);
  assert(f.cache->ep_l2_descriptor_count_used()==512);assert(f.cache->mshr_entries_used()==128);
  auto refused=f.make(512*128);assert(f.admit(refused)==RESERVATION_FAIL);delete refused;
  f.issue();while(!f.port.packets.empty()){auto p=f.port.packets.front();f.port.packets.pop_front();while(!f.fill(p)){} }
  f.consume(512);f.finish();puts("RO actual mixed native128/RO384 shared D512 capacity and drain PASS");
}
static void classifier(const char *base,const char *overlay,const char *binding){
  rig f(base,overlay,binding,1,1);
  auto synthetic=f.make(0,L2_WR_ALLOC_R);assert(f.admit(synthetic)!=RESERVATION_FAIL);
  assert(f.cache->mshr_entries_used()==1); // synthetic !is_write read never RO
  f.issue();auto p=f.port.packets.front();f.port.packets.pop_front();assert(!p->ro_lower_packet());while(!f.fill(p)){}
  // Consume directly; this fixture has no parent write for a synthetic request.
  f.consume(1);
  auto writer=f.make(0,GLOBAL_ACC_W,true);bool rejected=false;
  try{f.admit(writer);}catch(const std::logic_error&){rejected=true;}assert(rejected);delete writer;
  f.finish();puts("RO original GLOBAL_ACC_R predicate and certified writer rejection PASS");
}
static void dirty_victim(const char *base,const char *overlay,const char *binding){
  rig f(base,overlay,binding,1,1);auto w=f.make(0x400000,GLOBAL_ACC_W,true);
  assert(f.admit(w)!=RESERVATION_FAIL);auto token=(unsigned long long)w->get_request_uid()+1;delete w;
  auto a=f.make(0);assert(f.admit(a)!=RESERVATION_FAIL);assert(f.cache->ep_l2_wad_occupancy()==1);
  f.issue();while(!f.port.packets.empty()){
    auto p=f.port.packets.front();f.port.packets.pop_front();
    if(p->get_access_type()==L2_WRBK_ACC){assert(p->get_ft_wb_data()[0]==token);f.cache->ep_l2_wad_complete(p->get_addr(),f.time);delete p;}
    else while(!f.fill(p)){}
  }
  f.consume(1);f.finish();puts("RO production dirty-victim WB/WAD/version retention PASS");
}
static void native_pin_guard(const char *base,const char *overlay,const char *binding){
  rig f(base,overlay,binding,1,1);f.validate_reference=false;
  auto native=f.make(0,TEXTURE_ACC_R);assert(f.admit(native)!=RESERVATION_FAIL);auto h=f.tag();
  f.issue();auto p=f.port.packets.front();f.port.packets.pop_front();assert(!p->ro_lower_packet());while(!f.fill(p)){}
  assert(f.cache->ep_l2_payload_for_test().inspect(h).pins==1);
  auto reader=f.make(32);assert(f.admit(reader)!=RESERVATION_FAIL);f.issue();
  p=f.port.packets.front();f.port.packets.pop_front();assert(!p->ro_lower_packet());while(!f.fill(p)){}
  assert(f.cache->ep_l2_payload_for_test().inspect(h).allocation_role==ep_l2_payload_store::RESIDENT);
  f.consume(2);f.finish();puts("RO refuses takeover of foreign native-response pins PASS");
}
static void epoch_binding(const char *base,const char *overlay,const char *binding){
  rig f(base,overlay,binding,1,1,1,false);
  bool rejected=false;try{f.cache->ro_begin_epoch();}catch(const std::logic_error&){rejected=true;}assert(rejected);
  f.cache->ro_bind_commandlist(rig::command.c_str());f.cache->ro_begin_epoch();
  rejected=false;try{f.cache->ro_begin_epoch();}catch(const std::logic_error&){rejected=true;}assert(rejected);
  f.cache->ro_end_epoch();auto a=f.make(0);rejected=false;
  try{f.admit(a);}catch(const std::logic_error&){rejected=true;}assert(rejected);delete a;
  puts("RO command binding, single full epoch and post-end access rejection PASS");
}
static void warmup_scope(const char *base,const char *overlay,const char *binding){
  rig f(base,overlay,binding,1,1,1,false);f.validate_reference=false;
  f.cache->force_tag_access(0,4096,mem_access_sector_mask_t(1));auto h=f.tag();
  f.cache->force_tag_access(32,4097,mem_access_sector_mask_t(2));
  f.cache->force_tag_access(0,4098,mem_access_sector_mask_t(1));
  assert(f.tag()==h);const auto &slot=f.cache->ep_l2_payload_for_test().inspect(h);
  assert(slot.ready_sector_mask.to_ulong()==3 && !slot.pins && slot.pending_sector_mask.none());
  assert(slot.versions[0]==0 && slot.versions[1]==0);
  assert(f.cache->ep_l2_payload_for_test().bank_grants()==0); // untimed initialization
  f.cache->ro_bind_commandlist(rig::command.c_str());f.cache->ro_begin_epoch();
  auto a=f.make(0);assert(f.admit(a)==HIT);assert(a->get_ft_wb_data()[0]==0);delete a;
  bool rejected=false;try{f.cache->force_tag_access(0,8192,mem_access_sector_mask_t(1));}
  catch(const std::logic_error&){rejected=true;}assert(rejected);
  f.cache->ro_end_epoch();rejected=false;
  try{f.cache->force_tag_access(0,8193,mem_access_sector_mask_t(1));}
  catch(const std::logic_error&){rejected=true;}assert(rejected);
  assert(f.tag()==h);puts("RO warmed handle/sector reuse and initialization-only scope PASS");
}
int main(int argc,char **argv){
  assert(argc==5);rig::command=argv[4];
  assert(ep_l2_ro::sha256("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  assert(ep_l2_ro::sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  for(unsigned d=0;d<2;++d)for(unsigned b=0;b<2;++b)modes(argv[1],argv[2],argv[3],d,b);
  sectors(argv[1],argv[2],argv[3]);capacity(argv[1],argv[2],argv[3]);classifier(argv[1],argv[2],argv[3]);
  dirty_victim(argv[1],argv[2],argv[3]);native_pin_guard(argv[1],argv[2],argv[3]);epoch_binding(argv[1],argv[2],argv[3]);
  warmup_scope(argv[1],argv[2],argv[3]);
  puts("RO production runtime directed PASS");
}
