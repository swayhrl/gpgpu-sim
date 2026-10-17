#include "ideal_test_fixture.h"
#include "../../src/gpgpu-sim/scoreboard.h"

// Friend fixture exposes existing production objects, not alternate callbacks.
struct ep_l2_all_data_probe {
  static shader_core_ctx *core(gpgpu_sim &gpu) {return gpu.m_cluster[0]->m_core[0];}
  static void prepare(shader_core_ctx &c) {
    active_mask_t active;active.set(0);
    c.m_warp[0]->reset();c.m_warp[0]->init(0,0,0,active,0,0);
    c.get_gpu()->all_data_reference().new_shared_epoch(c.get_sid(),0);
  }
  static void issue(shader_core_ctx &c,memory_space_t space,bool store,unsigned reg,uint64_t address) {
    auto *inst=new warp_inst_t(c.get_config());
    inst->op=store?STORE_OP:LOAD_OP;inst->memory_op=store?memory_store:memory_load;
    inst->space=space;inst->data_size=4;inst->cache_op=CACHE_ALL;
    for (unsigned r=0;r<MAX_OUTPUT_VALUES;++r)inst->out[r]=0;
    for (unsigned r=0;r<MAX_REG_OPERANDS;++r)inst->arch_reg.dst[r]=-1;
    if (!store) {inst->out[0]=reg;inst->arch_reg.dst[0]=reg;}
    active_mask_t active;active.set(0);
    inst->issue(active,0,c.get_gpu()->gpu_sim_cycle,0,0,0);inst->set_addr(0,address);inst->generate_mem_accesses();
    c.m_warp[0]->inc_inst_in_pipeline();if (!store)c.m_scoreboard->reserveRegisters(inst);
    register_set source(1,"all_data_probe");source.move_in(inst);delete inst;
    assert(c.m_ldst_unit->can_issue(**source.get_ready()));c.m_ldst_unit->issue(source);
    assert(!source.has_ready());
  }
  static void cycle(shader_core_ctx &c,uint64_t now) {
    c.get_gpu()->gpu_sim_cycle=now;
    c.m_operand_collector.step();c.m_ldst_unit->cycle();
  }
  static bool pending(shader_core_ctx &c) {return c.m_scoreboard->pendingWrites(0);}
  static bool stores_done(shader_core_ctx &c) {return c.m_warp[0]->stores_done();}
  static unsigned in_pipeline(shader_core_ctx &c) {return c.m_warp[0]->num_inst_in_pipeline();}
};

int main(int argc,char **argv) {
  assert(argc==3);Fixture f(argv[1],argv[2]);auto *c=ep_l2_all_data_probe::core(*f.gpu);
  ep_l2_all_data_probe::prepare(*c);auto &s=f.gpu->all_data_reference();
  ep_l2_all_data_probe::issue(*c,global_space,false,1,0x100004000ULL);
  assert(ep_l2_all_data_probe::pending(*c) && s.requests_ready==0);
  ep_l2_all_data_probe::cycle(*c,0);assert(ep_l2_all_data_probe::pending(*c));
  ep_l2_all_data_probe::cycle(*c,1);assert(!ep_l2_all_data_probe::pending(*c));
  assert(s.drained() && f.gpu->gpu_sim_insn==1);
  ep_l2_all_data_probe::issue(*c,global_space,true,0,0x100004000ULL);
  assert(!ep_l2_all_data_probe::stores_done(*c));
  ep_l2_all_data_probe::cycle(*c,2);
  assert(ep_l2_all_data_probe::stores_done(*c) && s.store_reference_commits==1);
  for (auto space:{local_space,shared_space,const_space,tex_space,param_space_kernel,param_space_local}) {
    const uint64_t address=0xc4; //64B-block offset within a128B reference line.
    const uint64_t domain=space==shared_space?s.shared_domain(c->get_sid(),0):0;
    s.seed(domain,address,std::vector<uint8_t>(4,71));
    ep_l2_all_data_probe::issue(*c,space,false,1,address);
    const auto value=s.inspect(s.requests_issued).snapshot;
    assert(value.size()==4);
    for (unsigned b=0;b<4;++b)assert(value[b].address==address+b && value[b].value_known && value[b].value==71);
    ep_l2_all_data_probe::cycle(*c,f.gpu->gpu_sim_cycle+1);
    assert(s.drained() && !ep_l2_all_data_probe::pending(*c));
  }
  // Every response is ready at+1; a real SM register-bank conflict may consume
  // later. It is measured separately, without a memory FIFO or return quota.
  const auto before_ready=s.requests_ready;
  ep_l2_all_data_probe::issue(*c,global_space,false,1,0x2000);
  ep_l2_all_data_probe::issue(*c,global_space,false,17,0x3000);
  ep_l2_all_data_probe::cycle(*c,f.gpu->gpu_sim_cycle+1);
  assert(s.requests_ready==before_ready+2);
  for (unsigned n=0;n<8 && !s.drained();++n)ep_l2_all_data_probe::cycle(*c,f.gpu->gpu_sim_cycle+1);
  assert(s.drained() && ep_l2_all_data_probe::in_pipeline(*c)==0);
  assert(s.instruction_count==s.instructions_consumed && s.requests_ready==s.requests_consumed);
  assert(s.store_instructions==s.store_reference_commits);
  s.print(stdout);std::puts("ALL_DATA_NATIVE_LDST_SPACES_PENDING_REGISTER_STORE_COMPLETION PASS");
}
