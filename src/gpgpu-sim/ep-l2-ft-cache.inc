// FT20261016 production integration. Included once by gpu-cache.cc.
// The all-OFF path remains in the historical implementation.
unsigned long long l2_cache::ft_physical_cycle() const {
  return m_gpu->gpu_sim_cycle + m_gpu->gpu_tot_sim_cycle;
}

void l2_cache::force_tag_access(new_addr_type addr, unsigned time,
                                mem_access_sector_mask_t mask) {
  if (!ft_enabled()) { baseline_cache::force_tag_access(addr, time, mask); return; }
  if(m_ro)m_ro->prefill_begin(m_config.block_addr(addr));
  if (!ft_try_force_tag_access(addr, time, mask)) {
    fprintf(stderr, "EP-L2 host prefill requires quiescent clean ownership; no pending/dirty victim may be discarded\n");
    abort();
  }
}

bool l2_cache::ft_try_force_tag_access(new_addr_type addr, unsigned time,
                                      mem_access_sector_mask_t mask) {
  if(m_ro && !m_ro->prefill_allowed(m_config.block_addr(addr)))return false;
  // This is untimed host initialization, not a source of GPU bank grants or
  // free WB service. Fail BEFORE changing Tag, generation, owner or offset.
  if (!ft_enabled() || m_config.m_cache_type != SECTOR || mask.count() != 1 ||
      !ft_drained() || ep_l2_wad_occupancy() || m_tvd_lookup || m_tvd_read ||
      mshr_entries_used() || !miss_queue_empty() ||
      m_ep_l2_payload.pending_sector_count() || m_ep_l2_payload.pending_operations())
    return false;
  const new_addr_type owner = m_config.block_addr(addr);
  unsigned index = (unsigned)-1;
  const cache_request_status status = m_tag_array->probe(owner, index, mask, true, true);
  if (status == RESERVATION_FAIL || status == HIT_RESERVED ||
      index >= m_config.get_num_lines() || m_tag_array->block_is_modified(index) ||
      m_tag_array->get_block(index)->is_reserved_line())
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
  // TVD is another visibility of this exact host address. Preview all guards
  // before removing its optional copy; failed try-prefill changes no state.
  const auto alias = m_tvd ? m_tvd->find(owner) : tvd_directory::ticket();
  const bool has_alias = m_tvd && m_tvd->live(alias);
  ep_l2::PayloadHandle alias_payload;
  if (has_alias) {
    const auto &entry = m_tvd->at(alias.index);
    alias_payload = entry.payload;
    if (status != MISS || !entry.reclaimable() ||
        entry.incarnation == std::numeric_limits<std::uint64_t>::max() ||
        !m_ep_l2_payload.handle_owner_matches(alias_payload, owner))
      return false;
    const auto &slot = m_ep_l2_payload.inspect(alias_payload);
    if (slot.allocation_role != ep_l2_payload_store::TVD || slot.pins ||
        slot.pending_sector_mask.any() ||
        alias_payload.generation == std::numeric_limits<unsigned>::max())
      return false;
  }
  unsigned landing = index;
  if (status == MISS) {
    if (live) {
      // release and reserve each advance the generation; check both before
      // mutating either visibility. Epoch exhaustion is a refusal, not reuse.
      if (handle.generation >= std::numeric_limits<unsigned>::max() - 1)
        return false;
      landing = handle.payload_id;
    } else if (m_config.m_ep_l2_ft_mode == 2) {
      const ep_l2::PayloadHandle free = m_ep_l2_payload.preview_free(
          ep_l2_payload_store::RESIDENT, index % 4);
      if (free.valid()) {
        if (!free.generation) return false; // preview must not wrap generation
        landing = free.payload_id;
      } else {
        if (!alias_payload.valid() || alias_payload.payload_id % 4 != index % 4 ||
            alias_payload.generation >= std::numeric_limits<unsigned>::max() - 1)
          return false;
        landing = alias_payload.payload_id;
      }
    } else {
      const ep_l2_payload_store::slot &free = m_ep_l2_payload.resident(landing);
      if (free.occupied() || free.generation == std::numeric_limits<unsigned>::max())
        return false;
    }
    assert(landing % 4 == index % 4);
  } else if (!m_ep_l2_payload.handle_owner_matches(handle, owner)) {
    return false;
  }
  // Commit starts here: every resource and refusal has been previewed above.
  if (has_alias) {
    ep_l2::PayloadHandle released;
    assert(m_tvd->reclaim(alias, released, ft_physical_cycle()) == tvd_directory::OK);
    assert(released == alias_payload);
    m_ep_l2_payload.release(released);
  }
  if (status == MISS) {
    if (live) m_ep_l2_payload.release(handle);
    handle = m_ep_l2_payload.reserve(landing, ep_l2_payload_store::RESIDENT,
        owner, ep_l2_payload_store::RESIDENT_VALID);
    set_tag_payload_handle(index, handle);
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
  if(m_ro)m_ro->prefill_complete(index,handle,owner,mask);
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
  if(m_ro && m_ro->selected)return m_ro->reply.request;
  mem_fetch *mf = baseline_cache::peek_next_access();
  if (ft_enabled()) {
    std::map<mem_fetch *,ep_l2::PayloadHandle>::const_iterator it = m_ft_read_refs.find(mf);
    assert(it != m_ft_read_refs.end());
    mf->set_ft_wb_data(m_ep_l2_payload.inspect(it->second).versions);
  }
  return mf;
}

bool l2_cache::ft_response_ready(unsigned long long cycle) {
  if(m_ro && !m_ro->native_response_chosen && m_ro->has_response() &&
      (m_ro->reply.transaction.valid() || m_ro->prefer_ro || !baseline_cache::access_ready()))
    return m_ro->prepare(cycle);
  if (!ft_enabled()) return true;
  if(m_ro)m_ro->native_response_chosen=true; // Before the possibly denied read.
  mem_fetch *mf = baseline_cache::peek_next_access();
  std::map<mem_fetch *,ep_l2::PayloadHandle>::const_iterator it = m_ft_read_refs.find(mf);
  assert(it != m_ft_read_refs.end());
  const bool granted = m_ep_l2_payload.request(it->second, false, cycle,
      (unsigned long long)-1, ep_l2_payload_store::RESIDENT_HIT_READ,ep_l2_payload_store::RESPONSE,mf->get_request_uid()) == ep_l2_payload_store::GRANTED;
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

void l2_cache::ms_sample() {
  if (!m_ms_observer.enabled) return;
  unsigned bound = 0;
  for (unsigned t = 0; t < m_config.get_num_lines(); ++t) {
    const ep_l2::PayloadHandle h = tag_payload_handle(t);
    bound += !m_tag_array->get_block(t)->is_invalid_line() &&
        m_ep_l2_payload.handle_live(h);
  }
  m_ms_observer.sample(m_ep_l2_payload.wb_used(), ep_l2_wad_occupancy(),
      m_mshrs.descriptor_count_used(), m_ep_l2_payload.resident_used(), bound,
      m_ep_l2_payload.pending_resident_landings());
}

unsigned long long l2_cache::ms_frontend_blockers(mem_fetch *mf,
    const l2_access_plan &plan, bool response_full, unsigned long long &known) const {
  known = 0;
  if (!m_ms_observer.enabled) return 0;
  namespace M = ep_l2::mechanism_v1;
  unsigned long long blocked = 0;
  // This native path is reached only after the TVD frontend has declined to
  // handle the head. A previously observed paid victim lookup has completed;
  // do not accidentally extend its measured wait through unrelated admission.
  known |= 1ULL << M::VICTIM_LOOKUP;
  known |= 1ULL << M::READONLY_LOOKUP;
  if (plan.ep_l2_ro_lookup_wait) blocked |= 1ULL << M::READONLY_LOOKUP;
  known |= (1ULL << M::SAME_ADDRESS_WB) | (1ULL << M::FRONTEND_TRANSACTION) |
      (1ULL << M::COPY_PIPELINE);
  if (!plan.ep_l2_wad_same_address_hazard) known |= 1ULL << M::TAG_SET_PENDING;
  if (plan.ep_l2_wad_full || (plan.exact && plan.probe_status != RESERVATION_FAIL))
    known |= 1ULL << M::WAD_FULL;
  if (plan.exact && plan.probe_status != RESERVATION_FAIL)
    known |= (1ULL << M::LINE_MSHR_FULL) | (1ULL << M::DESCRIPTOR_FULL) |
        (1ULL << M::PER_LINE_CAP) | (1ULL << M::MISS_QUEUE_FULL) |
        (1ULL << M::RESPONSE_QUEUE_FULL);
  if (plan.ep_l2_tag_set_all_reserved) blocked |= 1ULL << M::TAG_SET_PENDING;
  if (plan.ep_l2_wad_full) blocked |= 1ULL << M::WAD_FULL;
  if (ep_l2_wad_contains(m_config.block_addr(mf->get_addr())))
    blocked |= 1ULL << M::SAME_ADDRESS_WB;
  if (plan.ep_l2_mshr_block_reason == mshr_table::EP_L2_BLOCK_LINE_MSHR_FULL)
    blocked |= 1ULL << M::LINE_MSHR_FULL;
  if (plan.ep_l2_mshr_block_reason == mshr_table::EP_L2_BLOCK_DESCRIPTOR_POOL_FULL)
    blocked |= 1ULL << M::DESCRIPTOR_FULL;
  if (plan.ep_l2_mshr_block_reason == mshr_table::EP_L2_BLOCK_PER_ADDRESS_CAP)
    blocked |= 1ULL << M::PER_LINE_CAP;
  if (!miss_queue_has_slots(plan.new_missq_entries))
    blocked |= 1ULL << M::MISS_QUEUE_FULL;
  if (plan.needs_immediate_response_slot && response_full)
    blocked |= 1ULL << M::RESPONSE_QUEUE_FULL;
  if (m_ft_pending.request && m_ft_pending.request != mf)
    blocked |= 1ULL << M::FRONTEND_TRANSACTION;
  if (plan.exact && plan.probe_status != RESERVATION_FAIL)
    known |= (1ULL << M::WB_DATA_SPACE) | (1ULL << M::BANK_COMPATIBLE_LANDING);
  if (plan.cache_index < m_config.get_num_lines()) {
    known |= 1ULL << M::PINNED_PAYLOAD;
    const ep_l2::PayloadHandle victim = tag_payload_handle(plan.cache_index);
    const bool live = m_ep_l2_payload.handle_live(victim);
    if (live && (plan.probe_status == MISS || mf->get_is_write() || mf->isatomic()) &&
        (m_ep_l2_payload.inspect(victim).pins ||
         (plan.probe_status == MISS && m_ep_l2_payload.inspect(victim).pending_sector_mask.any())))
      blocked |= 1ULL << M::PINNED_PAYLOAD;
    // A held transaction already owns its staging/landing. Its own
    // reservation must not be misreported as a fresh capacity refusal.
    if (plan.probe_status == MISS && !m_ft_pending.request) {
      if (plan.victim_dirty && m_config.m_ep_l2_ft_mode == 1 &&
          !m_ep_l2_payload.preview_free(ep_l2_payload_store::WB).valid())
        blocked |= 1ULL << M::WB_DATA_SPACE;
      if (m_config.m_ep_l2_ft_mode == 2 && (plan.victim_dirty || !live) &&
          !m_ep_l2_payload.preview_free(ep_l2_payload_store::RESIDENT, plan.cache_index % 4).valid())
        blocked |= 1ULL << M::BANK_COMPATIBLE_LANDING;
    }
  }
  return blocked;
}

void l2_cache::commit_next_access() {
  if(m_ro && m_ro->selected){m_ro->commit(ft_physical_cycle());return;}
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
  if(m_ro){m_ro->native_response_chosen=false;m_ro->prefer_ro=true;}
  ++m_ft_stats.descriptor_responses;
}

void l2_cache::ft_rollback() {
  ft_replacement &p = m_ft_pending;
  assert(p.request && !p.bound);
  m_ep_l2_payload.cancel_transaction(p.request->get_request_uid());
  if (p.quota_reserved) {
    assert(m_ft_quota.reserved == 1); --m_ft_quota.reserved;
    p.quota_reserved = false; ++m_ft_quota.rollbacks;
  }
  if (p.wad) {
    if (m_tvd) {
      assert(m_tvd->rollback_staged(p.wad_ticket, ft_physical_cycle())==tvd_directory::OK);
      ++m_tvd_indexed_rollbacks;
    }
    else assert(m_ep_l2_wad_entries_live.erase(p.victim_address)==1);
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
  ft_assert_quota();
}

enum cache_request_status l2_cache::ft_access(new_addr_type addr, mem_fetch *mf,
    unsigned time, std::list<cache_event> &events) {
  ep_l2_detach_diag::commit_scope diagnostic(this,mf->get_request_uid(),ft_physical_cycle());
  using namespace ep_l2_detach_diag;
  // memcpy offset orders native Tag timestamps; it is not elapsed hardware
  // time and must never reset the payload bank arbiter or WB/WAD lifetimes.
  ft_assert_quota();
  if (m_ft_pending.request == mf && m_ft_pending.quota_reserved) ++m_ft_quota.retries;
  const unsigned long long physical_cycle = ft_physical_cycle();
  m_ep_l2_last_payload_request_result = ep_l2_payload_store::GRANTED;
  const new_addr_type owner = m_config.block_addr(addr);
  if(m_ro) {
    if(m_ro->native_owner_conflict(mf)){if(diagnostic.d)diagnostic.d->add(commit_native_owner);return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_owner));}
    cache_request_status merged;
    ep_l2_detach_diag::field observed_merge_denial=ep_l2_detach_diag::denied_other;
    if(m_ro->merge_existing(mf,time,events,merged,diagnostic.d?&observed_merge_denial:nullptr)){
      if(merged!=RESERVATION_FAIL){++m_ft_stats.accepted;
        m_stats.inc_stats(mf->get_access_type(),MSHR_HIT,mf->get_streamID());
        m_stats.inc_stats_pw(mf->get_access_type(),MSHR_HIT,mf->get_streamID());
      }
      if(diagnostic.d)diagnostic.d->add(commit_merge);return cache_request_status(diagnostic.finish(merged,merged!=RESERVATION_FAIL,observed_merge_denial));
    }
  }
  const bool ro_new=m_ro && m_ro->new_request(mf);
  if(ro_new && !m_ro->directory_ready(mf)){if(diagnostic.d)diagnostic.d->add(commit_directory);return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_directory));}
  if (tvd_write_candidate(mf) && !tvd_promote(mf,time,events)) {if(diagnostic.d)diagnostic.d->add(commit_promotion);return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_promotion));}
  unsigned index = (unsigned)-1;
  cache_request_status probe = m_tag_array->probe(owner, index, mf, mf->is_write(), true);
  if(diagnostic.d){diagnostic.d->add(commit_raw_probe_reached);if(probe==RESERVATION_FAIL)diagnostic.d->add(raw_tag_fail_commit);}
  if(probe==RESERVATION_FAIL && m_ro){m_ro->relieve(addr);probe=m_tag_array->probe(owner,index,mf,mf->is_write(),true);if(diagnostic.d)diagnostic.d->add(probe>=0&&probe<=WRITE_ALLOCATED?field(post_relief_hit+probe):post_relief_unknown);}
  if (probe == RESERVATION_FAIL) return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_tag));
  if (m_tvd && probe == MISS &&
      !(m_tvd_lookup == mf && m_tvd_lookup_done)) return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_directory));
  // Capture each original short-circuit result without extra modeled checks.
  bool diagnostic_capacity_ok=false;
  if(ro_new && (!(diagnostic_capacity_ok=m_ro->capacity(mf,false)) || !miss_queue_has_slots(mf->get_access_sector_mask().count()+(probe==MISS && m_tag_array->block_is_modified(index)))))return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,diagnostic_capacity_ok?denied_missq:denied_capacity));
  if (m_ft_pending.request && m_ft_pending.request != mf) return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_held));
  if (m_tvd ? tvd_cached_wad_contains(mf,owner) : ep_l2_wad_contains(owner)) {
    ++m_ep_l2_wad_same_address_wait_count; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_wad));
  }

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
      ++m_ft_stats.pin_block; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_pin));
    }
    assert(!dirty || live);
    // Compute the actual target policy's issue needs before acquiring anything.
    const bool read = !mf->get_is_write();
    const new_addr_type key = m_config.mshr_addr(addr);
    unsigned slots = dirty ? 1 : 0;
    if(ro_new) {slots+=mf->get_access_sector_mask().count();}
    else if (read) {
      if (m_mshrs.full(key)) return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_mshr));
      if (m_mshrs.needs_lower_read(key, mf->get_access_sector_mask())) ++slots;
    } else if (m_config.m_write_alloc_policy == WRITE_ALLOCATE) {
      if (m_mshrs.full(key)) return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_mshr));
      slots += 2;
    } else if (m_config.m_write_alloc_policy == FETCH_ON_WRITE &&
               mf->get_access_byte_mask().count() != m_config.get_atom_sz()) {
      if (m_mshrs.full(key)) return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_mshr));
      ++slots;
    }
    if (!miss_queue_has_slots(slots)) { ++m_ft_stats.issue_block; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_missq)); }
    if (dirty && (m_tvd ? m_tvd->preview_install(m_tag_array->get_block(index)->m_block_addr,true,true).status != tvd_directory::OK : m_ep_l2_wad_entries_live.size() >= m_config.m_ep_l2_wad_entries)) {
      ++m_ep_l2_wad_full_block_count; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_wad_full));
    }
    ep_l2::PayloadHandle staging, landing;
    if (dirty && m_config.m_ep_l2_ft_mode == 1) {
      staging = m_ep_l2_payload.preview_free(ep_l2_payload_store::WB);
      if (!staging.valid()) { ++m_ft_stats.wbuf_full; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_wb)); }
    }
    bool retain_clean = false;
    if (m_tvd && m_config.m_ep_l2_tvd_clean && live && !dirty &&
        m_ep_l2_payload.inspect(victim).ready_sector_mask.any() &&
        m_tvd->preview_install(m_ep_l2_payload.inspect(victim).owner,false,true).status==tvd_directory::OK) {
      bool reserved=true;
      if(m_ep_l2_payload.fixed_victim_entries()) {
        reserved=m_ep_l2_payload.reserve_fixed_retention(victim);
        if(!reserved && tvd_reclaim_one(index%4,physical_cycle))
          reserved=m_ep_l2_payload.reserve_fixed_retention(victim);
      }
      if(reserved) {
        landing=m_ep_l2_payload.preview_free(ep_l2_payload_store::RESIDENT,index%4);
        retain_clean=landing.valid();
        if(!retain_clean && m_ep_l2_payload.fixed_victim_entries())
          m_ep_l2_payload.cancel_fixed_retention(victim);
      }
    }
    if (m_config.m_ep_l2_ft_mode == 2 && (dirty || !live)) {
      landing = m_ep_l2_payload.preview_free(ep_l2_payload_store::RESIDENT, index % 4);
      if (!landing.valid() && m_tvd && tvd_reclaim_one(index%4,physical_cycle))
        landing=m_ep_l2_payload.preview_free(ep_l2_payload_store::RESIDENT,index%4);
      if (!landing.valid()) { ++m_ft_stats.pool_full; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_landing)); }
      if (dirty) staging = victim;
    }
    // All issue/WAD/staging/landing prechecks passed. Reserve quota before
    // exposing any cross-cycle transaction; retries keep this one token.
    if (dirty && m_config.m_ep_l2_wb_local_cap &&
        m_ft_quota.reserved + m_ft_quota.live >= m_config.m_ep_l2_wb_local_cap) {
      ++m_ft_quota.refusals; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_quota));
    }
    // The only producer is the serialized frontend head. While this token is
    // live no other frontend request can steal its prechecked issue slots;
    // fills and lower drains cannot add entries to m_miss_queue.
    ft_replacement &p = m_ft_pending;
    p.request = mf; p.tag_index = index; p.dirty = dirty; p.retain_clean=retain_clean; p.victim = live ? victim : ep_l2::PayloadHandle();
    p.born = physical_cycle;
    p.landing_id = landing.valid() ? landing.payload_id :
        (m_config.m_ep_l2_ft_mode == 2 && live ? victim.payload_id : index);
    if (live) p.saved = m_ep_l2_payload.inspect(victim);
    if (dirty) {
      if (m_config.m_ep_l2_wb_local_cap) {
        assert(!m_ft_quota.reserved); ++m_ft_quota.reserved; p.quota_reserved = true;
        m_ft_quota.reserved_peak = std::max(m_ft_quota.reserved_peak, m_ft_quota.reserved);
        m_ft_quota.total_peak = std::max(m_ft_quota.total_peak, m_ft_quota.reserved + m_ft_quota.live);
      }
      p.victim_address = m_tag_array->get_block(index)->m_block_addr;
      if (m_tvd) {
        const auto dirty_mask=m_tag_array->get_block(index)->get_dirty_sector_mask().to_ulong();
        const auto ready_mask=p.saved.ready_sector_mask.to_ulong();
        const bool retain=m_config.m_ep_l2_tvd_dirty && (dirty_mask & ~ready_mask)==0;
        assert(tvd_install_victim(p.victim_address,retain?victim:ep_l2::PayloadHandle(),
            retain?ready_mask:dirty_mask,dirty_mask,true,physical_cycle,&p.wad_ticket));
      } else assert(m_ep_l2_wad_entries_live.insert(p.victim_address).second);
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
            false, physical_cycle, (unsigned long long)-1, ep_l2_payload_store::WB_READOUT,ep_l2_payload_store::COPY_READ,mf->get_request_uid());
        if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED) {
          ++m_ft_stats.bank_block; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_bank));
        }
        p.born = physical_cycle; // actual source readout starts data-staging lifetime
        p.copy_versions = m_ep_l2_payload.inspect(p.victim).versions;
        p.phase = 1;
        // COPY_FREE explicitly removes copy service and its staging delay.
        // DATA_FREE removes port contention but retains this fixed one-cycle
        // copy-latch transfer, even when source and target banks differ.
        if (!m_config.m_ep_l2_diag_copy_free) return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_copy));
      }
      if (p.phase == 1) {
        m_ep_l2_last_payload_request_result = m_ep_l2_payload.request(p.staging,
            true, physical_cycle, (unsigned long long)-1, ep_l2_payload_store::RESIDENT_WRITE,ep_l2_payload_store::COPY_WRITE,mf->get_request_uid());
        if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED) {
          ++m_ft_stats.bank_block; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_bank));
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
          (unsigned long long)-1, ep_l2_payload_store::RESIDENT_WRITE,ep_l2_payload_store::FRONTEND,mf->get_request_uid());
      if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED) {
        ++m_ft_stats.bank_block; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_bank));
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
      ++m_ft_stats.pin_block; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_pin));
    }
    if (probe == HIT || mf->get_is_write()) {
      m_ep_l2_last_payload_request_result = m_ep_l2_payload.request(h,
          mf->get_is_write() || mf->isatomic(), physical_cycle, (unsigned long long)-1,
          mf->get_is_write() || mf->isatomic() ? ep_l2_payload_store::RESIDENT_WRITE : ep_l2_payload_store::RESIDENT_HIT_READ,
          ep_l2_payload_store::FRONTEND,mf->get_request_uid());
      if (m_ep_l2_last_payload_request_result != ep_l2_payload_store::GRANTED) {
        ++m_ft_stats.bank_block; return cache_request_status(diagnostic.finish(RESERVATION_FAIL,false,denied_bank));
      }
    }
  }
  const ep_l2::PayloadHandle handle = tag_payload_handle(index);
  mf->set_ep_l2_payload_identity(handle.payload_id, handle.generation);
  const cache_request_status result = ro_new ? m_ro->accept_new(mf,index,probe,time,events) : data_cache::access(addr, mf, time, events);
  if (result == RESERVATION_FAIL) {
    if (replacement) ft_rollback();
    return cache_request_status(diagnostic.finish(result,false,denied_baseline));
  }
  if (replacement && m_ft_pending.dirty) assert(m_ft_pending.bound);
  if (replacement && m_ft_pending.retain_clean) {
    auto &p=m_ft_pending;
    if (tvd_install_victim(p.saved.owner,p.victim,p.saved.ready_sector_mask.to_ulong(),0,false,physical_cycle))
      m_ep_l2_payload.transfer(p.victim,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_VALID);
    else m_ep_l2_payload.release(p.victim);
  }
  ep_l2_payload_store::slot &slot = m_ep_l2_payload.inspect_mutable(handle);
  if (mf->get_is_write() || (mf->isatomic() && probe == HIT)) {
    for (unsigned s = 0; s < 4; ++s)
      if (mf->get_access_sector_mask().test(s)) slot.versions[s] = (unsigned long long)mf->get_request_uid() + 1;
    if (mf->get_access_byte_mask().count() == m_config.get_atom_sz() || probe == HIT)
      slot.ready_sector_mask |= mf->get_access_sector_mask();
  }
  if (m_tag_array->block_is_modified(index)) slot.status = ep_l2_payload_store::RESIDENT_DIRTY;
  else if (slot.pending_sector_mask.none() && slot.allocation_role!=ep_l2_payload_store::RO) slot.status = ep_l2_payload_store::RESIDENT_VALID;
  if (!mf->get_is_write()) mf->set_ft_wb_data(slot.versions);
  if (replacement) m_ft_pending = ft_replacement();
  if (m_tvd_lookup==mf) m_tvd_lookup=NULL;
  ++m_ft_stats.accepted;
  m_tag_array->l2_char_tracking_refresh_line(index);
  assert(m_ep_l2_payload.ownership_consistent());
  return cache_request_status(diagnostic.finish(result,true,denied_other));
}

void l2_cache::send_write_request(mem_fetch *mf, cache_event event, unsigned time,
    std::list<cache_event> &events) {
  if (ft_enabled() && mf->get_access_type() == L2_WRBK_ACC) {
    ft_replacement &p = m_ft_pending;
    assert(p.request && p.dirty && p.wad && !p.bound);
    assert(m_config.block_addr(mf->get_addr()) == p.victim_address);
    ft_wb_record r; r.handle = p.staging; r.born = p.born;
    if (m_tvd) {
      assert(m_tvd->live(p.wad_ticket) &&
             m_tvd->at(p.wad_ticket.index).line == p.victim_address);
      r.wad_ticket = p.wad_ticket;
      // Reuse the packet's existing UID. No directory ticket/header bits are
      // transmitted through the frozen ICNT/DRAM model.
      assert(m_tvd->bind_writeback(p.wad_ticket,mf->get_request_uid())==tvd_directory::OK);
    }
    r.expected = m_ep_l2_payload.inspect(p.staging).versions;
    assert(m_ft_wb.size() < m_config.m_ep_l2_wad_entries);
    assert(m_ft_wb.insert(std::make_pair(mf, r)).second);
    mf->set_ep_l2_payload_identity(p.staging.payload_id, p.staging.generation);
    if (m_config.m_ep_l2_wb_local_cap) {
      assert(p.quota_reserved && m_ft_quota.reserved == 1);
      --m_ft_quota.reserved; ++m_ft_quota.live; p.quota_reserved = false;
      m_ft_quota.live_peak = std::max(m_ft_quota.live_peak, m_ft_quota.live);
    }
    p.bound = true; ++m_ft_stats.wb_created;
    ft_assert_quota();
    m_ft_stats.staged_max = std::max(m_ft_stats.staged_max, (unsigned long long)m_ft_wb.size());
  }
  data_cache::send_write_request(mf, event, time, events);
}

bool l2_cache::lower_mq2_safe(mem_fetch *head, mem_fetch *next) {
  if (!m_config.m_ep_l2_mq2 || !ft_enabled() ||
      head->get_access_type() != L2_WRBK_ACC || !m_ft_wb.count(head) ||
      next->get_type() != READ_REQUEST || next->get_is_write() ||
      next->get_access_type() != GLOBAL_ACC_R || next->isatomic() ||
      next->get_inst().empty() || !next->get_inst().ep_l2_mq2_ordinary_read ||
      next->get_inst().cache_op == CACHE_VOLATILE ||
      !next->get_inst().is_load() || next->get_inst().m_is_ldgsts ||
      next->get_original_wr_mf() ||
      m_config.block_addr(head->get_addr()) == m_config.block_addr(next->get_addr()))
    return false;
  // Native MSHR admission owns this exact descriptor/generation; write
  // allocation and unknown/synthetic requests cannot acquire this proof.
  auto it=m_ft_read_refs.find(next);
  const bool safe=it!=m_ft_read_refs.end() && next->has_ep_l2_payload_identity() &&
      it->second.payload_id == next->get_ep_l2_payload_id() &&
      it->second.generation == next->get_ep_l2_payload_generation() &&
      m_ep_l2_payload.handle_owner_matches(it->second,m_config.block_addr(next->get_addr()));
  if(safe)++m_ft_stats.mq2_bypasses;
  return safe;
}

bool l2_cache::lower_ready(mem_fetch *mf, unsigned long long cycle) {
  if (!ft_enabled() || mf->get_access_type() != L2_WRBK_ACC) return true;
  std::map<mem_fetch *,ft_wb_record>::iterator it = m_ft_wb.find(mf);
  assert(it != m_ft_wb.end());
  const ep_l2_payload_store::request_result result = m_ep_l2_payload.request(
      it->second.handle, false, cycle, (unsigned long long)-1, ep_l2_payload_store::WB_READOUT,ep_l2_payload_store::LOWER_WB,mf->get_request_uid());
  if (result != ep_l2_payload_store::GRANTED) { ++m_ft_stats.bank_block;
    if(m_config.m_ep_l2_mq2)++m_ft_stats.mq2_wb_bank_waits;
    return false; }
  assert(m_ep_l2_payload.inspect(it->second.handle).versions == it->second.expected);
  m_ft_last_wb_versions = it->second.expected;
  mf->set_ft_wb_data(it->second.expected);
  ++m_ft_stats.staging_reads; ++m_ft_stats.data_checks;
  return true;
}

void l2_cache::lower_accepted(mem_fetch *mf, unsigned long long cycle) {
  if (ft_enabled()) m_ms_observer.lower_accept(mf->get_is_write(),
      mf->get_access_type() == L2_WRBK_ACC, mf->get_data_size());
  if (ft_enabled() && !mf->get_is_write()) {
    ++m_ft_stats.lower_read_packets; m_ft_stats.lower_read_bytes+=mf->get_data_size();
  }
  if(mf->ro_lower_packet()) {if(!m_ro)throw std::logic_error("RO packet without runtime");m_ro->tx->lower_accepted(ep_l2::PayloadHandle(mf->get_ep_l2_payload_id(),mf->get_ep_l2_payload_generation()),mf->get_access_sector_mask().to_ulong());return;}
  if (!ft_enabled() || mf->get_access_type() != L2_WRBK_ACC) return;
  std::map<mem_fetch *,ft_wb_record>::iterator it = m_ft_wb.find(mf);
  assert(it != m_ft_wb.end());
  assert(mf->has_ft_wb_data() && mf->get_ft_wb_data() == it->second.expected);
  // The physical lower FIFO now carries this packet's dirty sector bytes;
  // all subsequent queues must retain data until actual set_done. No original
  // payload pointer is needed with TVD disabled. These FIFO bytes are ledgered.
  if (m_tvd) {
    const auto t=it->second.wad_ticket;
    assert(m_tvd->live(t) && m_tvd->at(t.index).line==m_config.block_addr(mf->get_addr()));
    assert(m_tvd->writeback_matches(t,m_config.block_addr(mf->get_addr()),mf->get_request_uid()));
    assert(m_tvd->lower_accept(t,true,cycle)==tvd_directory::OK);
    ++m_tvd_indexed_accepts;
    if (m_tvd->at(t.index).payload.valid()) {
      if (!m_ep_l2_payload.inspect(it->second.handle).pins)
        m_ep_l2_payload.transfer(it->second.handle,ep_l2_payload_store::TVD,ep_l2_payload_store::TVD_INFLIGHT);
      // Else retain WB role while a paid reader pin protects its retry key.
    }
    else m_ep_l2_payload.release(it->second.handle);
  } else m_ep_l2_payload.release(it->second.handle);
  m_ft_stats.staging_lifetime += cycle >= it->second.born ? cycle - it->second.born : 0;
  ++m_ft_stats.lower_accept;
  if (m_config.m_ep_l2_wb_local_cap) {
    assert(m_ft_quota.live); --m_ft_quota.live; ++m_ft_quota.lower_accept;
  }
  m_ft_wb.erase(it);
  ft_assert_quota();
}

void l2_cache::ft_print(FILE *fp, unsigned slice) const {
  if (!ft_enabled()) return;
  ft_assert_quota();
  m_ms_observer.print(fp, slice);
  fprintf(fp, "EPL2MSV1|LOCAL_DIAGNOSTIC|slice=%u|copy_free=%u|l2_data_free=%u|logical_copy_reads=%llu|logical_copy_writes=%llu|exempt_response=%llu|exempt_fill=%llu|exempt_lower_wb=%llu|exempt_frontend=%llu|exempt_copy_read=%llu|exempt_copy_write=%llu|physical_grants=%llu|hardware_P1_throughput_eligible=%u\n",
      slice, m_config.m_ep_l2_diag_copy_free, m_config.m_ep_l2_diag_l2_data_free,
      m_ep_l2_payload.logical_copy_reads(), m_ep_l2_payload.logical_copy_writes(),
      m_ep_l2_payload.diagnostic_services(ep_l2_payload_store::RESPONSE),
      m_ep_l2_payload.diagnostic_services(ep_l2_payload_store::FILL),
      m_ep_l2_payload.diagnostic_services(ep_l2_payload_store::LOWER_WB),
      m_ep_l2_payload.diagnostic_services(ep_l2_payload_store::FRONTEND),
      m_ep_l2_payload.diagnostic_services(ep_l2_payload_store::COPY_READ),
      m_ep_l2_payload.diagnostic_services(ep_l2_payload_store::COPY_WRITE),
      m_ep_l2_payload.bank_grants(),
      !(m_config.m_ep_l2_diag_copy_free || m_config.m_ep_l2_diag_l2_data_free));
  m_ep_l2_payload.rg1_print(fp,slice);
  fprintf(fp,"EPL2SERVICEV1|MQ2|slice=%u|enabled=%u|wb_bank_waits=%llu|bypasses=%llu|window=2|lower_enqueues_per_cycle=1\n",slice,m_config.m_ep_l2_mq2,m_ft_stats.mq2_wb_bank_waits,m_ft_stats.mq2_bypasses);
  fprintf(fp,"EPL2STRUCTV1|BANK_PEAK|slice=%u|ops_per_physical_cycle=%u|bank0=%u|bank1=%u|bank2=%u|bank3=%u\n",slice,
      m_ep_l2_payload.bank_service_limit(),m_ep_l2_payload.bank_peak(0),m_ep_l2_payload.bank_peak(1),
      m_ep_l2_payload.bank_peak(2),m_ep_l2_payload.bank_peak(3));
  fprintf(fp, "EPL2STRUCTV1|WB_QUOTA|slice=%u|cap=%u|reserved=%u|live=%u|reserved_peak=%u|live_peak=%u|total_peak=%u|refusal_attempts=%llu|pending_retry_calls=%llu|rollback_releases=%llu|lower_accept_releases=%llu\n",
      slice, m_config.m_ep_l2_wb_local_cap, m_ft_quota.reserved, m_ft_quota.live,
      m_ft_quota.reserved_peak, m_ft_quota.live_peak, m_ft_quota.total_peak,
      m_ft_quota.refusals, m_ft_quota.retries, m_ft_quota.rollbacks, m_ft_quota.lower_accept);
  fprintf(fp, "EPL2STRUCTV1|CONFIG|slice=%u|static_s64=%u|physical_slots=%u|"
      "resident_nominal=1024|bypass_capacity=%u|wb_begin=%u|static_wb_capacity=%u|"
      "legacy_wbuf=%u|bank_count=4|bank_ops_per_physical_cycle=%u|copy_latch_bytes=128\n",
      slice, m_config.m_ep_l2_static_s64 ? 1 : 0, m_ep_l2_payload.physical_slots(),
      m_ep_l2_payload.bypass_capacity(), m_ep_l2_payload.wb_begin(),
      m_ep_l2_payload.static_wb_capacity(), m_config.m_ep_l2_wbuf_entries, m_ep_l2_payload.bank_service_limit());
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
  if (m_tvd) const_cast<l2_cache *>(this)->tvd_print(fp,slice);
}
