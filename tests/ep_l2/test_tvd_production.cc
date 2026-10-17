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
  unsigned long long audited_cycles = 0;
  unsigned max_physical_bank_grants = 0;
  explicit scenario(const char *base, const char *overlay, bool dirty=false, bool tvd=true, bool hold=false, unsigned sets=1, unsigned ft_mode=2, unsigned ways=1, unsigned wbuf=16, bool texture_only=false)
      : cfg(&context) {
    option_parser_t p=option_parser_create(); context.ptx_reg_options(p);
    context.func_sim->ptx_opcocde_latency_options(p); icnt_reg_options(p); cfg.reg_options(p);
    option_parser_cfgfile(p,base); option_parser_cfgfile(p,overlay);
    const std::string mode=std::to_string(ft_mode);
    const std::string entries=std::to_string(wbuf);
    const char *args[]={"tvd-test", "-gpgpu_ep_l2_ft_mode",mode.c_str(), "-gpgpu_ep_l2_wbuf_entries",entries.c_str(),
      "-gpgpu_dram_partition_queues","64:128:64:1", "-gpgpu_ep_l2_tvd_clean",tvd?"1":"0",
      "-gpgpu_ep_l2_tvd_dirty",dirty?"1":"0"};
    option_parser_cmdline(p,11,args);
    if(texture_only) {
      const char *routing[]={"tvd-test","-gpgpu_cache:dl2_texture_only","1"};
      option_parser_cmdline(p,3,routing);
    }
    std::string geometry="S:"+std::to_string(sets)+":128:"+std::to_string(ways)+",L:B:m:L:L,A:128:1,256:0,32";
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
  // Real host prefill hook, with actual memcpy offset accumulation. It may
  // initialize one declared sector only and must not perform a GPU bank op.
  void warm(new_addr_type address, unsigned repeats=1) {
    mem_access_sector_mask_t mask; mask.set((address>>5)&3);
    const auto physical_cycle=gpu->gpu_tot_sim_cycle+gpu->gpu_sim_cycle;
    unsigned long long before[4];
    for(unsigned b=0;b<4;++b) before[b]=sp->ft_bank_grants_for_test(b);
    for(unsigned i=0;i<repeats;++i) sp->force_l2_tag_update(address,physical_cycle,mask);
    assert(gpu->gpu_tot_sim_cycle+gpu->gpu_sim_cycle==physical_cycle);
    for(unsigned b=0;b<4;++b) assert(sp->ft_bank_grants_for_test(b)==before[b]);
  }
  bool try_warm(new_addr_type address) {
    mem_access_sector_mask_t mask;mask.set((address>>5)&3);
    const auto offset=sp->ft_memcpy_offset_for_test();
    unsigned long long before[4];
    for(unsigned b=0;b<4;++b)before[b]=sp->ft_bank_grants_for_test(b);
    const bool accepted=sp->ft_try_force_l2_tag_update(address,
        gpu->gpu_tot_sim_cycle+gpu->gpu_sim_cycle,mask);
    assert(sp->ft_memcpy_offset_for_test()==offset+(accepted?1:0));
    for(unsigned b=0;b<4;++b)assert(sp->ft_bank_grants_for_test(b)==before[b]);
    return accepted;
  }
  ~scenario() { delete alloc; delete part; delete stats; delete gpu; }
  l2_cache *cache() { return sp->tvd_cache_for_test(); }
  mem_fetch *make(new_addr_type address, bool write=false, bool texture=false) {
    active_mask_t active; active.set(0); mem_access_byte_mask_t bytes;
    const unsigned sector=(address>>5)&3;
    for(unsigned i=sector*32;i<sector*32+32;++i) bytes.set(i);
    mem_access_sector_mask_t sectors; sectors.set(sector);
    assert(!texture || !write);
    if(texture) {
      // istexture() reads the instruction's space, not the access-type enum.
      warp_inst_t instruction(gpu->getShaderCoreConfig());instruction.initiation_interval=1;
      instruction.space=memory_space_t(tex_space);
      instruction.issue(active,0,gpu->gpu_sim_cycle,0,0,0);
      mem_access_t access(TEXTURE_ACC_R,address,32,false,active,bytes,sectors,&context);
      auto mf=new mem_fetch(access,&instruction,0,8,0,0,0,gpu->getMemoryConfig(),gpu->gpu_sim_cycle);
      assert(mf->istexture());return mf;
    }
    return alloc->alloc(address,write?GLOBAL_ACC_W:GLOBAL_ACC_R,active,bytes,sectors,32,write,
        gpu->gpu_sim_cycle,0,0,0,NULL,0);
  }
  unsigned push_partial_write(new_addr_type address) {
    active_mask_t active; active.set(0); mem_access_byte_mask_t bytes;
    unsigned sector=(address>>5)&3;
    for(unsigned b=sector*32;b<sector*32+4;++b) bytes.set(b);
    mem_access_sector_mask_t mask; mask.set(sector);
    auto mf=alloc->alloc(address,GLOBAL_ACC_W,active,bytes,mask,32,true,gpu->gpu_sim_cycle,0,0,0,NULL,0);
    unsigned uid=mf->get_request_uid();sp->push(mf,gpu->gpu_sim_cycle);return uid;
  }
  unsigned push_atomic(new_addr_type address) {
    active_mask_t active; active.set(0); mem_access_byte_mask_t bytes;
    unsigned sector=(address>>5)&3;
    for(unsigned b=sector*32;b<sector*32+32;++b) bytes.set(b);
    mem_access_sector_mask_t mask; mask.set(sector);
    warp_inst_t instruction(gpu->getShaderCoreConfig()); instruction.initiation_interval=1;
    instruction.space = memory_space_t(global_space);
    instruction.issue(active,0,gpu->gpu_sim_cycle,0,0,0);
    instruction.add_callback(0,NULL,NULL,NULL,true);
    mem_access_t access(GLOBAL_ACC_R,address,32,false,active,bytes,mask,&context);
    auto mf=new mem_fetch(access,&instruction,0,8,0,0,0,gpu->getMemoryConfig(),gpu->gpu_sim_cycle);
    assert(mf->isatomic()); unsigned uid=mf->get_request_uid();
    sp->push(mf,gpu->gpu_sim_cycle); return uid;
  }
  unsigned push(new_addr_type address, bool write=false) {
    auto mf=make(address,write); unsigned uid=mf->get_request_uid();
    sp->push(mf,gpu->gpu_sim_cycle); return uid;
  }
  void collect() {
    while(auto mf=sp->pop()) {
      assert(!responses.count(mf->get_request_uid()));
      // Zero is an explicit sector-version label. A missing read payload must
      // not silently become zero and make warmed/clean read assertions pass.
      assert((mf->get_is_write() || mf->has_ft_wb_data()) &&
             "FT/TVD read response requires explicit sector-version backing");
      unsigned sector=(mf->get_addr()>>5)&3;
      responses[mf->get_request_uid()]=mf->has_ft_wb_data()?mf->get_ft_wb_data()[sector]:0;
      delete mf;
    }
  }
  void step(bool drain=true) {
    unsigned long long before[4];
    for(unsigned b=0;b<4;++b) before[b]=sp->ft_bank_grants_for_test(b);
    part->cache_cycle(gpu->gpu_sim_cycle); part->simple_dram_model_cycle();
    for(unsigned b=0;b<4;++b) {
      const auto grants=sp->ft_bank_grants_for_test(b)-before[b];
      assert(grants<=1 && "at most one grant per bank per physical GPU cycle");
      max_physical_bank_grants=std::max(max_physical_bank_grants,(unsigned)grants);
    }
    ++audited_cycles;
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
  void clock_report() {
    assert(audited_cycles && max_physical_bank_grants==1);
    printf("TVD_REPAIR_CLOCK|cycles=%llu|max_grants_per_physical_bank_cycle=%u\n",
        audited_cycles,max_physical_bank_grants);
  }
};

// New r14 gates. They are deliberately separate from all frozen selectors.
// Source preparation only until this exact revision is compiled and executed.
static void texture_bypass_does_not_consume_tvd(const char *base,const char *overlay) {
  scenario f(base,overlay,false,true,false,1,2,1,16,true);
  auto texture_push=[&](new_addr_type address) {
    auto mf=f.make(address,false,true);const auto uid=mf->get_request_uid();
    f.sp->push(mf,f.gpu->gpu_sim_cycle);return uid;
  };
  auto a=texture_push(0);f.await(a);
  auto b=texture_push(128);f.await(b);f.drain();
  auto dir=f.cache()->tvd_for_test();const auto t=dir->find(0);
  assert(dir->live(t));const auto h=dir->at(t.index).payload;
  const auto lower=f.sp->ep_l2_lower_read_issue_count();
  const auto served=dir->stats().read_responses;
  const auto uid=f.push(0);bool returned=false;
  // A bypass return has no cache payload identity/version contract. Inspect
  // its real queue response here; do not weaken collect()'s cache-read guard
  // or claim a numerical data comparison for dataless lower memory.
  for(unsigned n=0;n<10000 && !returned;++n) {
    f.step(false);
    if(auto mf=f.sp->pop()) {
      assert(mf->get_request_uid()==uid && !returned);
      assert(!mf->has_ep_l2_payload_identity());
      delete mf;returned=true;
    }
  }
  assert(returned && f.sp->ep_l2_lower_read_issue_count()==lower+1);
  assert(dir->stats().read_responses==served && dir->live(t) && dir->at(t.index).payload==h);
  auto again=texture_push(0);f.await(again);
  assert(f.responses.at(again)==0 && dir->stats().read_responses==served+1);
  f.drain();f.sp->flushL2();assert(!dir->occupied());
  puts("PASS r14_texture_only_global_bypass_exact_once_then_texture_TVD_service");
}

static void clean_retention_physical_cost(const char *base,const char *overlay) {
  scenario f(base,overlay);f.warm(0,4096);
  auto b=f.push(128);f.await(b);f.drain();
  auto dir=f.cache()->tvd_for_test();assert(dir->occupied()==1 && dir->live(dir->find(0)));
  dir->advance(f.gpu->gpu_sim_cycle+f.gpu->gpu_tot_sim_cycle);
  const auto clean=dir->stats().clean_retention_slot_cycles;
  const auto dirty=dir->stats().tvd_extra_retention_slot_cycles;
  const auto grants=f.cache()->ep_l2_payload_for_test().bank_grants();
  for(unsigned n=0;n<7;++n)f.step();
  dir->advance(f.gpu->gpu_sim_cycle+f.gpu->gpu_tot_sim_cycle);
  assert(dir->stats().clean_retention_slot_cycles==clean+7);
  assert(dir->stats().tvd_extra_retention_slot_cycles==dirty);
  assert(f.cache()->ep_l2_payload_for_test().bank_grants()==grants);
  // Exact host alias retirement contributes no fabricated offset cycles.
  const auto before=dir->stats().clean_retention_slot_cycles;
  assert(f.try_warm(0));assert(!dir->live(dir->find(0)));
  assert(dir->stats().clean_retention_slot_cycles==before);
  f.drain();f.sp->flushL2();assert(!dir->occupied());
  puts("PASS r14_clean_retention_physical_slot_cycles_no_memcpy_offset_cost");
}

static void full_shared_directory_dirty_promotion(const char *base,const char *overlay) {
  scenario f(base,overlay,true);
  // Unlike the existing all-mandatory WAD-full gate, start with all 128
  // shared address entries holding clean victims, then promote one while
  // replacing a dirty resident. No transient 129th address/second owner.
  for(unsigned line=0;line<128;++line) {
    auto uid=f.push(new_addr_type(line)*128);f.await(uid);
  }
  auto dirty=f.push(new_addr_type(128)*128,true);f.await(dirty);
  auto dir=f.cache()->tvd_for_test();assert(dir->occupied()==128 && !dir->mandatory());
  auto source=dir->find(0);assert(dir->live(source));
  const auto promoted=dir->at(source.index).payload;
  const auto victim=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  const auto pools=f.cache()->ep_l2_payload_for_test().physical_slots();
  auto write=f.push(0,true);f.await(write);
  const auto wb=dir->find(new_addr_type(128)*128);
  assert(dir->occupied()==128 && dir->mandatory()==1 && dir->live(wb));
  assert(wb.index==source.index && wb.incarnation!=source.incarnation && !dir->live(source));
  assert(dir->at(wb.index).payload==victim && !dir->live(dir->find(0)));
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==promoted);
  assert(f.cache()->ep_l2_payload_for_test().physical_slots()==pools && pools==1168);
  assert(f.cache()->ep_l2_payload_for_test().ownership_consistent());
  auto read=f.push(0);f.await(read);assert(f.responses.at(read)==(unsigned long long)write+1);
  f.drain();assert(dir->occupied()==128 && !dir->mandatory());
  assert(dir->at(wb.index).status==l2_cache::tvd_directory::WB_COMPLETE);
  f.sp->flushL2();assert(!dir->occupied());
  puts("PASS r14_shared128_clean_to_mandatory_dirty_promotion_same_slot_same_payload");
}

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

static void clean_write_promotes_dirty_resident(const char *base,const char *overlay, bool atomic=false) {
  scenario f(base,overlay);auto a=f.push(0);f.await(a);
  auto old=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  auto b=f.push(128,true);f.await(b);
  auto w=atomic?f.push_atomic(0):f.push(0,true);f.await(w);
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==old);
  assert(f.cache()->tvd_for_test()->stats().promotions==1);
  assert(!f.cache()->tvd_for_test()->live(f.cache()->tvd_for_test()->find(0)));
  auto read=f.push(0);f.await(read);assert(f.responses.at(read)==(unsigned long long)w+1);
  f.drain();f.cache()->tvd_print(stdout,0);f.sp->flushL2();
  puts(atomic?"PASS production_clean_atomic_promotion_dirty_victim_real_wb_reference":
      "PASS production_clean_write_promotion_dirty_victim_real_wb_reference");
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

static void dirty_forward_order(const char *base,const char *overlay,bool atomic, unsigned warm_repeats=0) {
  scenario f(base,overlay,true);
  if(warm_repeats) f.warm(0,warm_repeats);
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
  if(warm_repeats) f.clock_report();
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

static void dirty_wad_full(const char *base,const char *overlay, unsigned warm_repeats=0) {
  scenario f(base,overlay,true,true,true);
  if(warm_repeats) f.warm(0,warm_repeats);
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
  if(warm_repeats) f.clock_report();
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

static void partial_dirty_promotion(const char *base,const char *overlay) {
  scenario f(base,overlay,true); auto a=f.push(0);f.await(a);
  auto handle=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  auto partial=f.push_partial_write(128);f.await(partial);
  auto b=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  assert(f.cache()->ep_l2_payload_for_test().inspect(b).ready_sector_mask.none());
  auto write=f.push(0,true);f.await(write);
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==handle);
  auto t=f.cache()->tvd_for_test()->find(128);
  if (f.cache()->tvd_for_test()->live(t))
    assert(!f.cache()->tvd_for_test()->at(t.index).payload.valid());
  auto read=f.push(0);f.await(read);assert(f.responses.at(read)==(unsigned long long)write+1);
  f.drain();f.sp->flushL2();
  puts("PASS production_partial_dirty_victim_WAD_only_no_false_sector_forward_or_promotion_deadlock");
}

static void primary_tag_geometry(const char *base,const char *overlay) {
  scenario f(base,overlay,false,true,false,64,2,16);
  auto first=f.push(0);f.await(first);
  ep_l2::PayloadHandle original;
  for(unsigned i=0;i<1024;++i) {
    auto h=f.cache()->ep_l2_tag_payload_handle_for_test(i);
    if(h.valid() && f.cache()->ep_l2_payload_for_test().inspect(h).owner==0) original=h;
  }
  assert(original.valid());
  for(unsigned i=1;i<=16;++i) { auto uid=f.push(i*64*128);f.await(uid); }
  auto dir=f.cache()->tvd_for_test();assert(dir->live(dir->find(0)));
  auto read=f.push(0);f.await(read);assert(f.responses.at(read)==0);
  auto write=f.push(0,true);f.await(write);
  assert(!dir->live(dir->find(0)) && dir->stats().promotions==1);
  unsigned matches=0;
  for(unsigned i=0;i<1024;++i) {
    auto h=f.cache()->ep_l2_tag_payload_handle_for_test(i);
    if(!h.valid())continue;
    assert(h.payload_id%4==i%4);
    if(f.cache()->ep_l2_payload_for_test().inspect(h).owner==0) {assert(h==original);++matches;}
  }
  assert(matches==1);
  auto current=f.push(0);f.await(current);assert(f.responses.at(current)==(unsigned long long)write+1);
  f.drain();f.sp->flushL2();
  puts("PASS production_primary_64sets_16ways_bank_class_preserved_same_handle_promotion");
}

// New review_r3 producer gates. These are source candidates until a repaired
// binary can be admitted/built. Do not inherit the earlier 12-group PASS.
static void warm_sectors_then_tvd(const char *base,const char *overlay,unsigned c) {
  scenario f(base,overlay,true,true,false,1,2,1,c);
  f.warm(0,4096);
  const auto warmed=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  f.warm(32);f.warm(0);
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==warmed);
  const auto &s=f.cache()->ep_l2_payload_for_test().inspect(warmed);
  assert(s.ready_sector_mask.to_ulong()==3 && s.versions[0]==0 && s.versions[1]==0);
  auto hit=f.push(32);f.await(hit);assert(f.responses.at(hit)==0);
  assert(f.cache()->tvd_actual_lower_read_packets()==0);
  auto b=f.push(128,true);f.await(b);
  auto dir=f.cache()->tvd_for_test();auto a=dir->find(0);
  assert(dir->live(a) && dir->at(a.index).payload==warmed && dir->at(a.index).valid_sectors==3);
  auto tvd=f.push(32);f.await(tvd);assert(f.responses.at(tvd)==0);
  auto write=f.push(0,true);f.await(write);
  assert(f.cache()->ep_l2_tag_payload_handle_for_test(0)==warmed);
  auto current=f.push(0);f.await(current);assert(f.responses.at(current)==(unsigned long long)write+1);
  f.drain();f.sp->flushL2();assert(!dir->occupied());f.clock_report();
  printf("PASS repair_warm_repeat_partial_sector_TVD_promotion_WB_flush_C%u\n",c);
}

static void warm_replaces_clean_and_tvd_alias(const char *base,const char *overlay) {
  scenario f(base,overlay,true);
  f.warm(0);const auto old=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  f.warm(128);
  auto resident=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  assert(resident!=old && !f.cache()->ep_l2_payload_for_test().handle_live(old));
  assert(!f.cache()->tvd_for_test()->live(f.cache()->tvd_for_test()->find(0)));
  auto hit=f.push(128);f.await(hit);assert(f.responses.at(hit)==0);
  auto next=f.push(256);f.await(next);f.drain();
  auto dir=f.cache()->tvd_for_test();auto alias=dir->find(128);
  assert(dir->live(alias) && dir->at(alias.index).payload==resident);
  f.warm(128); // idle host overwrite must drop the older detached authority
  assert(!dir->live(dir->find(128)) && !f.cache()->ep_l2_payload_for_test().handle_live(resident));
  auto again=f.push(128);f.await(again);assert(f.responses.at(again)==0);
  f.drain();f.sp->invalidateL2();assert(!dir->occupied());
  f.warm(512);auto epoch=f.push(512);f.await(epoch);assert(f.responses.at(epoch)==0);
  f.drain();f.sp->invalidateL2();f.clock_report();
  puts("PASS repair_host_clean_victim_TVD_alias_reinit_epoch_identity");
}

static void warm_missing_sector(const char *base,const char *overlay) {
  scenario f(base,overlay,true);f.warm(0,4096);
  auto other=f.push(128);f.await(other);
  auto dir=f.cache()->tvd_for_test();assert(dir->live(dir->find(0)));
  const auto lower=f.cache()->tvd_actual_lower_read_packets();
  auto missing=f.push(32);f.await(missing);
  assert(f.responses.at(missing)==0 && !dir->live(dir->find(0)));
  assert(f.cache()->tvd_actual_lower_read_packets()==lower+1);
  assert(dir->stats().read_responses==0);
  f.drain();f.sp->invalidateL2();f.clock_report();
  puts("PASS repair_warm_partial_sector_TVD_missing_real_lower_fallback");
}

static void warm_primary_geometry(const char *base,const char *overlay) {
  scenario f(base,overlay,true,true,false,64,2,16);f.warm(0,4096);
  for(unsigned i=1;i<=16;++i)f.warm(i*64*128);
  const unsigned address=16*64*128;
  ep_l2::PayloadHandle last;
  for(unsigned i=0;i<1024;++i) {
    auto h=f.cache()->ep_l2_tag_payload_handle_for_test(i);
    if(!h.valid())continue;
    const auto &slot=f.cache()->ep_l2_payload_for_test().inspect(h);
    assert(h.payload_id%4==i%4 && slot.allocation_role==ep_l2_payload_store::RESIDENT);
    if(slot.owner==address)last=h;
  }
  assert(last.valid());f.warm(address+32);
  assert(f.cache()->ep_l2_payload_for_test().inspect(last).ready_sector_mask.to_ulong()==3);
  auto read=f.push(address+32);f.await(read);assert(f.responses.at(read)==0);
  assert(f.cache()->tvd_actual_lower_read_packets()==0);
  f.drain();f.sp->invalidateL2();f.clock_report();
  puts("PASS repair_warm_primary64x16_actual_chosen_tag_and_bank_identity");
}

static void candidate62_try_prefill_tvd(const char *base,const char *overlay) {
  scenario f(base,overlay,true);
  const new_addr_type a=0x100000000ULL;
  f.warm(a,4096);
  auto original=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  auto w=f.push(a+128,true);
  for(unsigned n=0;n<10000 && !f.sp->ep_l2_l2icnt_occupancy();++n)f.step(false);
  assert(f.sp->ep_l2_l2icnt_occupancy()==1);
  auto dir=f.cache()->tvd_for_test();auto alias=dir->find(a);
  assert(dir->live(alias) && !dir->live(dir->find(a&0xffffffffULL)));
  const auto before=dir->at(alias.index);
  const auto slot=f.cache()->ep_l2_payload_for_test().inspect(original);
  auto resident=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  assert(!f.try_warm(a)); // real held response prevents mutation
  assert(dir->live(alias) && dir->at(alias.index).age==before.age &&
         dir->at(alias.index).payload==original);
  assert(f.cache()->ep_l2_payload_for_test().inspect(original).versions==slot.versions &&
         f.cache()->ep_l2_tag_payload_handle_for_test(0)==resident);
  f.collect();assert(f.responses.count(w));f.drain();
  assert(!f.try_warm(a)); // drained but selected resident is dirty
  assert(dir->live(alias) && dir->at(alias.index).age==before.age &&
         f.cache()->ep_l2_tag_payload_handle_for_test(0)==resident);
  auto read=f.push(a+256);f.await(read);f.drain();
  auto unrelated=dir->find(a+128);assert(dir->live(unrelated));
  assert(f.try_warm(a)); // exact optional alias only, after real dirty WB
  assert(!dir->live(alias) && !dir->live(dir->find(a)) && dir->live(unrelated));
  assert(!f.cache()->ep_l2_payload_for_test().handle_live(original));
  auto h=f.cache()->ep_l2_tag_payload_handle_for_test(0);
  assert(f.cache()->ep_l2_payload_for_test().inspect(h).owner==a);
  auto final=f.push(a);f.await(final);assert(f.responses.at(final)==0);
  f.drain();f.sp->flushL2();assert(!dir->occupied());f.clock_report();
  puts("PASS candidate62_try_prefill_TVD_refusal_atomicity_full_address_exact_alias");
}

int main(int argc,char **argv) {
  assert(argc==3 || argc==4);
  if (argc==4) {
    std::string negative=argv[3];
    if (negative=="--r14-integration-gates") {
      texture_bypass_does_not_consume_tvd(argv[1],argv[2]);
      clean_retention_physical_cost(argv[1],argv[2]);
      full_shared_directory_dirty_promotion(argv[1],argv[2]);return 0;
    }
    if (negative=="--candidate62-gates") {
      candidate62_try_prefill_tvd(argv[1],argv[2]);return 0;
    }
    if (negative=="--repair-gates") {
      warm_sectors_then_tvd(argv[1],argv[2],16);
      warm_sectors_then_tvd(argv[1],argv[2],8);
      warm_replaces_clean_and_tvd_alias(argv[1],argv[2]);
      warm_missing_sector(argv[1],argv[2]);
      warm_primary_geometry(argv[1],argv[2]);
      dirty_forward_order(argv[1],argv[2],false,4096);
      puts("PASS repair_offset4096_dirty_forward_write_wait_physical_bank");
      dirty_forward_order(argv[1],argv[2],true,4096);
      puts("PASS repair_offset4096_dirty_forward_atomic_wait_physical_bank");
      dirty_wad_full(argv[1],argv[2],4096);
      puts("PASS repair_offset4096_WAD128_lower_accept_set_done_flush_physical_bank");
      return 0;
    }
    if (negative=="--reject_warm_dirty_victim" || negative=="--reject_warm_dirty_owner") {
      scenario f(argv[1],argv[2],true);
      auto a=f.push(0,true);f.await(a);f.drain();
      f.warm(negative=="--reject_warm_dirty_owner"?0:128);return 77;
    }
    if (negative=="--reject_warm_active_request") {
      scenario f(argv[1],argv[2],true);f.push(0);f.warm(128);return 77;
    }
    if (negative=="--reject_warm_mandatory_wb") {
      scenario f(argv[1],argv[2],true,true,true);
      auto a=f.push(0,true);f.await(a);auto b=f.push(128,true);f.await(b);
      assert(f.sp->ep_l2_wad_occupancy());f.warm(256);return 77;
    }
    if (negative=="--reject_static_tvd") { scenario illegal(argv[1],argv[2],true,true,false,1,1); return 77; }
    if (negative=="--reject_old_tvd") { scenario illegal(argv[1],argv[2],true,true,false,1,0); return 77; }
    if (negative=="--reject_live_epoch") {
      scenario f(argv[1],argv[2],true,true,true);
      auto a=f.push(0,true);f.await(a);auto b=f.push(128,true);f.await(b);
      assert(f.cache()->ep_l2_wad_occupancy());f.sp->flushL2();return 77;
    }
    return 78;
  }
  clean_actual_frontend(argv[1],argv[2]);
  clean_partial_sector_fallback(argv[1],argv[2]);
  clean_write_promotes_dirty_resident(argv[1],argv[2]);
  clean_write_promotes_dirty_resident(argv[1],argv[2],true);
  matched_actual_lower(argv[1],argv[2]);
  dirty_forward_order(argv[1],argv[2],false);
  dirty_forward_order(argv[1],argv[2],true);
  dirty_partial(argv[1],argv[2]);
  dirty_wad_full(argv[1],argv[2]);
  dirty_bank_pool_full(argv[1],argv[2]);
  partial_dirty_promotion(argv[1],argv[2]);
  primary_tag_geometry(argv[1],argv[2]);
}
