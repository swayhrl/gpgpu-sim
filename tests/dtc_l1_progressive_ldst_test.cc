// CPU component fixture for actual production LDST response/selection/collection/
// RF WB/ledger retirement/K1/normal-print paths. -fno-access-control is a setup
// seam only. No kernel, trace body, GPU cycle(), native run or runtime thread.
#include "gpgpu-sim/gpu-sim.h"
#include "gpgpu-sim/shader.h"
#include "gpgpu-sim/icnt_wrapper.h"
#include "gpgpu-sim/scoreboard.h"
#include "gpgpu_context.h"
#include "gpgpusim_entrypoint.h"
#include "option_parser.h"
#include <csignal>
#include <functional>
#include <ctime>
#include <cstring>
#include <string>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
static void rejects(const std::function<void()> &f) {
  auto pid=fork(); assert(pid>=0);
  if(!pid) { std::signal(SIGABRT,SIG_DFL); f(); _exit(0); }
  int status; assert(waitpid(pid,&status,0)==pid);
  assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
}
int main(int argc, const char **argv) {
  assert(argc==2 || argc==3);
  if(argc==2) {
    for(const char *mode: {"off", "on"}) {
      auto pid=fork(); assert(pid>=0);
      if(!pid) { execl(argv[0],argv[0],argv[1],mode,(char *)nullptr); _exit(127); }
      int status; assert(waitpid(pid,&status,0)==pid);
      assert(WIFEXITED(status) && WEXITSTATUS(status)==0);
    }
    int pipefd[2]; assert(pipe(pipefd)==0);
    auto pid=fork(); assert(pid>=0);
    if(!pid) {
      close(pipefd[0]); assert(dup2(pipefd[1],STDERR_FILENO)>=0); close(pipefd[1]);
      execl(argv[0],argv[0],argv[1],"invalid",(char *)nullptr); _exit(127);
    }
    close(pipefd[1]); std::string error; char chunk[1024]; ssize_t n;
    while((n=read(pipefd[0],chunk,sizeof(chunk)))>0) error.append(chunk,n);
    close(pipefd[0]);
    int status; assert(waitpid(pid,&status,0)==pid);
    assert((WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status)!=0)) &&
           error.find("DTC_SHORT_COLLECT_UNSUPPORTED_MODE_STOP")!=std::string::npos);
    std::puts("PASS unsupported_mode_rejected");
    std::puts("PASS same_binary_independent_OFF_ON_processes");
    return 0;
  }
  const bool invalid_mode=std::strcmp(argv[2],"invalid")==0;
  const bool short_on=std::strcmp(argv[2],"on")==0 || invalid_mode;
  assert(short_on || std::strcmp(argv[2],"off")==0);
  rlimit limit{0,0}; setrlimit(RLIMIT_CORE,&limit);
  gpgpu_context ctx; gpgpu_sim_config cfg(&ctx);
  auto parser=option_parser_create(); cfg.reg_options(parser); icnt_reg_options(parser);
  ctx.ptx_reg_options(parser); ctx.func_sim->ptx_opcocde_latency_options(parser);
  assert(!cfg.m_shader_config.dtc_l1_progressive_wakeup);
  option_parser_cfgfile(parser,argv[1]);
  const char *opts[]={"AD_CPU", "-gpgpu_n_clusters","1", "-gpgpu_n_cores_per_cluster","1",
    "-gpgpu_n_mem","1", "-gpgpu_n_sub_partition_per_mchannel","1",
    "-gpgpu_shader_core_pipeline","64:32", "-gpgpu_shader_cta","2",
    "-gpgpu_smem_latency","2", "-network_mode","2", "-gpgpu_dtc_l1_mode","3",
    "-gpgpu_dtc_l1_finite_service","1", "-gpgpu_dtc_l1_progressive_wakeup","1",
    "-gpgpu_dtc_l1_invalidate_on_source_event","1",
    "-gpgpu_l1_cache_write_ratio","0", "-gpgpu_gmem_skip_L1D","0",
    "-enable_ptx_file_line_stats","0", "-gpgpu_dtc_l1_debug_event_limit","0",
    "-power_simulation_enabled","0", "-visualizer_enabled","0",
    "-gpgpu_clock_domains","1410.0:1410.0:1410.0:850.0"};
  option_parser_cmdline(parser,sizeof(opts)/sizeof(opts[0]),opts);
  const char *short_option[]={"AD_CPU", "-gpgpu_dtc_l1_short_collect_first",
                              short_on ? "1" : "0"};
  option_parser_cmdline(parser,3,short_option);
  if(invalid_mode) {
    const char *invalid_options[]={"AD_CPU", "-gpgpu_dtc_l1_mode", "2",
                                   "-gpgpu_dtc_l1_progressive_wakeup", "0",
                                   "-gpgpu_dtc_l1_invalidate_on_source_event", "0"};
    option_parser_cmdline(parser,7,invalid_options);
  }
  cfg.init();
  assert(cfg.m_shader_config.dtc_l1_short_collect_first==short_on);
  ctx.the_gpgpusim->g_simulation_starttime=std::time(nullptr);
  auto *gpu=new exec_gpgpu_sim(cfg,&ctx); gpu->init();
  auto *core=gpu->get_simt_core_clusters()[0]->get_shader_cores()[0];
  auto *ld=core->m_ldst_unit; auto &f=*ld->m_dtc_l1_oo_frontend;
  auto *rf=dynamic_cast<opndcoll_rfu_t *>(ld->m_operand_collector); assert(rf);
  const auto *sc=gpu->getShaderCoreConfig();
  auto inst=[&](unsigned uid, bool reg=false) {
    warp_inst_t i(sc);
    // Real decoders initialize every output slot; inst_t's constructor only
    // zeros sizeof(unsigned) bytes. Synthetic instructions must do the rest.
    std::fill(std::begin(i.out),std::end(i.out),0U);
    active_mask_t active; active.set(0);
    i.issue(active,0,1,0,0,0); i.m_uid=uid; i.pc=0; i.op=LOAD_OP; i.op_pipe=MEM__OP;
    i.space=memory_space_t(global_space); i.cache_op=CACHE_ALL;
    if(reg) { i.out[0]=1; i.arch_reg.dst[0]=1; }
    return i;
  };
  auto fence=inst(90); fence.m_is_proxy_fence=true;
  assert(ld->dtc_l1_m4_observe_admit(fence,ldst_unit::dtc_l1_m4_operation::PROXY_FENCE));
  auto admit=[&](unsigned uid) {
    auto i=inst(uid,uid==100); assert(f.admit(uid));
    if(uid==100) { ld->m_pending_writes[0][1]=1; ld->m_scoreboard->reserveRegisters(&i); }
    const std::vector<dtc_l1::line_reference> refs{{0,1}};
    ld->dtc_l1_r4c0_record_issue(i,refs,1);
    // Source issue accounting for these explicitly injected CPU instructions.
    ++ld->m_dtc_l1_oo_completion_dependencies;
    ld->m_dtc_l1_oo_pib.push_back({i,refs,1});
    ld->dtc_l1_r4c0_record_pib(i,refs,"OO");
    return f.access(gpu->global_cycle(),uid,0).physical;
  };
  gpu->gpu_sim_cycle=10; f.advance_service(10);
  dtc_l1::physical_identity id{};
  for(unsigned uid=100;uid<109;++uid) id=admit(uid);
  auto root=[&](dtc_l1::physical_identity identity) {
    mem_access_t access(GLOBAL_ACC_R,0,128,false,&ctx);
    auto *r=new mem_fetch(access,(const warp_inst_t *)nullptr,0,8,0,0,0,&cfg.m_memory_config,gpu->global_cycle());
    assert(gpu->dtc_l1_try_acquire_lower_request());
    // This CPU fixture injects the already-created, already-issued root.
    ++ld->m_dtc_l1_oo_lower_created; ++ld->m_dtc_l1_oo_lower_issued;
    assert(ld->m_dtc_l1_oo_inflight.emplace(r->get_request_uid(),
        ldst_unit::dtc_l1_oo_inflight{identity,0,100,15,0}).second);
    return r;
  };
  auto sector=[&](mem_fetch *r,unsigned n,bool write=false) {
    mem_access_t access(write?GLOBAL_ACC_W:GLOBAL_ACC_R,n*32,32,write,&ctx);
    auto *s=new mem_fetch(access,(const warp_inst_t *)nullptr,0,8,0,0,0,&cfg.m_memory_config,gpu->global_cycle(),r);
    s->set_reply(); assert(ld->dtc_l1_oo_consume_response(s));
  };
  auto *r=root(id);
  sector(r,0); assert(f.wakeups()==0 && !f.entry_returns_complete_for(100));
  rejects([&]{sector(r,0);}); // duplicate sector reaches real consume assertion
  rejects([&]{sector(r,4);}); // out-of-line sector
  rejects([&]{sector(r,1,true);}); // write reply is not a load fill
  sector(r,2); sector(r,1);
  assert(!f.entry_ready_for(100) && !f.entry_returns_complete_for(100));
  sector(r,3); assert(f.entry_returns_complete_for(100) && f.wakeups()==0);
  f.advance_service(10); assert(!f.entry_ready_for(100));
  gpu->gpu_sim_cycle=11; f.advance_service(11);
  assert(f.entry_ready_for(100) && !f.entry_ready_for(104));
  uint64_t selected=0;
  assert(!ld->dtc_l1_oo_select_ready(&selected)); // same-warp fence still gates
  ld->dtc_l1_m4_observe_complete(fence);
  assert(ld->dtc_l1_oo_writeback_ready()); // fence sidecar, no data WB/Ref
  assert(!ld->dtc_l1_oo_writeback_ready()); // one retirement per cycle
  auto *first=ld->dtc_l1_oo_find_entry(100); first->next_reference=0;
  assert(ld->dtc_l1_oo_select_ready(&selected) && selected==101); // partial admission excluded
  first->next_reference=1;
  assert(ld->dtc_l1_oo_select_ready(&selected) && selected==100);
  assert(admit(109).id==id.id && !f.entry_ready_for(109)); // late merge consumes future quota
  f.advance_service(11); assert(f.wakeups()==4 && !f.entry_ready_for(109));
  // Hold ordinary pending-commit; K1 hides lookup NOW, preserves return/Ref.
  ld->m_L1D->m_extra_mf_fields.emplace(nullptr,baseline_cache::extra_mf_fields());
  ld->invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER);
  assert(!f.tag_visible(id) && f.ref_count(id)==10 && ld->m_L1D->m_invalidate_pending);
  ld->cycle(); // actual LDST sample observes own-ready at 11; no collection at ready edge
  assert(first->service.ready_cycle==11 && first->service.admitted_returns_cycle==11);
  assert(first->service.collect_done_cycle==UINT64_MAX && f.ref_count(id)==10);
  gpu->gpu_sim_cycle=12; rf->process_banks(); ld->cycle();
  assert(f.wakeups()==8 && !f.entry_ready_for(108) && !f.entry_ready_for(109));
  assert(first->service.collect_done_cycle==12 && f.ref_count(id)==10);
  gpu->gpu_sim_cycle=13; rf->process_banks();
  auto competing=inst(999,true); assert(rf->writeback(competing)); // real RF bank reservation
  ld->cycle();
  assert(f.wakeups()==10 && f.entry_ready_for(109));
  assert(ld->m_dtc_l1_oo_ready_but_wb_blocked_cycles==1 && f.ref_count(id)==10);
  assert(ld->m_pending_writes[0][1]==1 && ld->m_dtc_l1_r4c0_completion_trace.count(100)==1);
  assert(ld->m_L1D->m_extra_mf_fields.erase(nullptr)==1);
  gpu->gpu_sim_cycle=14; rf->process_banks(); ld->cycle();
  assert(!ld->dtc_l1_oo_find_entry(100) && f.ref_count(id)==9);
  assert(ld->m_pending_writes[0].count(1)==0 && ld->m_dtc_l1_r4c1_retired_history.at(100).closed_dependencies==1);
  for(unsigned cycle=15;!ld->m_dtc_l1_oo_pib.empty();++cycle) {
    assert(cycle<50); gpu->gpu_sim_cycle=cycle; rf->process_banks(); ld->cycle();
  }
  assert(!f.is_allocated(id) && f.active_refs()==0 && f.final_ref_reclaims()==1);
  assert(gpu->dtc_l1_lower_outstanding()==0 && ld->m_dtc_service_retired==10);
  assert(ld->m_dtc_progressive_collectable_entries==10);
  // Real LDST selector positive fixture: both ordinary OO loads have all
  // references admitted and actual finite frontend readiness. The older load
  // owns two line references / six sectors; the younger owns one sector.
  gpu->gpu_sim_cycle=40; f.advance_service(40);
  auto add_short_case=[&](unsigned uid, unsigned reg,
                          std::vector<dtc_l1::line_reference> refs) {
    auto i=inst(uid,true); i.out[0]=reg; i.arch_reg.dst[0]=reg;
    ld->m_pending_writes[0][reg]=refs.size();
    ld->m_scoreboard->reserveRegisters(&i);
    assert(f.admit(uid));
    ld->dtc_l1_r4c0_record_issue(i,refs,refs.size());
    ld->m_dtc_l1_oo_completion_dependencies+=refs.size();
    ld->m_dtc_l1_oo_pib.push_back({i,refs,refs.size()});
    ld->dtc_l1_r4c0_record_pib(i,refs,"OO");
    for(const auto &ref:refs) {
      const auto result=f.access(gpu->global_cycle(),uid,ref.line_address);
      assert(result.kind==dtc_l1::io_access_kind::NEW_MISS);
      f.complete(result.physical);
    }
  };
  add_short_case(201,1,{{8192,0xF},{8320,0x3}});
  add_short_case(202,2,{{8448,0x1}});
  while(!f.entry_ready_for(201) || !f.entry_ready_for(202)) {
    ++gpu->gpu_sim_cycle; assert(gpu->gpu_sim_cycle<100);
    f.advance_service(gpu->global_cycle());
  }
  auto *long_entry=ld->dtc_l1_oo_find_entry(201);
  auto *short_entry=ld->dtc_l1_oo_find_entry(202);
  assert(long_entry && short_entry);
  bool different=false;
  assert(ld->dtc_l1_oo_select_ready(&selected,&different) && selected==201 && !different);
  ld->cycle(); // production LDST samples both ready timestamps and return ages
  assert(long_entry->service.ready_cycle==gpu->global_cycle());
  assert(short_entry->service.ready_cycle==gpu->global_cycle());
  assert(long_entry->service.admitted_returns_cycle<=long_entry->service.ready_cycle);
  assert(short_entry->service.admitted_returns_cycle<=short_entry->service.ready_cycle);
  assert(long_entry->service.sectors_left==6 && short_entry->service.sectors_left==1);
  ++gpu->gpu_sim_cycle;
  assert(ld->dtc_l1_oo_select_ready(&selected,&different));
  assert(selected==(short_on ? 202U : 201U) && different==short_on);
  const auto before_grant=ld->m_dtc_short_collect_retire_granted;
  assert(!ld->dtc_l1_oo_writeback_ready()); // finite collect; no early WB
  assert((short_on ? short_entry->service.sectors_left==0
                   : long_entry->service.sectors_left==2) && f.active_refs()==3);
  assert(ld->m_dtc_short_collect_retire_granted==before_grant);
  ++gpu->gpu_sim_cycle; rf->process_banks();
  if(short_on) {
    auto competing_short=inst(999,true); competing_short.out[0]=2;
    competing_short.arch_reg.dst[0]=2;
    assert(rf->writeback(competing_short));
    assert(!ld->dtc_l1_oo_writeback_ready()); // RF backpressure retains Ref/ledger
    assert(ld->m_dtc_l1_oo_ready_but_wb_blocked_cycles>=2);
  } else {
    assert(!ld->dtc_l1_oo_writeback_ready()); // older load collects final two sectors
    assert(long_entry->service.sectors_left==0);
  }
  assert(f.active_refs()==3 && ld->m_dtc_short_collect_retire_granted==before_grant);
  ++gpu->gpu_sim_cycle; rf->process_banks();
  assert(ld->dtc_l1_oo_writeback_ready());
  assert(ld->m_dtc_short_collect_retire_granted==before_grant+(short_on ? 1 : 0));
  const unsigned first_retired=short_on ? 202 : 201;
  const unsigned later_retired=short_on ? 201 : 202;
  assert(!ld->dtc_l1_oo_find_entry(first_retired) && ld->dtc_l1_oo_find_entry(later_retired));
  assert(f.active_refs()==(short_on ? 2U : 1U));
  assert(ld->m_dtc_l1_r4c1_retired_history.at(first_retired).closed_dependencies==
         (short_on ? 1U : 2U));
  for(unsigned i=0;ld->dtc_l1_oo_find_entry(later_retired);++i) {
    assert(i<20); ++gpu->gpu_sim_cycle; rf->process_banks();
    ld->dtc_l1_oo_writeback_ready();
  }
  assert(ld->m_dtc_l1_r4c1_retired_history.at(201).closed_dependencies==2);
  assert(ld->m_dtc_l1_r4c1_retired_history.at(202).closed_dependencies==1);
  assert(f.active_refs()==0);
  // With both loads genuinely ready but the older one already eight cycles
  // old, ON must serve the older entry. This bounds repeated short arrivals.
  gpu->gpu_sim_cycle=60; f.advance_service(60);
  add_short_case(301,3,{{12288,0xF},{12416,0x3}});
  add_short_case(302,4,{{12544,0x1}});
  while(!f.entry_ready_for(301) || !f.entry_ready_for(302)) {
    ++gpu->gpu_sim_cycle; assert(gpu->gpu_sim_cycle<100);
    f.advance_service(gpu->global_cycle());
  }
  ld->cycle();
  auto *aged=ld->dtc_l1_oo_find_entry(301);
  assert(aged && aged->service.ready_cycle==gpu->global_cycle());
  gpu->gpu_sim_cycle+=dtc_l1::kShortCollectAgeLimit;
  assert(ld->dtc_l1_oo_select_ready(&selected,&different) && selected==301 && !different);
  const auto age_grants=ld->m_dtc_short_collect_retire_granted;
  for(unsigned i=0;!ld->m_dtc_l1_oo_pib.empty();++i) {
    assert(i<20); rf->process_banks(); ld->dtc_l1_oo_writeback_ready();
    ++gpu->gpu_sim_cycle;
  }
  assert(ld->m_dtc_short_collect_retire_granted==age_grants);
  assert(ld->m_dtc_l1_r4c1_retired_history.at(301).closed_dependencies==2);
  assert(ld->m_dtc_l1_r4c1_retired_history.at(302).closed_dependencies==1);
  assert(f.active_refs()==0);
  // Stale physical/generation via actual LDST complete after four legal sectors.
  rejects([&]{auto *stale=root(id); for(unsigned s=0;s<4;++s) sector(stale,s);});
  f.assert_shadow_refs();
  auto *stream=tmpfile(); assert(stream);
  auto print=[&] { gpu->shader_print_dtc_l1_stats(stream); fflush(stream); };
  print(); auto size=ftell(stream); auto counters=ld->m_dtc_progressive_returns_to_retire_sum;
  print(); assert(ftell(stream)==size*2 && counters==ld->m_dtc_progressive_returns_to_retire_sum);
  rewind(stream); char buffer[4096]; while(auto n=fread(buffer,1,sizeof(buffer),stream)) fwrite(buffer,1,n,stdout);
  fclose(stream);
  gpu->gpu_print_stat(0); // actual normal entrypoint, no GPU cycle()/kernel
  std::printf("AD_HOST_LAYOUT physical_line=%zu entry=%zu config=%zu service=%zu frontend=%zu ldst=%zu\n",
      sizeof(dtc_l1::oo_frontend::physical_line), sizeof(dtc_l1::oo_frontend::entry),
      sizeof(dtc_l1::config), sizeof(dtc_l1::service_entry), sizeof(dtc_l1::oo_frontend), sizeof(ldst_unit));
  std::puts("PASS real_LDST legal_4x32 sectors duplicate illegal stale_generation plus1 own_ready late_merge partial_admission collection RF_WB_backpressure ledger_retire K1 final_ref fence_sidecar normal_export_repeat_readonly kernels=0 gpu_cycle_calls=0 threads_started=0 payload_values=UNPROVEN");
}
