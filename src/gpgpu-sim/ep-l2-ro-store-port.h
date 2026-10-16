#ifndef EP_L2_RO_STORE_PORT_H
#define EP_L2_RO_STORE_PORT_H
#include "ep-l2-ro-transactions.h"

namespace ep_l2_ro {
// Actual Tag callback is supplied by the production l2_cache runtime.
class tag_port {
 public:
  virtual ~tag_port() {}
  virtual bool matches(tag_binding,identity,new_addr_type) const = 0;
  virtual void ready(tag_binding,identity,new_addr_type,unsigned,bool) = 0;
  virtual void detach(tag_binding,identity) = 0;
  virtual void read(identity,unsigned,const std::array<unsigned long long,4>&) = 0;
};

// Executable binding against Lane-1 R0. Owns NO data array/allocator.
class store_port : public payload_port {
 public:
  store_port(ep_l2_payload_store &store,tag_port &tags)
      : m_store(store), m_tags(tags), m_return(NULL), m_return_mask(0) {
    if (!store.banked() || store.ft_mode()!=2 ||
        (store.physical_slots()!=1168 && store.physical_slots()!=1160))
      throw std::invalid_argument("RO requires unified banked C16/C8 store");
  }
  bool live(identity h,new_addr_type line) const {
    return m_store.handle_owner_matches(h,line);
  }
  bool pin(identity h) {
    if(!m_store.handle_live(h))return false;
    const auto &s=m_store.inspect(h);
    if(s.allocation_role==ep_l2_payload_store::RESIDENT && s.pending_sector_mask.none() && !s.pins && s.status!=ep_l2_payload_store::RESIDENT_DIRTY)
      m_store.transfer(h,ep_l2_payload_store::RO,ep_l2_payload_store::RO_FILL_ATTACHED);
    else if(s.allocation_role!=ep_l2_payload_store::RO)return false;
    m_store.pin(h);return true;
  }
  void unpin(identity h) { m_store.unpin(h); }
  void lower_accepted(identity h,unsigned mask) {
    auto &s=m_store.inspect_mutable(h);
    if ((s.pending_sector_mask.to_ulong()|s.ready_sector_mask.to_ulong())&mask)
      throw std::logic_error("RO lower sector already outstanding/ready");
    if(s.allocation_role==ep_l2_payload_store::RESIDENT)
      m_store.transfer(h,ep_l2_payload_store::RO,ep_l2_payload_store::RO_FILL_ATTACHED);
    s.pending_sector_mask|=mem_access_sector_mask_t(mask);
  }
  // Borrow the actual return packet's version tokens for one callback only.
  // A denied bank write leaves the packet at its existing lower return queue.
  bool accept_return(transactions &tx,identity h,unsigned mask,uint64_t cycle,
                     const std::array<unsigned long long,4> &packet_versions) {
    if(m_return)throw std::logic_error("nested RO return callback");
    m_return=&packet_versions;m_return_handle=h;m_return_mask=mask;
    try {bool accepted=tx.fill(h,mask,cycle);m_return=NULL;return accepted;}
    catch(...){m_return=NULL;throw;}
  }
  bool fill_write(identity h,unsigned mask,uint64_t cycle) {
    if(!m_return || !(h==m_return_handle) || mask!=m_return_mask)
      throw std::logic_error("RO fill requires bound return packet");
    const auto &before=m_store.inspect(h);
    if((mask&~before.pending_sector_mask.to_ulong())!=0)
      throw std::logic_error("RO return sector has no landing obligation");
    if(m_store.request(h,true,cycle,~0ULL,ep_l2_payload_store::RO_FILL_WRITE)!=ep_l2_payload_store::GRANTED)
      return false;
    auto &s=m_store.inspect_mutable(h);
    for(unsigned n=0;n<4;++n)if(mask>>n&1)s.versions[n]=(*m_return)[n];
    m_store.mark_sector_ready(h,mem_access_sector_mask_t(mask));return true;
  }
  bool response_read(identity h,unsigned mask,uint64_t cycle) {
    const auto &s=m_store.inspect(h);
    if((mask&~s.ready_sector_mask.to_ulong())!=0)
      throw std::logic_error("RO response requires all requested sectors");
    if(m_store.request(h,false,cycle,~0ULL,ep_l2_payload_store::RO_RESPONSE_READ)!=ep_l2_payload_store::GRANTED)
      return false;
    m_tags.read(h,mask,s.versions);return true;
  }
  bool tag_matches(tag_binding t,identity h,new_addr_type line) const {
    return m_tags.matches(t,h,line);
  }
  void tag_ready(tag_binding t,identity h,new_addr_type line,unsigned mask,bool complete) {
    if(complete)m_store.transfer(h,ep_l2_payload_store::RESIDENT,
                                ep_l2_payload_store::RESIDENT_VALID);
    m_tags.ready(t,h,line,mask,complete);
  }
  void tag_detach(tag_binding t,identity h) {
    auto &s=m_store.inspect_mutable(h);
    if(s.allocation_role!=ep_l2_payload_store::RO || !s.pins)
      throw std::logic_error("RO detach requires owned/pinned pending landing");
    m_tags.detach(t,h);s.status=ep_l2_payload_store::RO_FILL_DETACHED;
  }
  void release(identity h) { m_store.release(h); }
 private:
  ep_l2_payload_store &m_store;
  tag_port &m_tags;
  const std::array<unsigned long long,4> *m_return;
  identity m_return_handle;
  unsigned m_return_mask;
};
} // namespace ep_l2_ro
#endif
