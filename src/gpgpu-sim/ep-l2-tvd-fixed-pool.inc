// Included inside ep_l2_payload_store by A's adapter. No data array is added.
// A fixed CAPACITY partition, not fixed physical IDs: both buckets use the
// parent's pointer mapping and same-bank placement. Unused victim quota is
// never lent to any other role. One replacement credit is charged to that
// bucket while the old clean resident remains visible to its locked Tag.
public:
  void set_fixed_victim_entries(unsigned n) {
    assert(!n || (n==64 && m_mode==2 && m_ft_mode==2 && m_bank_ops==1 &&
                  m_wbuf==16 && !m_static_s64 && !m_ideal_wb));
    assert(free_slots()==physical_slots());m_fixed_victim_entries=n;
  }
  unsigned fixed_victim_entries() const { return m_fixed_victim_entries; }
  bool fixed_retention_reserved() const { return m_fixed_retention.valid(); }
  unsigned fixed_charged_victims(unsigned bank) const {
    assert(bank<4);unsigned n=0;
    for(unsigned i=bank;i<m_slots.size();i+=4)
      n+=m_slots[i].occupied() && m_slots[i].allocation_role==TVD;
    if(fixed_credit_live(bank)) ++n;
    return n;
  }
  unsigned fixed_charged_other(unsigned bank) const {
    assert(bank<4);unsigned n=0;
    for(unsigned i=bank;i<m_slots.size();i+=4)
      n+=m_slots[i].occupied() && m_slots[i].allocation_role!=TVD;
    if(fixed_credit_live(bank)) {assert(n);--n;}
    return n;
  }
  bool fixed_role_room(role r,unsigned bank) const {
    if(!m_fixed_victim_entries)return true;
    return r==TVD ? fixed_charged_victims(bank)<m_fixed_victim_entries/4 :
        fixed_charged_other(bank)<physical_slots()/4-m_fixed_victim_entries/4;
  }
  bool fixed_quota_consistent() const {
    if(!m_fixed_victim_entries)return true;
    for(unsigned b=0;b<4;++b)
      if(fixed_charged_victims(b)>m_fixed_victim_entries/4 ||
          fixed_charged_other(b)>physical_slots()/4-m_fixed_victim_entries/4)return false;
    return !m_fixed_retention.valid() || handle_live(m_fixed_retention);
  }
  bool reserve_fixed_retention(const payload_handle &h) {
    if(!m_fixed_victim_entries)return true;
    assert(!m_fixed_retention.valid());
    const auto &s=inspect(h);
    assert(s.allocation_role==RESIDENT && s.status==RESIDENT_VALID &&
           !s.pins && s.pending_sector_mask.none());
    if(!fixed_role_room(TVD,h.payload_id%4))return false;
    m_fixed_retention=h;
    assert(fixed_quota_consistent());return true;
  }
  void cancel_fixed_retention(const payload_handle &h) {
    if(!m_fixed_victim_entries)return;
    assert(m_fixed_retention==h);m_fixed_retention=payload_handle();
    assert(fixed_quota_consistent());
  }
  bool fixed_promotion_room(const payload_handle &source,
                            const payload_handle &clean_replaced) const {
    if(!m_fixed_victim_entries)return true;
    assert(!m_fixed_retention.valid());
    const unsigned bank=source.payload_id%4;
    unsigned other=fixed_charged_other(bank);
    if(clean_replaced.valid()) {
      const auto &s=inspect(clean_replaced);
      assert(clean_replaced.payload_id%4==bank && s.allocation_role==RESIDENT &&
             s.status==RESIDENT_VALID && !s.pins && s.pending_sector_mask.none());
      assert(other);--other;
    }
    return other<physical_slots()/4-m_fixed_victim_entries/4;
  }
private:
  bool fixed_credit_live(unsigned bank) const {
    return m_fixed_retention.valid() && m_fixed_retention.payload_id%4==bank &&
        handle_live(m_fixed_retention) &&
        m_slots[m_fixed_retention.payload_id].allocation_role!=TVD;
  }
  unsigned m_fixed_victim_entries=0;
  payload_handle m_fixed_retention;
public:
