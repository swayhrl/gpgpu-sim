// FT20261016 production integration. Included once by gpu-cache.cc.
// The all-OFF path remains in the historical implementation.
unsigned long long l2_cache::ft_physical_cycle() const {
  return m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle;
}

void l2_cache::force_tag_access(new_addr_type addr, unsigned time,
                                mem_access_sector_mask_t mask) {
  if (!ft_enabled()) { baseline_cache::force_tag_access(addr, time, mask); return; }
  if (!ft_try_force_tag_access(addr, time, mask)) {
    fprintf(stderr, "EP-L2 host prefill requires quiescent clean ownership; no pending/dirty victim may be discarded\n");
    abort();
  }
}

bool l2_cache::ft_try_force_tag_access(new_addr_type addr, unsigned time,
                                      mem_access_sector_mask_t mask) {
  // This is untimed host initialization, not a source of GPU bank grants or
  // free WB service. Fail BEFORE changing Tag, generation, owner or offset.
  if (!ft_enabled() || m_config.m_cache_type != SECTOR || mask.count() != 1 ||
      !ft_drained() || !m_ep_l2_wad_entries_live.empty() ||
      mshr_entries_used() || !miss_queue_empty() ||
      m_ep_l2_payload.pending_sector_count() || m_ep_l2_payload.pending_operations())
    return false;
  const new_addr_type owner = m_config.block_addr(addr);
  unsigned index = (unsigned)-1;
  const cache_request_status status = m_tag_array->probe(owner, index, mask, true, true);
  if (status == RESERVATION_FAIL || status == HIT_RESERVED ||
      index >= m_config.get_num_lines() || m_tag_array->block_is_modified(index))
    return false;
  ep_l2::PayloadHandle handle = tag_payload_handle(index);
  const bool live = m_ep_l2_payload.handle_live(handle);
  if (live) {
    const ep_l2_payload_store::slot &old = m_ep_l2_payload.inspect(handle);
    if (old.allocation_role != ep_l2_payload_store::RESIDENT || old.pins ||
        old.pending_sector_mask.any() || old.status == ep_l2_payload_store::RESIDENT_DIRTY ||
        handle.payload_id % 4 != index % 4)
      return false;
  }
  if (status == MISS) {
    unsigned landing = index;
    if (live) {
      // release and reserve each advance the generation; check both before
      // mutating either visibility. Epoch exhaustion is a refusal, not reuse.
      if (handle.generation >= std::numeric_limits<unsigned>::max() - 1)
        return false;
      landing = handle.payload_id;
      m_ep_l2_payload.release(handle);
    } else if (m_config.m_ep_l2_ft_mode == 2) {
      const ep_l2::PayloadHandle free = m_ep_l2_payload.preview_free(
          ep_l2_payload_store::RESIDENT, index % 4);
      if (!free.valid()) return false;
      landing = free.payload_id;
    } else {
      const ep_l2_payload_store::slot &free = m_ep_l2_payload.resident(landing);
      if (free.occupied() || free.generation == std::numeric_limits<unsigned>::max())
        return false;
    }
    assert(landing % 4 == index % 4);
    handle = m_ep_l2_payload.reserve(landing, ep_l2_payload_store::RESIDENT,
        owner, ep_l2_payload_store::RESIDENT_VALID);
    set_tag_payload_handle(index, handle);
  } else {
    if (!m_ep_l2_payload.handle_owner_matches(handle, owner)) return false;
  }
  baseline_cache::force_tag_access(addr, time, mask);
  unsigned actual_index = (unsigned)-1;
  assert(m_tag_array->probe(owner, actual_index, mask, true, true) == HIT &&
         actual_index == index && "host prefill must commit the pre-reserved Tag index");
  ep_l2_payload_store::slot &slot = m_ep_l2_payload.inspect_mutable(handle);
  slot.ready_sector_mask |= mask;
  // Dataless host initialization has the same explicit version-zero symbol as
  // ordinary untouched lower memory, not fabricated numerical payload bytes.
  for (unsigned sector = 0; sector < 4; ++sector)
    if (mask.test(sector)) slot.versions[sector] = 0;
  slot.status = m_tag_array->block_is_modified(index) ?
      ep_l2_payload_store::RESIDENT_DIRTY : ep_l2_payload_store::RESIDENT_VALID;
  assert(m_ep_l2_payload.ownership_consistent());
  return true;
}

void l2_cache::descriptor_admitted(mem_fetch *mf, unsigned index) {
  if (!ft_enabled()) return;
  const ep_l2::PayloadHandle h = tag_payload_handle(index);
  assert(m_ep_l2_payload.handle_owner_matches(h, m_config.block_addr(mf->get_addr())));
  mf->set_ep_l2_payload_identity(h.payload_id, h.generation);
  assert(m_ft_read_refs.insert(std::make_pair(mf, h)).second);
  m_ep_l2_payload.pin(h);
}

void l2_cache::lower_read_enqueued(mem_fetch *mf, unsigned index) {
  if (!ft_enabled()) return;
  const ep_l2::PayloadHandle h = tag_payload_handle(index);
  assert(m_ft_read_refs.count(mf));
  m_ep_l2_payload.note_resident_lower_read(h.payload_id,
      m_config.block_addr(mf->get_addr()), h.generation, mf->get_access_sector_mask());
}

mem_fetch *l2_cache::peek_next_access() const {
  mem_fetch *mf = baseline_cache::peek_next_access();
  if (ft_enabled()) {
    std::map<mem_fetch *,ep_l2::PayloadHandle>::const_iterator it = m_ft_read_refs.find(mf);
    assert(it != m_ft_read_refs.end());
    mf->set_ft_wb_data(m_ep_l2_payload.inspect(it->second).versions);
  }
  return mf;
}

bool l2_cache::ft_response_ready(unsigned long long cycle) {
  if (!ft_enabled()) return true;
  mem_fetch *mf = baseline_cache::peek_next_access();
  std::map<mem_fetch *,ep_l2::PayloadHandle>::const_iterator it = m_ft_read_refs.find(mf);
  assert(it != m_ft_read_refs.end());
  const bool granted = m_ep_l2_payload.request(it->second, false, cycle,
      (unsigned long long)-1, ep_l2_payload_store::RESIDENT_HIT_READ) == ep_l2_payload_store::GRANTED;
  if (granted && mf->isatomic()) {
    // Symbolic write-completion label only, matching the existing atomic-HIT
    // convention. Do not execute the PTX atomic callback here: it belongs to
    // the response consumer (memory_sub_partition::pop / shader). Pins retain
    // this exact generation until the response is enqueued and committed.
    auto &slot = m_ep_l2_payload.inspect_mutable(it->second);
    for (unsigned s = 0; s < 4; ++s)
      if (mf->get_access_sector_mask().test(s))
        slot.versions[s] = (unsigned long long)mf->get_request_uid() + 1;
  }
  return granted;
}

void l2_cache::cycle() {
  baseline_cache::cycle();
  if (ft_enabled()) {
    ++m_ft_stats.observed_cycles;
    m_ft_stats.staging_occupancy_sum += m_ep_l2_payload.wb_used();
  }
}

void l2_cache::commit_next_access() {
  if (!ft_enabled()) { baseline_cache::commit_next_access(); return; }
  // A synthetic write-allocate response may already have been deleted by the
  // frontend. Use its pointer only as an identity key, never dereference it.
  mem_fetch *mf = baseline_cache::peek_next_access();
  std::map<mem_fetch *,ep_l2::PayloadHandle>::iterator it = m_ft_read_refs.find(mf);
  assert(it != m_ft_read_refs.end());
  const ep_l2::PayloadHandle h = it->second;
  baseline_cache::commit_next_access();
  m_ep_l2_payload.unpin(h);
  m_ft_read_refs.erase(it);
  ++m_ft_stats.descriptor_responses;
}

void l2_cache::ft_rollback() {
  ft_replacement &p = m_ft_pending;
  assert(p.request && !p.bound);
  if (p.wad) {
    assert(m_ep_l2_wad_entries_live.erase(p.victim_address) == 1);
    assert(m_ep_l2_wad_birth_cycle.erase(p.victim_address) == 1);
  }
  if (p.staging.valid() && p.staging != p.victim && m_ep_l2_payload.handle_live(p.staging)) {
    m_ep_l2_payload.cancel_request(p.staging, true);
    m_ep_l2_payload.release(p.staging);
  }
  if (p.landing_reserved && m_ep_l2_payload.handle_live(p.landing))
    m_ep_l2_payload.release(p.landing);
  if (p.victim.valid()) m_ep_l2_payload.restore(p.victim.payload_id, p.saved);
  set_tag_payload_handle(p.tag_index, p.victim);
  ++m_ft_stats.rollback;
  p = ft_replacement();
}

enum cache_request_status l2_cache::ft_access(new_addr_type addr, mem_fetch *mf,
    unsigned time, std::list<cache_event> &events) {
  // memcpy offset orders native Tag timestamps; it is not elapsed hardware
  // time and must never reset the payload bank arbiter or WB/WAD lifetimes.
  const unsigned long long physical_cycle = ft_physical_cycle();
  m_ep_l2_last_payload_request_result = ep_l2_payload_store::GRANTED;
  const new_addr_type owner = m_config.block_addr(addr);
  unsigned index = (unsigned)-1;
  cache_request_status probe = m_tag_array->probe(owner, index, mf, mf->is_write(), true);
  if (probe == RESERVATION_FAIL) return RESERVATION_FAIL;
  if (m_ft_pending.request && m_ft_pending.request != mf) return RESERVATION_FAIL;
  if (ep_l2_wad_contains(owner)) { ++m_ep_l2_wad_same_address_wait_count; return RESERVATION_FAIL; }

  if (probe == MISS && !m_ft_pending.request) {
    // Retire completed invalidated tag identities (e.g. a kernel flush), never
    // a pinned response or a live pending sector. No address-wide RO cleanup.
    for (unsigned t = 0; t < m_config.get_num_lines(); ++t) {
      const ep_l2::PayloadHandle h = tag_payload_handle(t);
      if (!m_ep_l2_payload.handle_live(h)) continue;
      const ep_l2_payload_store::slot &s = m_ep_l2_payload.inspect(h);
      if (s.allocation_role == ep_l2_payload_store::RESIDENT &&
          m_tag_array->get_block(t)->is_invalid_line() && !s.pins && s.pending_sector_mask.none()) {
        m_ep_l2_payload.release(h); set_tag_payload_handle(t, ep_l2::PayloadHandle());
      }
    }
    const ep_l2::PayloadHandle victim = tag_payload_handle(index);
    const bool live = m_ep_l2_payload.handle_live(victim);
    const bool dirty = m_tag_array->block_is_modified(index);
    if (live && (m_ep_l2_payload.inspect(victim).pins ||
                 m_ep_l2_payload.inspect(victim).pending_sector_mask.any())) {
      ++m_ft_stats.pin_block; return RESERVATION_FAIL;
    }
    assert(!dirty || live);
    // Compute the actual target policy's issue needs before acquiring anything.
    const bool read = !mf->get_is_write();
    const new_addr_type key = m_config.mshr_addr(addr);
    unsigned slots = dirty ? 1 : 0;
    if (read) {
      if (m_mshrs.full(key)) return RESERVATION_FAIL;
      if (m_mshrs.needs_lower_read(key, mf->get_access_sector_mask())) ++slots;
    } else if (m_config.m_write_alloc_policy == WRITE_ALLOCATE) {
      if (m_mshrs.full(key)) return RESERVATION_FAIL;
      slots += 2;
    } else if (m_config.m_write_alloc_policy == FETCH_ON_WRITE &&
               mf->get_access_byte_mask().count() != m_config.get_atom_sz()) {
      if (m_mshrs.full(key)) return RESERVATION_FAIL;
      ++slots;
    }
    if (!miss_queue_has_slots(slots)) { ++m_ft_stats.issue_block; return RESERVATION_FAIL; }
    if (dirty && m_ep_l2_wad_entries_live.size() >= m_config.m_ep_l2_wad_entries) {
      ++m_ep_l2_wad_full_block_count; return RESERVATION_FAIL;
    }
    ep_l2::PayloadHandle staging, landing;
    if (dirty && m_config.m_ep_l2_ft_mode == 1) {
      staging = m_ep_l2_payload.preview_free(ep_l2_payload_store::WB);
      if (!staging.valid()) { ++m_ft_stats.wbuf_full; return RESERVATION_FAIL; }
    }
    if (m_config.m_ep_l2_ft_mode == 2 && (dirty || !live)) {
      landing = m_ep_l2_payload.preview_free(ep_l2_payload_store::RESIDENT, index % 4);
      if (!landing.valid()) { ++m_ft_stats.pool_full; return RESERVATION_FAIL; }
      if (dirty) staging = victim;
    }
    // The only producer is the serialized frontend head. While this token is
    // live no other frontend request can steal its prechecked issue slots;
    // fills and lower drains cannot add entries to m_miss_queue.
    ft_replacement &p = m_ft_pending;
    p.request = mf; p.tag_index = index; p.dirty = dirty; p.victim = live ? victim : ep_l2::PayloadHandle();
    p.born = physical_cycle;
    p.landing_id = landing.valid() ? landing.payload_id :
        (m_config.m_ep_l2_ft_mode == 2 && live ? victim.payload_id : index);
    if (live) p.saved = m_ep_l2_payload.inspect(victim);
    if (dirty) {
      p.victim_address = m_tag_array->get_block(index)->m_block_addr;
      assert(m_ep_l2_wad_entries_live.insert(p.victim_address).second);
      assert(m_ep_l2_wad_birth_cycle.insert(std::make_pair(p.victim_address, physical_cycle)).second);
      p.wad = true;
      if (m_config.m_ep_l2_ft_mode == 1)
        p.staging = m_ep_l2_payload.reserve(staging.payload_id,
            ep_l2_payload_store::WB, p.victim_address, ep_l2_payload_store::WB_RESERVED);
      else p.staging = staging;
    }
    if (landing.valid()) {
      p.landing = m_ep_l2_payload.reserve(landing.payload_id,
          ep_l2_payload_store::RESIDENT, owner, ep_l2_payload_store::RESIDENT_FILL_PENDING);
      p.landing_reserved = true;
    }
  }

  bool replacement = m_ft_pending.request == mf;
  if (replacement) {
    ft_replacement &p = m_ft_pending;
    assert(p.tag_index == index && probe == MISS);
    if (p.dirty && m_config.m_ep_l2_ft_mode == 1) {
      // A shared, explicitly costed 128B copy-pipeline latch bridges same-bank
      // read and write cycles. Both compared organizations include this latch.
      if (p.phase == 0) {
        m_ep_l2_last_payload_request_result = m_ep_l2_payload.request(p.victim,
            false, physical_cycle, (unsigned long long)-1, ep_l2_payload_store::WB_READOUT);
        if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED) {
          ++m_ft_stats.bank_block; return RESERVATION_FAIL;
        }
        p.born = physical_cycle; // actual source readout starts data-staging lifetime
        p.copy_versions = m_ep_l2_payload.inspect(p.victim).versions;
        p.phase = 1;
        return RESERVATION_FAIL;
      }
      if (p.phase == 1) {
        m_ep_l2_last_payload_request_result = m_ep_l2_payload.request(p.staging,
            true, physical_cycle, (unsigned long long)-1, ep_l2_payload_store::RESIDENT_WRITE);
        if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED) {
          ++m_ft_stats.bank_block; return RESERVATION_FAIL;
        }
        m_ep_l2_payload.inspect_mutable(p.staging).versions = p.copy_versions;
        m_ep_l2_payload.transfer(p.staging, ep_l2_payload_store::WB, ep_l2_payload_store::WB_STAGED);
        ++m_ft_stats.copy_lines; p.phase = 2;
      }
    }
    // Locally absorbed writes consume an actual payload RAM write. Request
    // the stable prospective ID before changing victim/tag ownership, so a
    // same-bank denial cannot expose a partially installed new generation.
    if (mf->get_is_write()) {
      m_ep_l2_last_payload_request_result = m_ep_l2_payload.request(
          ep_l2_payload_store::RESIDENT, p.landing_id, true, physical_cycle,
          (unsigned long long)-1, ep_l2_payload_store::RESIDENT_WRITE);
      if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED) {
        ++m_ft_stats.bank_block; return RESERVATION_FAIL;
      }
    }
    if (p.dirty && m_config.m_ep_l2_ft_mode == 2) {
      p.born = physical_cycle; // in-place WB role starts at actual producer commit
      m_ep_l2_payload.transfer(p.victim, ep_l2_payload_store::WB, ep_l2_payload_store::WB_STAGED);
      if (p.victim.payload_id < 1024) ++m_ft_stats.borrow_resident;
      else if (p.victim.payload_id < 1152) ++m_ft_stats.borrow_bypass;
    }
    if (!p.landing_reserved) {
      const unsigned id = p.landing_id;
      if (m_ep_l2_payload.handle_live(p.victim)) m_ep_l2_payload.release(p.victim);
      p.landing = m_ep_l2_payload.reserve(id, ep_l2_payload_store::RESIDENT,
          owner, ep_l2_payload_store::RESIDENT_FILL_PENDING);
      p.landing_reserved = true;
    }
    set_tag_payload_handle(index, p.landing);
  } else {
    const ep_l2::PayloadHandle h = tag_payload_handle(index);
    assert(m_ep_l2_payload.handle_owner_matches(h, owner));
    if ((mf->get_is_write() || mf->isatomic()) && m_ep_l2_payload.inspect(h).pins) {
      ++m_ft_stats.pin_block; return RESERVATION_FAIL;
    }
    if (probe == HIT || mf->get_is_write()) {
      m_ep_l2_last_payload_request_result = m_ep_l2_payload.request(h,
          mf->get_is_write() || mf->isatomic(), physical_cycle, (unsigned long long)-1,
          mf->get_is_write() || mf->isatomic() ? ep_l2_payload_store::RESIDENT_WRITE : ep_l2_payload_store::RESIDENT_HIT_READ);
      if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED) {
        ++m_ft_stats.bank_block; return RESERVATION_FAIL;
      }
    }
  }
  const ep_l2::PayloadHandle handle = tag_payload_handle(index);
  mf->set_ep_l2_payload_identity(handle.payload_id, handle.generation);
  const cache_request_status result = data_cache::access(addr, mf, time, events);
  if (result == RESERVATION_FAIL) {
    if (replacement) ft_rollback();
    return result;
  }
  if (replacement && m_ft_pending.dirty) assert(m_ft_pending.bound);
  ep_l2_payload_store::slot &slot = m_ep_l2_payload.inspect_mutable(handle);
  if (mf->get_is_write() || (mf->isatomic() && probe == HIT)) {
    for (unsigned s = 0; s < 4; ++s)
      if (mf->get_access_sector_mask().test(s)) slot.versions[s] = (unsigned long long)mf->get_request_uid() + 1;
    if (mf->get_access_byte_mask().count() == m_config.get_atom_sz() || probe == HIT)
      slot.ready_sector_mask |= mf->get_access_sector_mask();
  }
  if (m_tag_array->block_is_modified(index)) slot.status = ep_l2_payload_store::RESIDENT_DIRTY;
  else if (slot.pending_sector_mask.none()) slot.status = ep_l2_payload_store::RESIDENT_VALID;
  if (!mf->get_is_write()) mf->set_ft_wb_data(slot.versions);
  if (replacement) m_ft_pending = ft_replacement();
  ++m_ft_stats.accepted;
  m_tag_array->l2_char_tracking_refresh_line(index);
  assert(m_ep_l2_payload.ownership_consistent());
  return result;
}

void l2_cache::send_write_request(mem_fetch *mf, cache_event event, unsigned time,
    std::list<cache_event> &events) {
  if (ft_enabled() && mf->get_access_type() == L2_WRBK_ACC) {
    ft_replacement &p = m_ft_pending;
    assert(p.request && p.dirty && p.wad && !p.bound);
    assert(m_config.block_addr(mf->get_addr()) == p.victim_address);
    ft_wb_record r; r.handle = p.staging; r.born = p.born;
    r.expected = m_ep_l2_payload.inspect(p.staging).versions;
    assert(m_ft_wb.size() < m_config.m_ep_l2_wad_entries);
    assert(m_ft_wb.insert(std::make_pair(mf, r)).second);
    mf->set_ep_l2_payload_identity(p.staging.payload_id, p.staging.generation);
    p.bound = true; ++m_ft_stats.wb_created;
    m_ft_stats.staged_max = std::max(m_ft_stats.staged_max, (unsigned long long)m_ft_wb.size());
  }
  data_cache::send_write_request(mf, event, time, events);
}

bool l2_cache::lower_ready(mem_fetch *mf, unsigned long long cycle) {
  if (!ft_enabled() || mf->get_access_type() != L2_WRBK_ACC) return true;
  std::map<mem_fetch *,ft_wb_record>::iterator it = m_ft_wb.find(mf);
  assert(it != m_ft_wb.end());
  const ep_l2_payload_store::request_result result = m_ep_l2_payload.request(
      it->second.handle, false, cycle, (unsigned long long)-1, ep_l2_payload_store::WB_READOUT);
  if (result != ep_l2_payload_store::GRANTED) { ++m_ft_stats.bank_block; return false; }
  assert(m_ep_l2_payload.inspect(it->second.handle).versions == it->second.expected);
  m_ft_last_wb_versions = it->second.expected;
  mf->set_ft_wb_data(it->second.expected);
  ++m_ft_stats.staging_reads; ++m_ft_stats.data_checks;
  return true;
}

void l2_cache::lower_accepted(mem_fetch *mf, unsigned long long cycle) {
  if (!ft_enabled() || mf->get_access_type() != L2_WRBK_ACC) return;
  std::map<mem_fetch *,ft_wb_record>::iterator it = m_ft_wb.find(mf);
  assert(it != m_ft_wb.end());
  assert(mf->has_ft_wb_data() && mf->get_ft_wb_data() == it->second.expected);
  // The physical lower FIFO now carries this packet's dirty sector bytes;
  // all subsequent queues must retain data until actual set_done. No original
  // payload pointer is needed with TVD disabled. These FIFO bytes are ledgered.
  m_ep_l2_payload.release(it->second.handle);
  m_ft_stats.staging_lifetime += cycle >= it->second.born ? cycle - it->second.born : 0;
  ++m_ft_stats.lower_accept;
  m_ft_wb.erase(it);
}

void l2_cache::ft_print(FILE *fp, unsigned slice) const {
  if (!ft_enabled()) return;
  fprintf(fp, "EPL2FTV1|SUMMARY|slice=%u|mode=%u|wbuf=%u|physical_slots=%u|"
      "accepted=%llu|wb_created=%llu|wb_completed=%llu|wb_lower_accept=%llu|wbuf_full=%llu|pool_full=%llu|pin_block=%llu|"
      "bank_block=%llu|issue_block=%llu|copy_lines=%llu|copy_bytes=%llu|staging_reads=%llu|"
      "staged_max=%llu|staging_lifetime=%llu|borrow_resident=%llu|borrow_bypass=%llu|"
      "rollback=%llu|version_checks=%llu|wb_live=%u|wb_slots=%u|reserved=%u|read_refs=%u|"
      "role_free=%u|role_resident=%u|role_bypass=%u|ownership_consistent=%u|"
      "observed_cycles=%llu|wb_occupancy_sum=%llu|descriptor_responses=%llu\n",
      slice, m_config.m_ep_l2_ft_mode, m_config.m_ep_l2_wbuf_entries,
      m_ep_l2_payload.physical_slots(), m_ft_stats.accepted, m_ft_stats.wb_created,
      m_ep_l2_wad_lifetime_count, m_ft_stats.lower_accept,
      m_ft_stats.wbuf_full, m_ft_stats.pool_full, m_ft_stats.pin_block,
      m_ft_stats.bank_block, m_ft_stats.issue_block, m_ft_stats.copy_lines,
      m_ft_stats.copy_lines * 128, m_ft_stats.staging_reads, m_ft_stats.staged_max,
      m_ft_stats.staging_lifetime, m_ft_stats.borrow_resident, m_ft_stats.borrow_bypass,
      m_ft_stats.rollback, m_ft_stats.data_checks, (unsigned)m_ft_wb.size(),
      m_ep_l2_payload.wb_used(), m_ft_pending.request ? 1 : 0, (unsigned)m_ft_read_refs.size(),
      m_ep_l2_payload.free_slots(), m_ep_l2_payload.resident_used(), m_ep_l2_payload.bypass_used(),
      m_ep_l2_payload.ownership_consistent() ? 1 : 0, m_ft_stats.observed_cycles,
      m_ft_stats.staging_occupancy_sum, m_ft_stats.descriptor_responses);
}
