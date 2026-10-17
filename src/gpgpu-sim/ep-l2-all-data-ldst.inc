// Included once in shader.cc. Full instruction issue and register writeback
// remain native; no reference completion enters ordinary LDST/response FIFOs.
bool ldst_unit::m_config_all_data() const {return m_config->ep_l2_mem_all_data;}

void ldst_unit::all_data_issue(register_set &source) {
  warp_inst_t *original=*(source.get_ready());
  assert(original && !original->empty());
  if (original->isatomic() || original->m_is_ldgsts) {
    fprintf(stderr,"EPL2MEMREF_UNSUPPORTED|atomic=%u|ldgsts=%u|pc=%llu\n",
            original->isatomic(),original->m_is_ldgsts,(unsigned long long)original->pc);
    abort();
  }
  const bool data=original->is_load() || original->is_store();
  assert(data || original->op==MEMORY_BARRIER_OP);
  const auto space=original->space.get_type();
  if (data && space!=global_space && space!=local_space && space!=param_space_local &&
      space!=shared_space && space!=const_space && space!=param_space_kernel && space!=tex_space) {
    fprintf(stderr,"EPL2MEMREF_UNSUPPORTED_SPACE|space=%u|pc=%llu\n",space,(unsigned long long)original->pc);
    abort();
  }
  assert(!original->is_store() || (space!=const_space && space!=param_space_kernel && space!=tex_space));
  const uint64_t now=m_gpu->gpu_sim_cycle+m_gpu->gpu_tot_sim_cycle;
  auto &service=m_gpu->all_data_reference();service.advance(now);
  if (original->is_store()) assert(service.native_store_declared(original->get_uid()));
  all_data_instruction group(*original);group.issued=now;group.inst.op_pipe=MEM__OP;
  struct fragment {uint64_t line,original;ep_l2_all_data::Service::Mask mask;};
  std::vector<fragment> fragments;
  uint64_t domain=0;
  if (data && space==shared_space && original->active_count()) {
    domain=service.shared_domain(m_sid,m_core->reference_cta_id(original->warp_id()));
    std::map<uint64_t,ep_l2_all_data::Service::Mask> lines;
    assert(original->data_size>0);
    for (unsigned lane=0;lane<original->warp_size();++lane) if (original->active(lane)) {
      const uint64_t start=original->get_addr(lane);
      assert(start<=UINT64_MAX-(original->data_size-1));
      for (unsigned b=0;b<original->data_size;++b) lines[(start+b)&~uint64_t(127)][(start+b)%128]=true;
    }
    for (const auto &line:lines) fragments.push_back({line.first,0,line.second});
  } else if (data) {
    warp_inst_t walk(*original);
    while (!walk.accessq_empty()) {
      const auto &access=walk.accessq_back();
      const uint64_t line=access.get_addr()&~uint64_t(127);
      auto bytes=access.get_byte_mask();ep_l2_all_data::Service::Mask mask{};
      assert(access.get_size()<=128 && access.get_addr()+access.get_size()-1<=line+127);
      assert(bytes.any() || !original->active_count());
      if (space==const_space || space==param_space_kernel || space==tex_space) {
        // These native byte masks are cache-block-relative (possibly64B),
        // unlike global/local's128B coalescer masks. Use the original active
        // lane addresses in this exact native transaction, retaining identity.
        for (unsigned lane=0;lane<original->warp_size();++lane)
          if (access.get_warp_mask().test(lane) && original->active(lane))
            for (unsigned b=0;b<original->data_size;++b) {
              const uint64_t address=original->get_addr(lane)+b;
              if (address>=access.get_addr() && address<access.get_addr()+access.get_size())
                mask[address-line]=true;
            }
      } else
        for (unsigned b=0;b<128;++b)
          mask[b]=bytes.test(b) && line+b>=access.get_addr() && line+b<access.get_addr()+access.get_size();
      fragments.push_back({line,access.reference_access_uid(),mask});walk.accessq_pop_back();
    }
    assert(!original->active_count() || !fragments.empty());
  }
  group.native_requests=original->accessq_count();
  assert(space==shared_space || fragments.size()==group.native_requests);
  group.reference_id=service.begin(original->get_uid(),fragments.size(),original->is_store(),data?space:unsigned(undefined_space));
  service.annotate(group.reference_id,m_core->reference_kernel_id(original->warp_id()),
      original->pc,original->get_streamID(),m_sid,original->warp_id(),original->dynamic_warp_id());
  for (const auto &f:fragments)
    group.requests.push_back(service.issue(group.reference_id,domain,f.line,f.mask,now,nullptr,f.original));
  const unsigned wid=original->warp_id();
  if (original->is_load() && space!=shared_space)
    for (unsigned r=0;r<MAX_OUTPUT_VALUES;++r) if (original->out[r]>0)
      m_pending_writes[wid][original->out[r]]+=group.native_requests;
  if (original->is_store() && space!=shared_space)
    for (unsigned n=0;n<group.native_requests;++n) m_core->inc_store_req(wid);
  m_core->mem_instruction_stats(*original);m_core->incmem_stat(m_config->warp_size,1);
  if (!m_all_data.emplace(group.reference_id,std::move(group)).second) abort();
  original->clear(); // Consumes exactly the original SM memory-issue slot.
}

void ldst_unit::all_data_cycle() {
  const uint64_t now=m_gpu->gpu_sim_cycle+m_gpu->gpu_tot_sim_cycle;
  auto &service=m_gpu->all_data_reference();service.advance(now);
  // No per-tick response quota or head-of-line FIFO. Independently try every
  // ready instruction; only real SM register-bank ports may defer consumption.
  for (auto it=m_all_data.begin();it!=m_all_data.end();) {
    auto &g=it->second;bool ready=now>g.issued;
    for (auto id:g.requests) ready &= service.ready(id);
    if (g.inst.op==MEMORY_BARRIER_OP)
      for (auto before=m_all_data.begin();before!=it;++before)
        if (before->second.inst.warp_id()==g.inst.warp_id()) ready=false;
    if (!ready) {++it;continue;}
    if (g.inst.is_load() && !m_operand_collector->writeback(g.inst)) {
      ++service.sm_register_wait_attempts;++it;continue;
    }
    const unsigned wid=g.inst.warp_id();
    for (auto id:g.requests) service.consume(id,now);
    if (g.inst.is_load()) {
      if (g.inst.space.get_type()!=shared_space)
        for (unsigned r=0;r<MAX_OUTPUT_VALUES;++r) if (g.inst.out[r]>0) {
          auto &pending=m_pending_writes[wid][g.inst.out[r]];
          assert(pending>=g.native_requests);pending-=g.native_requests;
          assert(pending==0);m_pending_writes[wid].erase(g.inst.out[r]);
        }
      m_scoreboard->releaseRegisters(&g.inst);
    }
    if (g.inst.is_store() && g.inst.space.get_type()!=shared_space)
      for (unsigned n=0;n<g.native_requests;++n) m_core->reference_store_ack(wid);
    m_core->dec_inst_in_pipeline(wid);m_core->warp_inst_complete(g.inst);
    m_last_inst_gpu_sim_cycle=m_gpu->gpu_sim_cycle;m_last_inst_gpu_tot_sim_cycle=m_gpu->gpu_tot_sim_cycle;
    if (g.inst.op==MEMORY_BARRIER_OP) service.barrier_complete(m_sid,wid);
    service.finish(g.reference_id);it=m_all_data.erase(it);
  }
}
