// Lane 2: production consumers of the sole Lane 1 R0 payload store.
unsigned long long l2_cache::m_gpu_cycle_for_tvd() const {
  return ft_physical_cycle();
}

bool l2_cache::tvd_install_victim(new_addr_type address, ep_l2::PayloadHandle h,
    unsigned valid, unsigned dirty, bool wb, unsigned long long now) {
  auto plan = m_tvd->preview_install(address, wb);
  if (plan.status != tvd_directory::OK) return false;
  tvd_directory::ticket ticket;
  if (m_tvd->commit_install(plan, address, h, valid, dirty, 0, wb, now, ticket) != tvd_directory::OK)
    return false;
  if (plan.displaced.valid()) {
    assert(m_ep_l2_payload.inspect(plan.displaced).pins == 0);
    m_ep_l2_payload.release(plan.displaced);
  }
  return true;
}

bool l2_cache::tvd_reclaim_one(unsigned bank, unsigned long long now) {
  for (unsigned i = 0; i < m_tvd->capacity(); ++i) {
    const auto &e = m_tvd->at(i);
    if (!e.reclaimable() || !e.payload.valid() ||
        (bank != (unsigned)-1 && e.payload.payload_id % 4 != bank)) continue;
    ep_l2::PayloadHandle h;
    assert(m_tvd->reclaim(tvd_directory::ticket(i, e.incarnation), h, now) == tvd_directory::OK);
    assert(m_ep_l2_payload.inspect(h).pins == 0);
    m_ep_l2_payload.release(h); return true;
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
    m_tvd_lookup = mf; m_tvd_lookup_cycle = now; ++m_tvd_lookup_wait; return TVD_WAIT;
  }
  if (now <= m_tvd_lookup_cycle) return TVD_WAIT;
  auto ticket = m_tvd->find(address);
  if (!m_tvd->live(ticket)) return TVD_NORMAL;
  const auto &entry = m_tvd->at(ticket.index);
  // Before lower acceptance the existing WB readout owns service. Forwarding
  // starts at actual INFLIGHT, before the generic WAD hazard check, and does
  // not wait for set_done. This also avoids aliasing WB/TVD bank retry tokens.
  if (entry.status == tvd_directory::WB_STAGED) { ++m_tvd_wb_wait; return TVD_WAIT; }
  auto op = mf->isatomic() ? tvd_directory::ATOMIC_OP :
            mf->get_is_write() ? tvd_directory::WRITE_OP : tvd_directory::READ_OP;
  auto observation = m_tvd->preview_access(address, mf->get_access_sector_mask().to_ulong(),
      op, m_tvd_lookup_cycle, now);
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
      ep_l2_payload_store::TVD_READ) != ep_l2_payload_store::GRANTED) { ++m_tvd_bank_wait; return TVD_WAIT; }
  mf->set_ep_l2_payload_identity(h.payload_id, h.generation);
  mf->set_ft_wb_data(m_ep_l2_payload.inspect(h).versions);
  return TVD_RESPONSE;
}

void l2_cache::tvd_response_commit(mem_fetch *mf, unsigned long long now) {
  assert(m_tvd_read && mf == m_tvd_lookup);
  const auto h = m_tvd_read->handle();
  assert(m_tvd->response_commit(*m_tvd_read, now) == tvd_directory::OK);
  m_ep_l2_payload.unpin(h); m_tvd_read.reset(); m_tvd_lookup = NULL;
  ++m_ft_stats.accepted;
}

bool l2_cache::tvd_promote(mem_fetch *mf, unsigned time, std::list<cache_event> &events) {
  const auto physical_cycle = m_gpu_cycle_for_tvd();
  const auto address = m_config.block_addr(mf->get_addr());
  auto t = m_tvd->find(address);
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
  ep_l2::PayloadHandle promoted; tvd_directory::ticket wb_ticket;
  if (dirty) {
    if (!miss_queue_has_slots(1)) return false;
    assert(live);
    const auto saved = m_ep_l2_payload.inspect(victim);
    const unsigned dirty_mask = block->get_dirty_sector_mask().to_ulong();
    const unsigned ready_mask = saved.ready_sector_mask.to_ulong();
    const bool retain = m_config.m_ep_l2_tvd_dirty && (dirty_mask & ~ready_mask) == 0;
    auto plan = m_tvd->preview_dirty_promotion(t, block->m_block_addr, victim,
        retain ? ready_mask : dirty_mask, dirty_mask,
        saved.pending_sector_mask.to_ulong(), retain);
    if (plan.status != tvd_directory::OK) return false;
    evicted_block_info evicted;
    evicted.set_info(block->m_block_addr, block->get_modified_size(),
        block->get_dirty_byte_mask(), block->get_dirty_sector_mask());
    assert(m_tvd->commit_dirty_promotion(plan, physical_cycle, promoted, wb_ticket) == tvd_directory::OK);
    assert(m_ep_l2_wad_birth_cycle.insert(std::make_pair(evicted.m_block_addr, physical_cycle)).second);
    m_ep_l2_payload.transfer(victim, ep_l2_payload_store::WB, ep_l2_payload_store::WB_STAGED);
    auto &p = m_ft_pending; p.request = mf; p.dirty = p.wad = true;
    p.tag_index = index; p.victim = p.staging = victim;
    p.victim_address = evicted.m_block_addr; p.born = physical_cycle;
    mem_fetch *packet = m_memfetch_creator->alloc(evicted.m_block_addr, L2_WRBK_ACC,
        mf->get_access_warp_mask(), evicted.m_byte_mask, evicted.m_sector_mask,
        evicted.m_modified_size, true, time, -1, -1, -1, NULL, mf->get_streamID());
    packet->set_chip(mf->get_tlx_addr().chip); packet->set_partition(mf->get_tlx_addr().sub_partition);
    send_write_request(packet, cache_event(WRITE_BACK_REQUEST_SENT, evicted), time, events);
    assert(p.bound); p = ft_replacement();
  } else {
    assert(m_tvd->promote(t, promoted, physical_cycle) == tvd_directory::OK);
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
  fprintf(fp, "EPL2TVDV2|SUMMARY|slice=%u|clean_installs=%llu|dirty_installs=%llu|"
      "read_responses=%llu|dirty_forward_responses=%llu|served_sectors=%llu|served_sector_bytes=%llu|"
      "promotions=%llu|live=%u|mandatory=%u|role_tvd=%u|retention_slot_cycles=%llu|"
      "lookup_wait_cycles=%llu|wb_wait_cycles=%llu|bank_wait_cycles=%llu|icnt_wait_cycles=%llu\n", slice,
      (unsigned long long)s.clean_installs, (unsigned long long)s.dirty_installs,
      (unsigned long long)s.read_responses, (unsigned long long)s.dirty_forward_responses,
      (unsigned long long)s.served_sectors, (unsigned long long)s.served_sector_bytes,
      (unsigned long long)s.promotions, m_tvd->occupied(), m_tvd->mandatory(),
      m_ep_l2_payload.tvd_used(), (unsigned long long)s.tvd_extra_retention_slot_cycles,
      m_tvd_lookup_wait, m_tvd_wb_wait, m_tvd_bank_wait, m_tvd_resp_wait);
}
