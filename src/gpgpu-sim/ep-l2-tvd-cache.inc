// Mechanism-study B: port of historical TVD consumers; public hook patch is
// reviewed/applied by A. Compile/production qualification is separate.
unsigned long long l2_cache::m_gpu_cycle_for_tvd() const {
  return ft_physical_cycle();
}

bool l2_cache::tvd_install_victim(new_addr_type address, ep_l2::PayloadHandle h,
    unsigned valid, unsigned dirty, bool wb, unsigned long long now,
    tvd_directory::ticket *installed) {
  auto plan = m_tvd->preview_install(address, wb, true);
  if (plan.status != tvd_directory::OK) return false;
  tvd_directory::ticket ticket;
  if (m_tvd->commit_install(plan, address, h, valid, dirty, 0, wb, now, ticket) != tvd_directory::OK)
    return false;
  ++m_tvd_native_installs;
  if (installed) *installed = ticket;
  if (plan.displaced.valid()) {
    assert(m_ep_l2_payload.inspect(plan.displaced).pins == 0);
    m_ep_l2_payload.release(plan.displaced);
  }
  return true;
}

bool l2_cache::tvd_cached_wad_contains(mem_fetch *mf, new_addr_type address) const {
  assert(m_tvd);
  if (m_tvd_lookup == mf && m_tvd_lookup_done) {
    const auto t = m_tvd_lookup_ticket;
    return m_tvd->live(t) && m_tvd->at(t.index).line == address &&
        m_tvd->at(t.index).mandatory();
  }
  // A normal resident hit/sector miss cannot alias a mandatory WB owner:
  // miss lookup blocks on WAD; promotion waits for completion; host prefill
  // requires quiescence. No new functional address lookup is issued here.
  // Before the query pipeline this is UNKNOWN to an observational preview;
  // it is not an admission proof. ft_access below refuses an unqueried MISS.
  return false;
}

bool l2_cache::tvd_reclaim_one(unsigned bank, unsigned long long now) {
  // Mandatory space first reclaims optional clean, then completed WB data.
  // Pinned reads and pending/inflight WB are never eligible victims.
  for (auto state : {tvd_directory::CLEAN, tvd_directory::WB_COMPLETE}) {
    unsigned best=m_tvd->capacity();
    for (unsigned i=0;i<m_tvd->capacity();++i) {
      const auto &e=m_tvd->at(i);
      if (e.status!=state || !e.reclaimable() || !e.payload.valid() ||
          (bank!=(unsigned)-1 && e.payload.payload_id%4!=bank)) continue;
      if (best==m_tvd->capacity() || e.age<m_tvd->at(best).age) best=i;
    }
    if (best==m_tvd->capacity()) continue;
    const auto token=tvd_directory::ticket(best,m_tvd->at(best).incarnation);
    ep_l2::PayloadHandle h;
    assert(m_tvd->reclaim(token,h,now)==tvd_directory::OK);
    assert(m_ep_l2_payload.inspect(h).pins==0);
    m_ep_l2_payload.release(h);return true;
  }
  return false;
}

l2_cache::tvd_frontend_result l2_cache::tvd_frontend(mem_fetch *mf,
    unsigned long long now, bool response_space) {
  if (!m_tvd) return TVD_NORMAL;
  unsigned index = (unsigned)-1;
  const auto address = m_config.block_addr(mf->get_addr());
  const auto tag = m_tag_array->probe(address, index, mf, mf->is_write(), true);
  if (tag == HIT || tag == SECTOR_MISS || tag == HIT_RESERVED) return TVD_NORMAL;
  if (m_tvd_lookup != mf) {
    assert(!m_tvd_read);
    m_tvd_lookup = mf; m_tvd_lookup_cycle = now;
    m_tvd_lookup_done = false;
    ++m_tvd_lookup_wait; return TVD_WAIT;
  }
  if (now <= m_tvd_lookup_cycle) return TVD_WAIT;
  if (!m_tvd_lookup_done) {
    if (m_tvd_query_cycle == now) return TVD_WAIT;
    m_tvd_query_cycle = now; ++m_tvd_lookup_queries;
    m_tvd_lookup_ticket = m_tvd->find(address);
    m_tvd_lookup_done = true;
  }
  const auto ticket = m_tvd_lookup_ticket;
  if (!m_tvd->live(ticket)) return TVD_NORMAL;
  const auto &entry = m_tvd->at(ticket.index);
  // Dirty pending/inflight reads use a distinct client+UID+generation key
  // in the existing bank scoreboard. The public r2 adapter delays WB->TVD
  // role transitions while the reader is pinned. CLEAN_ONLY WAD records have
  // no retained handle and still return WAIT_WRITEBACK below.
  auto op = mf->isatomic() ? tvd_directory::ATOMIC_OP :
            mf->get_is_write() ? tvd_directory::WRITE_OP : tvd_directory::READ_OP;
  auto observation = m_tvd->preview_ticket(ticket, mf->get_access_sector_mask().to_ulong(), op);
  if (observation.status == tvd_directory::WAIT_WRITEBACK ||
      observation.status == tvd_directory::PINNED) { ++m_tvd_wb_wait; return TVD_WAIT; }
  if (op != tvd_directory::READ_OP) return TVD_NORMAL; // ft_access commits promotion
  if (observation.status == tvd_directory::MISSING_SECTOR) {
    ep_l2::PayloadHandle released;
    if (m_tvd->reclaim(ticket, released, now) == tvd_directory::PINNED) return TVD_WAIT;
    m_ep_l2_payload.release(released); return TVD_NORMAL;
  }
  if (observation.status != tvd_directory::READ_READY) return TVD_NORMAL;
  if (!response_space) { ++m_tvd_resp_wait; return TVD_WAIT; }
  if (!m_tvd_read) {
    assert(m_ep_l2_payload.handle_owner_matches(observation.payload, address));
    m_tvd_read.reset(new tvd_directory::read_lease());
    assert(m_tvd->begin_read(observation, mf->get_access_sector_mask().to_ulong(), *m_tvd_read) == tvd_directory::OK);
    m_ep_l2_payload.pin(observation.payload);
  }
  const auto h = m_tvd_read->handle();
  if (m_ep_l2_payload.request(h, false, now, (unsigned long long)-1,
      ep_l2_payload_store::TVD_READ, ep_l2_payload_store::FRONTEND, mf->get_request_uid()) != ep_l2_payload_store::GRANTED) { ++m_tvd_bank_wait; return TVD_WAIT; }
  mf->set_ep_l2_payload_identity(h.payload_id, h.generation);
  mf->set_ft_wb_data(m_ep_l2_payload.inspect(h).versions);
  return TVD_RESPONSE;
}

void l2_cache::tvd_response_commit(mem_fetch *mf, unsigned long long now) {
  assert(m_tvd_read && mf == m_tvd_lookup);
  const auto h = m_tvd_read->handle();
  assert(m_tvd->response_commit(*m_tvd_read, now) == tvd_directory::OK);
  m_ep_l2_payload.unpin(h);
  const auto ticket=m_tvd_lookup_ticket;
  assert(m_tvd->live(ticket) && m_tvd->at(ticket.index).payload==h);
  const auto &e=m_tvd->at(ticket.index);
  const auto &slot=m_ep_l2_payload.inspect(h);
  if (!slot.pins && slot.allocation_role==ep_l2_payload_store::WB &&
      (e.status==tvd_directory::WB_INFLIGHT || e.status==tvd_directory::WB_COMPLETE))
    m_ep_l2_payload.transfer(h,ep_l2_payload_store::TVD,
        e.status==tvd_directory::WB_COMPLETE?ep_l2_payload_store::TVD_VALID:ep_l2_payload_store::TVD_INFLIGHT);
  m_tvd_read.reset(); m_tvd_lookup = NULL;
  ++m_ft_stats.accepted;
}

bool l2_cache::tvd_promote(mem_fetch *mf, unsigned time, std::list<cache_event> &events) {
  const auto physical_cycle = m_gpu_cycle_for_tvd();
  const auto address = m_config.block_addr(mf->get_addr());
  assert(m_tvd_lookup == mf && m_tvd_lookup_done);
  auto t = m_tvd_lookup_ticket;
  if (!m_tvd->live(t)) return true;
  const auto old = m_tvd->at(t.index);
  if (old.mandatory() || old.readers) return false;
  // Preserve payload bank class. Select the oldest eligible resident way of
  // that class in the address's set; invalid ways take priority.
  unsigned index = (unsigned)-1;
  const unsigned first = m_config.set_index(address) * m_config.get_assoc();
  for (unsigned i = first; i < first + m_config.get_assoc(); ++i) {
    if (i % 4 != old.payload.payload_id % 4) continue;
    auto b = m_tag_array->get_block(i); auto h = tag_payload_handle(i);
    if (b->is_reserved_line()) continue;
    if (m_ep_l2_payload.handle_live(h) && (m_ep_l2_payload.inspect(h).pins ||
        m_ep_l2_payload.inspect(h).pending_sector_mask.any())) continue;
    if (index == (unsigned)-1 || b->is_invalid_line() ||
        b->get_last_access_time() < m_tag_array->get_block(index)->get_last_access_time()) index = i;
    if (b->is_invalid_line()) break;
  }
  if (index == (unsigned)-1 || m_ft_pending.request) return false;
  auto block = m_tag_array->get_block(index);
  const auto victim = tag_payload_handle(index);
  const bool live = m_ep_l2_payload.handle_live(victim);
  const bool dirty = block->is_modified_line();
  // Fixed control withholds64 slots even when empty. A clean replaced
  // resident gives back one same-bank non-victim credit; a dirty WB does not.
  if(!m_ep_l2_payload.fixed_promotion_room(old.payload,
      live&&!dirty?victim:ep_l2::PayloadHandle()))return false;
  ep_l2::PayloadHandle promoted; tvd_directory::ticket wb_ticket;
  if (dirty) {
    if (!miss_queue_has_slots(1)) return false;
    // This first adapter rejects local-WB-cap combinations at construction;
    // do not bypass the parent quota for an unqualified combined mode.
    assert(!m_config.m_ep_l2_wb_local_cap);
    assert(live);
    const auto saved = m_ep_l2_payload.inspect(victim);
    const unsigned dirty_mask = block->get_dirty_sector_mask().to_ulong();
    const unsigned ready_mask = saved.ready_sector_mask.to_ulong();
    const bool retain = m_config.m_ep_l2_tvd_dirty && (dirty_mask & ~ready_mask) == 0;
    auto plan = m_tvd->preview_dirty_promotion(t, block->m_block_addr, victim,
        retain ? ready_mask : dirty_mask, dirty_mask,
        saved.pending_sector_mask.to_ulong(), retain, true);
    if (plan.status != tvd_directory::OK) return false;
    evicted_block_info evicted;
    evicted.set_info(block->m_block_addr, block->get_modified_size(),
        block->get_dirty_byte_mask(), block->get_dirty_sector_mask());
    assert(m_tvd->commit_dirty_promotion(plan, physical_cycle, promoted, wb_ticket) == tvd_directory::OK);
    ++m_tvd_native_promotions;
    assert(m_ep_l2_wad_birth_cycle.insert(std::make_pair(evicted.m_block_addr, physical_cycle)).second);
    m_ep_l2_payload.transfer(victim, ep_l2_payload_store::WB, ep_l2_payload_store::WB_STAGED);
    auto &p = m_ft_pending; p.request = mf; p.dirty = p.wad = true;
    p.tag_index = index; p.victim = p.staging = victim;
    p.victim_address = evicted.m_block_addr; p.born = physical_cycle;
    p.wad_ticket = wb_ticket;
    mem_fetch *packet = m_memfetch_creator->alloc(evicted.m_block_addr, L2_WRBK_ACC,
        mf->get_access_warp_mask(), evicted.m_byte_mask, evicted.m_sector_mask,
        evicted.m_modified_size, true, time, -1, -1, -1, NULL, mf->get_streamID());
    packet->set_chip(mf->get_tlx_addr().chip); packet->set_partition(mf->get_tlx_addr().sub_partition);
    send_write_request(packet, cache_event(WRITE_BACK_REQUEST_SENT, evicted), time, events);
    assert(p.bound); p = ft_replacement();
  } else {
    assert(m_tvd->promote(t, promoted, physical_cycle) == tvd_directory::OK);
    ++m_tvd_native_promotions;
    if (live) m_ep_l2_payload.release(victim);
  }
  m_ep_l2_payload.transfer(promoted, ep_l2_payload_store::RESIDENT, ep_l2_payload_store::RESIDENT_VALID);
  m_tag_array->tvd_bind(index, address, time, old.valid_sectors);
  set_tag_payload_handle(index, promoted);
  assert(m_ep_l2_payload.ownership_consistent());
  return true;
}

void l2_cache::tvd_epoch_cleanup(unsigned long long now) {
  if (!m_tvd) return;
  assert(m_tvd->epoch_ready() && !m_tvd_read);
  while (tvd_reclaim_one((unsigned)-1, now)) {}
  assert(m_tvd->occupied() == 0); m_tvd_lookup = NULL;
}

void l2_cache::tvd_print(FILE *fp, unsigned slice) {
  m_tvd->advance(m_gpu_cycle_for_tvd()); const auto &s = m_tvd->stats();
  fprintf(fp,"EPL2TVDV3|METADATA_COST|slice=%u|frontend_cam_queries=%llu|frontend_cam_ports=1|completion_cam_queries=%llu|completion_cam_ports=1|indexed_wb_accept=%llu|indexed_rollback=%llu|native_install_commits=%llu|native_promotion_commits=%llu|extra_packet_identity_bits=0|directory_bound_wb_uid_bits=32|indexed_control_read_views=3|indexed_control_update_lanes=3|metadata_PPA=UNKNOWN|metadata_clock_closure=UNKNOWN\n",
      slice,m_tvd_lookup_queries,m_tvd_indexed_completions,m_tvd_indexed_accepts,
      m_tvd_indexed_rollbacks,m_tvd_native_installs,m_tvd_native_promotions);
  fprintf(fp, "EPL2TVDV2|SUMMARY|slice=%u|clean_installs=%llu|dirty_installs=%llu|"
      "read_responses=%llu|dirty_forward_responses=%llu|served_sectors=%llu|served_sector_bytes=%llu|"
      "promotions=%llu|live=%u|mandatory=%u|role_tvd=%u|retention_slot_cycles=%llu|"
      "clean_retention_slot_cycles=%llu|total_retention_slot_cycles=%llu|"
      "lookup_wait_cycles=%llu|wb_wait_cycles=%llu|bank_wait_cycles=%llu|icnt_wait_cycles=%llu|directory_queries=%llu\n", slice,
      (unsigned long long)s.clean_installs, (unsigned long long)s.dirty_installs,
      (unsigned long long)s.read_responses, (unsigned long long)s.dirty_forward_responses,
      (unsigned long long)s.served_sectors, (unsigned long long)s.served_sector_bytes,
      (unsigned long long)s.promotions, m_tvd->occupied(), m_tvd->mandatory(),
      m_ep_l2_payload.tvd_used(), (unsigned long long)s.tvd_extra_retention_slot_cycles,
      (unsigned long long)s.clean_retention_slot_cycles,
      (unsigned long long)(s.clean_retention_slot_cycles + s.tvd_extra_retention_slot_cycles),
      m_tvd_lookup_wait, m_tvd_wb_wait, m_tvd_bank_wait, m_tvd_resp_wait, m_tvd_lookup_queries);
}
