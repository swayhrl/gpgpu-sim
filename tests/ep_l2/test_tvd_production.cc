// Actual memory_sub_partition/cache/memport/response producers. Version labels
// are symbolic complete-sector labels; dataless SASS is not full-byte evidence.
#include <cassert>
#include <cstdio>
#include <map>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/gpgpu-sim/l2cache.h"
#include "../../src/gpgpu-sim/mem_latency_stat.h"
#include "../../src/option_parser.h"

struct scenario {
  gpgpu_context context; gpgpu_sim_config cfg;
  exec_gpgpu_sim *gpu; memory_stats_t *stats; memory_partition_unit *part;
  memory_sub_partition *sp; partition_mf_allocator *alloc;
  std::map<unsigned, unsigned long long> responses;
  explicit scenario(const char *base, const char *overlay, bool dirty=false, bool tvd=true, bool hold=false, unsigned sets=1)
      : cfg(&context) {
    option_parser_t p=option_parser_create(); context.ptx_reg_options(p);
    context.func_sim->ptx_opcocde_latency_options(p); icnt_reg_options(p); cfg.reg_options(p);
    option_parser_cfgfile(p,base); option_parser_cfgfile(p,overlay);
    const char *args[]={"tvd-test", "-gpgpu_ep_l2_ft_mode","2", "-gpgpu_ep_l2_wbuf_entries","16",
      "-gpgpu_dram_partition_queues","64:128:64:1", "-gpgpu_ep_l2_tvd_clean",tvd?"1":"0",
      "-gpgpu_ep_l2_tvd_dirty",dirty?"1":"0"};
    option_parser_cmdline(p,11,args);
    std::string geometry="S:"+std::to_string(sets)+":128:1,L:B:m:L:L,A:128:1,256:0,32";
    const char *geo[]={"tvd-test","-gpgpu_cache:dl2",geometry.c_str()}; option_parser_cmdline(p,3,geo);
    if (hold) {
      const char *block[]={"tvd-test","-gpgpu_dram_partition_queues","2048:2:64:1",
        "-gpgpu_l2_char_dram_issue_hold_after_issues","0", "-gpgpu_l2_char_dram_issue_hold_cycles","1000000"};
      option_parser_cmdline(p,7,block);
    }
    cfg.init(); option_parser_destroy(p);
    gpu=new exec_gpgpu_sim(cfg,&context); const memory_config *mc=gpu->getMemoryConfig();
    stats=new memory_stats_t(cfg.num_shader(),gpu->getShaderCoreConfig(),mc,gpu);
    part=new memory_partition_unit(0,mc,stats,gpu); sp=part->get_sub_partition(0);
    alloc=new partition_mf_allocator(mc);
  }
  ~scenario() { delete alloc; delete part; delete stats; delete gpu; }
  l2_cache *cache() { return sp->tvd_cache_for_test(); }
  mem_fetch *make(unsigned address, bool write=false) {
    active_mask_t active; active.set(0); mem_access_byte_mask_t bytes;
    const unsigned sector=(address>>5)&3;
    for(unsigned i=sector*32;i<sector*32+32;++i) bytes.set(i);
    mem_access_sector_mask_t sectors; sectors.set(sector);
    return alloc->alloc(address,write?GLOBAL_ACC_W:GLOBAL_ACC_R,active,bytes,sectors,32,write,
        gpu->gpu_sim_cycle,0,0,0,NULL,0);
  }
  unsigned push_atomic(unsigned address) {
    active_mask_t active; active.set(0); mem_access_byte_mask_t bytes;
    unsigned sector=(address>>5)&3;
    for(unsigned b=sector*32;b<sector*32+32;++b) bytes.set(b);
    mem_access_sector_mask_t mask; mask.set(sector);
    warp_inst_t instruction(gpu->getShaderCoreConfig()); instruction.initiation_interval=1;
    instruction.issue(active,0,gpu->gpu_sim_cycle,0,0,0);
    instruction.add_callback(0,NULL,NULL,NULL,true);
    mem_access_t access(GLOBAL_ACC_R,address,32,false,active,bytes,mask,&context);
    auto mf=new mem_fetch(access,&instruction,0,8,0,0,0,gpu->getMemoryConfig(),gpu->gpu_sim_cycle);
    assert(mf->isatomic()); unsigned uid=mf->get_request_uid();
    sp->push(mf,gpu->gpu_sim_cycle); return uid;
  }
  unsigned push(unsigned address, bool write=false) {
    auto mf=make(address,write); unsigned uid=mf->get_request_uid();
    sp->push(mf,gpu->gpu_sim_cycle); return uid;
  }
  void collect() {
    while(auto mf=sp->pop()) {
      assert(!responses.count(mf->get_request_uid()));
      unsigned sector=(mf->get_addr()>>5)&3;
      responses[mf->get_request_uid()]=mf->has_ft_wb_data()?mf->get_ft_wb_data()[sector]:0;
      delete mf;
    }
  }
  void step(bool drain=true) {
    part->cache_cycle(gpu->gpu_sim_cycle); part->simple_dram_model_cycle();
    ++gpu->gpu_sim_cycle; if(drain) collect();
  }
  void await(unsigned uid) {
    for(unsigned n=0;n<10000&&!responses.count(uid);++n) step();
    assert(responses.count(uid));
  }
  void drain() {
    for(unsigned n=0;n<200000 && part->busy();++n) step();
    assert(!part->busy() && !sp->ep_l2_wad_occupancy());
    assert(sp->l2_char_no_resource_leak());
  }
};

static void clean_actual_frontend(const char *base,const char *overlay) {
  scenario f(base,overlay);
  const auto a=f.push(0);f.await(a); // explicitly initialized symbolic memory version 0
  assert(f.responses.at(a)==0);
  auto original=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  auto b=f.push(128,true);
  for(unsigned n=0;n<1000 && !f.sp->ep_l2_l2icnt_occupancy();++n) f.step(false);
  assert(f.sp->ep_l2_l2icnt_occupancy()==1);
  auto dir=f.cache()->tvd_for_test();auto t=dir->find(0);
  assert(dir->live(t)&&dir->at(t.index).payload==original);
  const auto lower_before=f.sp->ep_l2_lower_read_issue_count();
  auto read=f.push(0);
  for(unsigned n=0;n<8;++n) f.step(false);
  assert(dir->stats().read_responses==0 && f.sp->ep_l2_icntl2_occupancy()==1);
  f.collect();assert(f.responses.count(b));
  // Force one real same-bank conflict using the public arbiter.
  auto resident=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  auto &pool=f.cache()->ep_l2_payload_for_test();
  assert(pool.request(resident,false,f.gpu->gpu_sim_cycle)==ep_l2_payload_store::GRANTED);
  f.step(false);
  assert(dir->stats().read_responses==0);
  f.await(read);
  assert(f.responses.at(read)==0 && dir->stats().read_responses==1);
  assert(dir->stats().served_sector_bytes==32 && f.sp->ep_l2_lower_read_issue_count()==lower_before);
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==resident); // no read promotion
  f.cache()->tvd_print(stdout,0);f.drain();f.sp->flushL2();assert(!dir->occupied());
  puts("PASS production_clean_victim_lookup_bank_icnt_same_handle_no_lower_read");
}

static void clean_partial_sector_fallback(const char *base,const char *overlay) {
  scenario f(base,overlay); auto a=f.push(0);f.await(a);auto b=f.push(128);f.await(b);
  auto dir=f.cache()->tvd_for_test();assert(dir->live(dir->find(0)));
  const auto lower=f.sp->ep_l2_lower_read_issue_count();
  auto a1=f.push(32);f.await(a1);
  assert(dir->stats().read_responses==0 && f.sp->ep_l2_lower_read_issue_count()==lower+1);
  assert(!dir->live(dir->find(0)) && f.responses.at(a1)==0);
  f.drain();f.sp->invalidateL2();assert(!dir->occupied());
  puts("PASS production_clean_missing_sector_fallback_epoch");
}

static void clean_write_promotes_dirty_resident(const char *base,const char *overlay) {
  scenario f(base,overlay);auto a=f.push(0);f.await(a);
  auto old=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  auto b=f.push(128,true);f.await(b);
  auto w=f.push(0,true);f.await(w);
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==old);
  assert(f.cache()->tvd_for_test()->stats().promotions==1);
  assert(!f.cache()->tvd_for_test()->live(f.cache()->tvd_for_test()->find(0)));
  auto read=f.push(0);f.await(read);assert(f.responses.at(read)==(unsigned long long)w+1);
  f.drain();f.cache()->tvd_print(stdout,0);f.sp->flushL2();
  puts("PASS production_clean_write_promotion_dirty_victim_real_wb_reference");
}

static void matched_actual_lower(const char *base,const char *overlay) {
  unsigned long long packets[2], bytes[2];
  for (unsigned on=0;on<2;++on) {
    scenario f(base,overlay,false,on);
    auto a=f.push(0);f.await(a);auto b=f.push(128,true);f.await(b);
    auto read=f.push(0);f.await(read);assert(f.responses.at(read)==0);f.drain();
    packets[on]=f.cache()->tvd_actual_lower_read_packets();
    bytes[on]=f.cache()->tvd_actual_lower_read_bytes();
    f.sp->flushL2();
  }
  assert(packets[0]==2 && packets[1]==1 && bytes[0]==64 && bytes[1]==32);
  printf("MATCHED_LOWER|off_packets=%llu|on_packets=%llu|off_bytes=%llu|on_bytes=%llu\n",
      packets[0],packets[1],bytes[0],bytes[1]);
  puts("PASS production_matched_off_on_actual_lower_packets_bytes_NOT_DRAM_saved");
}

static void dirty_forward_order(const char *base,const char *overlay,bool atomic) {
  scenario f(base,overlay,true);
  auto w=f.push(0,true); f.await(w);const auto version=(unsigned long long)w+1;
  auto saved=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  auto other=f.push(128,true);f.await(other);
  auto read=f.push(0);f.await(read);
  auto dir=f.cache()->tvd_for_test();auto t=dir->find(0);
  assert(dir->live(t)&&dir->at(t.index).status==l2_cache::tvd_directory::WB_INFLIGHT);
  assert(f.cache()->ep_l2_wad_contains(0)&&f.responses.at(read)==version);
  assert(dir->stats().dirty_forward_responses==1);
  const auto before=f.gpu->gpu_sim_cycle;
  auto pending=atomic?f.push_atomic(0):f.push(0,true);
  for(unsigned n=0;n<8;++n) f.step();
  assert(!f.responses.count(pending)&&f.cache()->ep_l2_wad_contains(0));
  assert(f.cache()->ep_l2_payload_for_test().inspect(saved).versions[0]==version);
  f.await(pending);
  assert(f.gpu->gpu_sim_cycle>before+8);
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==saved);
  auto current=f.push(0);f.await(current);
  assert(f.responses.at(current)==(unsigned long long)pending+1);
  f.drain(); f.cache()->tvd_print(stdout,0);f.sp->flushL2();assert(!dir->occupied());
  puts(atomic?"PASS production_dirty_forward_before_wad_atomic_wait_then_same_handle_promote":
      "PASS production_dirty_forward_before_wad_write_wait_then_same_handle_promote");
}

static void dirty_partial(const char *base,const char *overlay) {
  scenario f(base,overlay,true); auto w=f.push(0,true);f.await(w);
  auto b=f.push(128,true);f.await(b);
  auto missing=f.push(32);for(unsigned n=0;n<8;++n) f.step();
  assert(!f.responses.count(missing)&&f.cache()->ep_l2_wad_contains(0));
  f.await(missing);assert(f.responses.at(missing)==0);
  assert(f.cache()->tvd_for_test()->stats().read_responses==0);
  f.drain();f.sp->flushL2();
  puts("PASS production_dirty_missing_sector_wait_completion_then_real_lower_fallback");
}

static void dirty_wad_full(const char *base,const char *overlay) {
  scenario f(base,overlay,true,true,true);
  for(unsigned i=0;i<142;++i) f.push(i*128,true);
  for(unsigned n=0;n<2000;++n) f.step();
  auto dir=f.cache()->tvd_for_test();
  assert(dir->occupied()==128&&dir->mandatory()==128&&f.sp->ep_l2_icntl2_occupancy()>0);
  auto h=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  for(unsigned n=0;n<16;++n) f.step();
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==h&&!f.cache()->ft_reservation_active());
  f.part->l2_char_release_dram_issue_hold();f.drain();
  assert(dir->epoch_ready() && f.cache()->ep_l2_payload_for_test().ownership_consistent());
  f.cache()->tvd_print(stdout,0);f.sp->flushL2();assert(!dir->occupied());
  puts("PASS production_WAD128_lower_full_no_partial_mutation_then_set_done_epoch_drain");
}

static void dirty_bank_pool_full(const char *base,const char *overlay) {
  scenario f(base,overlay,true,true,true,1024);
  for(unsigned i=0;i<1024;++i) f.push(i*128,true);
  for(unsigned n=0;n<15000 && f.responses.size()<1024;++n) f.step();
  assert(f.responses.size()==1024);
  for(unsigned i=1;i<=37;++i) f.push(i*1024*128,true);
  for(unsigned n=0;n<3000;++n) f.step();
  auto dir=f.cache()->tvd_for_test();
  assert(dir->mandatory()==36 && f.sp->ep_l2_icntl2_occupancy()==1);
  assert(!f.cache()->ft_reservation_active());
  const auto h=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  for(unsigned n=0;n<8;++n)f.step();
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==h);
  f.part->l2_char_release_dram_issue_hold();f.drain();
  assert(f.responses.size()==1061 && f.cache()->ep_l2_payload_for_test().ownership_consistent());
  f.sp->flushL2();assert(!dir->occupied());
  puts("PASS production_same_bank_pool_full_reclaims_only_completed_TVD_then_drains");
}

int main(int argc,char **argv) {
  assert(argc==3);
  clean_actual_frontend(argv[1],argv[2]);
  clean_partial_sector_fallback(argv[1],argv[2]);
  clean_write_promotes_dirty_resident(argv[1],argv[2]);
  matched_actual_lower(argv[1],argv[2]);
  dirty_forward_order(argv[1],argv[2],false);
  dirty_forward_order(argv[1],argv[2],true);
  dirty_partial(argv[1],argv[2]);
  dirty_wad_full(argv[1],argv[2]);
  dirty_bank_pool_full(argv[1],argv[2]);
}
