// AC2 feasibility witness ONLY. No compact production mechanism is installed.
// Reuse the unchanged AC actual-LDST fixture, including its explicit test seams.
#define main ac_parent_fixture_main
#include "dtc_l1_collect_wb_pipeline_probe.cc"
#undef main
#include <sys/wait.h>
#include <csignal>

static void binding() {
  Dl_info b{};
  assert(dladdr(dlsym(RTLD_DEFAULT, "_ZN9gpgpu_sim14gpu_print_statEy"), &b));
  printf("AC2_LIBRARY %s\n", b.dli_fname);
  char self[4096]{};
  assert(readlink("/proc/self/exe", self, sizeof(self)-1)>0);
  printf("AC2_ELF %s\n", self);
  std::ifstream maps("/proc/self/maps"); std::string line;
  while (std::getline(maps,line)) if (line.find(b.dli_fname)!=std::string::npos)
    printf("AC2_LIBRARY_MAP %s\n", line.c_str());
}
static void reject(unsigned capacity, unsigned demand) {
  fflush(nullptr);
  const pid_t child=fork(); assert(child>=0);
  if (!child) {
    dtc_l1::collect_wb_pipeline p; p.configure(capacity/4);
    p.claim(1,demand); _exit(0);
  }
  int status=0; assert(waitpid(child,&status,0)==child);
  assert(WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
  printf("AC2_NEGATIVE capacity_sectors=%u demand_sectors=%u signal=SIGABRT\n", capacity,demand);
}
static void resource() {
#ifndef DTC_COLLECT_WB_PROBE_BASELINE
  dtc_l1::collect_wb_pipeline p; p.configure(32); // test-local 4096B/bank
  p.claim(1,128);
  for (unsigned c=1;c<=32;++c) p.consumed(c,4);
  p.promote(); p.claim(2,128);
  for (unsigned c=33;c<=64;++c) p.consumed(c,4);
  assert(p.peak_buffered_sectors==256);
  p.attempted(65,false); assert(p.result_sectors==128 && p.collecting_sectors==128);
  p.attempted(66,true); p.retired(66,1,true); p.promote();
  p.attempted(67,true); p.retired(67,2,true);
  assert(!p.result_valid && !p.collecting_valid);
  reject(128,129); reject(256,257); reject(256,512);
  printf("AC2_RESOURCE banks=2 bytes=8192 exact128_fits=1 both_full_hold_denial=1 later_drain=1 production_compact=0\n");
#endif
  // Original service_entry cannot WB until all sectors are collected and an
  // additional cycle elapsed. A retained raw image grows monotonically until
  // WB. At capacity<demand, a bounded stall cannot change the state thereafter.
  for (auto pair : {std::pair<unsigned,unsigned>{128,129}, {256,257}, {256,512}}) {
    dtc_l1::service_entry s; dtc_l1::service_quota q(4);
    s.observe_ready(0,pair.second); unsigned retained=0;
    for (unsigned c=1;c<=pair.first/4+64;++c) {
      if (retained<pair.first) {
        const unsigned before=s.sectors_left;
        assert(!s.collect(c,q)); retained+=before-s.sectors_left;
      }
      assert(retained<=pair.first && s.sectors_left==pair.second-retained);
      assert(s.collect_done_cycle==UINT64_MAX);
    }
    assert(retained==pair.first && s.sectors_left>0);
    printf("AC2_STALL_INVARIANT capacity=%u demand=%u retained=%u remaining=%u done=UINT64_MAX no_WB_transition=1\n",pair.first,pair.second,retained,s.sectors_left);
  }
}
int main(int argc,const char **argv) {
  rlimit limit{0,0}; setrlimit(RLIMIT_CORE,&limit);
  binding();
  if (argc==2 && !strcmp(argv[1],"resource")) { resource(); return 0; }
  if (argc==2 && !strcmp(argv[1],"compact-option")) {
    // A STOP artifact must not silently accept an unimplemented production flag.
    gpgpu_context ctx;
    gpgpu_sim_config cfg(&ctx);
    auto parser=option_parser_create(); cfg.reg_options(parser);
    const char *opts[]={"AC2_NEGATIVE","-gpgpu_dtc_l1_collect_wb_compact","1"};
    option_parser_cmdline(parser,3,opts);
    return 99; // Unexpected acceptance is a test failure, not a compact mode.
  }
  if (argc!=6) return 64; // config pipeline sectors coalesced|pool K1
  const unsigned demand=std::stoul(argv[3]);
  const bool pool=!strcmp(argv[4],"pool");
  assert((pool && demand==2560) || (!pool && demand && demand<=512));
  fixture f(argv[1],argv[2],"3","1",argv[5],"1"); f.normal();
  f.setup_advance(); auto inst=f.instruction();
  if (pool) {
    // Structural full-pool stress: manually authored already-coalesced input,
    // deliberately NOT claimed to originate from a scalar ISA instruction.
    for (unsigned i=0;i<640;++i) {
      mem_access_byte_mask_t bytes; bytes.set();
      mem_access_sector_mask_t sectors; sectors.set();
      inst.m_accessq.emplace_back(GLOBAL_ACC_R,i*128,128,false,
          inst.get_active_mask(),bytes,sectors,&f.ctx);
    }
  } else {
    // Real production coalescer, source-supported <=8 addresses/lane. 4B
    // accesses at offset30 exercise its explicit cross-sector path. This is
    // a source API domain witness, not an executed ISA/real trace claim.
    const unsigned addresses=(demand+1)/2;
    assert(addresses<=32*MAX_ACCESSES_PER_INSN_PER_THREAD);
    unsigned index=0;
    for (unsigned lane=0;lane<32;++lane) {
      new_addr_type addr[MAX_ACCESSES_PER_INSN_PER_THREAD]{}; unsigned n=0;
      while (n<MAX_ACCESSES_PER_INSN_PER_THREAD && index<addresses) {
        const bool single=(demand%2 && index==addresses-1);
        addr[n++]=index*256+(single?0:30); ++index;
      }
      inst.set_addr(lane,addr,n);
    }
    inst.memory_coalescing_arch(false,GLOBAL_ACC_R);
  }
  const auto refs=f.unit->dtc_l1_io_line_references(inst);
  unsigned actual=0; for (auto r:refs) actual+=__builtin_popcount(r.sector_mask);
  assert(actual==demand && refs.size()<=640);
  printf("AC2_DEMAND sectors=%u raw_bytes=%u refs=%zu source=%s one_bank_fits=%u both_banks_fit=%u\n",
      actual,actual*32,refs.size(),pool?"MANUAL_POOL_STRUCTURAL":"PRODUCTION_COALESCER_API",actual<=128,actual<=256);
  register_set issue(1,"AC2_CAPACITY"); **issue.get_free()=inst;
  f.core->m_warp[0]->inc_inst_in_pipeline();
  f.unit->m_scoreboard->reserveRegisters(&inst); f.unit->issue(issue);
  f.loads=1; f.expected_sectors=actual;
  bool seen=false, invalidated=false; unsigned peak_refs=0;
  // Bound includes admission/finite four-packet return service per line and
  // all collection cycles. The640-line stress cannot finish within3000 edges.
  const unsigned max_steps=8*refs.size()+(actual+3)/4+32;
  for (unsigned step=0;step<max_steps;++step) {
    f.edge(false,true,3);
    peak_refs=std::max(peak_refs,unsigned(f.front().active_refs()));
    if (!f.unit->m_dtc_l1_oo_pib.empty()) seen=true;
    auto *entry=f.unit->dtc_l1_oo_find_entry(inst.get_uid());
    if (entry && entry->next_reference==entry->references.size()) {
      assert(entry->references.size()==refs.size());
      assert(f.front().active_refs()==refs.size());
      if (!invalidated && !strcmp(argv[5],"1")) {
        f.unit->invalidate(dtc_l1::invalidation_source::IDLE_CLUSTER);
        f.unit->invalidate(dtc_l1::invalidation_source::MEMBAR);
        invalidated=true;
      }
    }
    if (seen && f.unit->m_dtc_l1_oo_pib.empty()) break;
    assert(step+1<max_steps);
  }
  assert(seen && peak_refs==refs.size()); f.drain(3); f.normal(); f.edge(); f.normal();
  printf("AC2_RESULT status=PASS_PARENT_CAPACITY_WITNESS_ONLY sectors=%u peak_refs=%u lower=%llu compact_implemented=0 value_correctness=UNPROVEN\n",
      actual,peak_refs,(unsigned long long)f.unit->m_dtc_l1_oo_lower_responses);
  return 0;
}
