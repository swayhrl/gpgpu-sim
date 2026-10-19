"""Static victim/non-victim capacity partition; mutate proposed strings only."""


def extend(text):
    def replace(name, before, after, count=1):
        path = 'src/gpgpu-sim/' + name
        assert text[path].count(before) == count, (path, before[:90], text[path].count(before))
        text[path] = text[path].replace(before, after)

    h = 'gpu-cache.h'
    replace(h, '    m_ep_l2_tvd_clean = m_ep_l2_tvd_dirty = false;',
            '    m_ep_l2_tvd_clean = m_ep_l2_tvd_dirty = false;\n    m_ep_l2_tvd_fixed_entries=0;')
    replace(h, '  bool m_ep_l2_tvd_clean, m_ep_l2_tvd_dirty;',
            '  bool m_ep_l2_tvd_clean, m_ep_l2_tvd_dirty;\n  unsigned m_ep_l2_tvd_fixed_entries;')
    replace(h, '  void set_tvd_identity(bool enabled)',
            '  #include "ep-l2-tvd-fixed-pool.inc"\n  void set_tvd_identity(bool enabled)')
    replace(h, '    m_ep_l2_payload.set_tvd_identity(config.m_ep_l2_tvd_dirty);',
            '    m_ep_l2_payload.set_tvd_identity(config.m_ep_l2_tvd_dirty);\n    m_ep_l2_payload.set_fixed_victim_entries(config.m_ep_l2_tvd_fixed_entries);')
    replace(h, '      if (m_slots[id].occupied() || (bank != (unsigned)-1 && id % 4 != bank)) continue;',
            '      if (m_slots[id].occupied() || (bank != (unsigned)-1 && id % 4 != bank)) continue;\n      if (!fixed_role_room(r,id%4)) continue;')
    replace(h, '    assert(m_ft_mode && id < m_slots.size() && !m_slots[id].occupied());',
            '    assert(m_ft_mode && id < m_slots.size() && !m_slots[id].occupied());\n    assert(fixed_role_room(r,id%4));')
    replace(h, '    s.status = FREE; s.owner = 0; s.ready_sector_mask.reset(); ++s.generation;',
            '''    if(m_fixed_retention==h)m_fixed_retention=payload_handle();
    s.status = FREE; s.owner = 0; s.ready_sector_mask.reset(); ++s.generation;
    assert(fixed_quota_consistent());''')
    replace(h, '    s.allocation_role = r; s.status = st;',
            '''    if(m_fixed_victim_entries && ((r==TVD)!=(s.allocation_role==TVD)) &&
        !(m_fixed_retention==h && r==TVD))assert(fixed_role_room(r,h.payload_id%4));
    s.allocation_role = r; s.status = st;
    if(m_fixed_retention==h && r==TVD)m_fixed_retention=payload_handle();
    assert(fixed_quota_consistent());''')
    replace(h, '  void restore(unsigned id, const slot &saved) { assert(id < m_slots.size()); m_slots[id] = saved; }',
            '''  void restore(unsigned id, const slot &saved) {
    assert(id<m_slots.size());
    if(m_fixed_retention.valid() && m_fixed_retention.payload_id==id)m_fixed_retention=payload_handle();
    m_slots[id]=saved;assert(fixed_quota_consistent());
  }''')
    replace(h, '    assert(id < k_resident_slots); m_slots[id] = saved;',
            '    assert(id < k_resident_slots); restore(id,saved);')
    # Legacy fixed-index allocation APIs must enforce the same reciprocal cap;
    # ordinary FT uses preview_free/reserve. No alternate allocator evades it.
    replace(h, '    assign(m_slots[id], id, owner, mf, RESIDENT_FILL_PENDING);',
            '''    if(!m_slots[id].occupied() || m_slots[id].allocation_role==TVD)
      assert(fixed_role_room(RESIDENT,id%4));
    assign(m_slots[id], id, owner, mf, RESIDENT_FILL_PENDING);
    assert(fixed_quota_consistent());''')
    replace(h, '    assert(id < bypass_capacity()); assign(m_slots[k_resident_slots + id], k_resident_slots + id, owner, mf, BYPASS_FILL_PENDING);',
            '''    assert(id < bypass_capacity());
    if(!m_slots[k_resident_slots+id].occupied() || m_slots[k_resident_slots+id].allocation_role==TVD)
      assert(fixed_role_room(BYPASS,(k_resident_slots+id)%4));
    assign(m_slots[k_resident_slots+id],k_resident_slots+id,owner,mf,BYPASS_FILL_PENDING);
    assert(fixed_quota_consistent());''')
    replace(h, '  void validate_ep_l2_mode_config() const {',
            '''  void validate_ep_l2_mode_config() const {
    if(m_config.m_ep_l2_tvd_fixed_entries &&
        (m_config.m_ep_l2_tvd_fixed_entries!=64 || !m_config.m_ep_l2_tvd_clean || m_config.m_ep_l2_tvd_dirty)) {
      fprintf(stderr,"FIXED64 requires CLEAN_ONLY; no dirty/diagnostic combined qualification\\n");abort();
    }''')
    f = 'ep-l2-ft-cache.inc'
    replace(f, '''      landing=m_ep_l2_payload.preview_free(ep_l2_payload_store::RESIDENT,index%4);
      retain_clean=landing.valid();''',
            '''      bool reserved=true;
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
      }''')
    replace('gpu-sim.cc', '  option_parser_register(opp,"-gpgpu_ep_l2_tvd_clean",OPT_BOOL,',
            '''  option_parser_register(opp,"-gpgpu_ep_l2_tvd_fixed_entries",OPT_UINT32,
      &m_L2_config.m_ep_l2_tvd_fixed_entries,"Fixed victim/non-victim bank quotas:0 dynamic,64 fixed (1168 total)","0");
  option_parser_register(opp,"-gpgpu_ep_l2_tvd_clean",OPT_BOOL,''')
