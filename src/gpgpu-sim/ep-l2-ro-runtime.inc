// Included once by gpu-cache.cc. No separate payload/descriptor allocator.
#include "ep-l2-ro-store-port.h"
#include "ep-l2-ro-sha256.h"
#include <fstream>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
class ep_l2_ro_runtime : public ep_l2_ro::tag_port {
 public:
  typedef ep_l2_ro::identity handle;
  l2_cache &c;
  std::unique_ptr<ep_l2_ro::oracle_certificate> cert;
  ep_l2_ro::store_port port;
  std::unique_ptr<ep_l2_ro::transactions> tx;
  std::vector<uint64_t> generations;
  bool open, ever_open, selected, command_bound, prefer_ro;
  std::string command_sha;
  ep_l2_ro::response reply;
  uint64_t response_cycle;
  unsigned cursor;
  uint64_t accepted_global_reads, accepted_candidates, accepted_ro;
  ep_l2_ro_runtime(l2_cache &cache) : c(cache), port(c.m_ep_l2_payload,*this),
    generations(c.m_config.get_num_lines(),1),open(false),ever_open(false),selected(false),command_bound(false),prefer_ro(true),
    response_cycle(0),cursor(0),accepted_global_reads(0),accepted_candidates(0),accepted_ro(0) {
    const auto &cfg=c.m_config;
    if(cfg.get_line_sz()!=128 || cfg.get_atom_sz()!=32 || cfg.m_alloc_policy!=ON_MISS ||
       cfg.m_mshr_entries!=128 || cfg.m_ep_l2_descriptor_per_line_cap!=32)
      throw std::invalid_argument("RO requires reviewed sector128/32 LineMSHR128 D512/address32 geometry");
    if(!cfg.m_ro_ranges || !cfg.m_ro_ranges_sha256 || !cfg.m_ro_manifest_sha256 || !cfg.m_ro_scope)
      throw std::invalid_argument("RO input binding absent");
    std::ifstream f(cfg.m_ro_ranges,std::ios::binary);
    if(!f)throw std::invalid_argument("RO ranges cannot be opened");
    std::string bytes((std::istreambuf_iterator<char>(f)),{});
    if(ep_l2_ro::sha256(bytes)!=cfg.m_ro_ranges_sha256)
      throw std::invalid_argument("RO range SHA256 mismatch");
    if(!cfg.m_ro_manifest)throw std::invalid_argument("RO manifest missing");
    std::ifstream mf(cfg.m_ro_manifest,std::ios::binary);
    if(!mf)throw std::invalid_argument("RO manifest cannot be opened");
    std::string manifest((std::istreambuf_iterator<char>(mf)),{});
    if(ep_l2_ro::sha256(manifest)!=cfg.m_ro_manifest_sha256)throw std::invalid_argument("RO manifest SHA256 mismatch");
    boost::property_tree::ptree doc;std::istringstream json(manifest);
    boost::property_tree::read_json(json,doc);
    if(doc.get<std::string>("schema")!="EP_L2_RO_ORACLE_V1" ||
       doc.get<std::string>("epoch")!="whole_commandlist" ||
       doc.get<std::string>("scope")!="TRACE_MODEL_CLOSED_WORLD" ||
       doc.get<std::string>("address_domain")!="CANONICAL_GPU_GLOBAL_VA_NO_TRANSLATION_ALIAS" ||
       !doc.get_child("unresolved_opcodes").empty())throw std::invalid_argument("RO unsupported/incomplete epoch certificate");
    if(doc.get_child("artifacts").get<std::string>(boost::property_tree::ptree::path_type("certified_ranges.csv",'/'))!=cfg.m_ro_ranges_sha256)
      throw std::invalid_argument("RO ranges not bound to manifest");
    command_sha=doc.get<std::string>("commandlist_sha256");
    if(command_sha.size()!=64 || command_sha.find_first_not_of("0123456789abcdef")!=std::string::npos)
      throw std::invalid_argument("invalid RO command identity");
    std::istringstream ranges(bytes);
    cert.reset(new ep_l2_ro::oracle_certificate(ranges,cfg.m_ro_manifest_sha256,cfg.m_ro_scope));
    tx.reset(new ep_l2_ro::transactions(c.m_mshrs,port,c.m_ep_l2_payload.physical_slots(),*cert,
      ep_l2_ro::switches(cfg.m_ro_tag_detach,cfg.m_ro_line_mshr_bypass)));
  }
  uint64_t physical_cycle() const {
    return c.m_gpu->gpu_sim_cycle+c.m_gpu->gpu_tot_sim_cycle;
  }
  void prefill_begin() const {
    // The certified full application can start only once. A drained kernel
    // boundary is not permission to mutate its whole-commandlist certificate.
    if(ever_open || open || selected || !tx->drained())
      throw std::logic_error("RO host prefill is initialization-only before epoch activation");
  }
  void prefill_complete(unsigned index,handle h,new_addr_type owner,
                        mem_access_sector_mask_t mask) {
    prefill_begin();unsigned actual=~0U;
    const auto result=c.m_tag_array->probe(owner,actual,mask,true,true);
    if(result!=HIT || actual!=index || index>=generations.size() ||
       c.tag_payload_handle(index)!=h || !c.m_ep_l2_payload.handle_owner_matches(h,owner) ||
       h.payload_id%4!=index%4)
      throw std::logic_error("RO host prefill Tag/payload/bank identity mismatch");
    const auto &slot=c.m_ep_l2_payload.inspect(h);
    if(slot.allocation_role!=ep_l2_payload_store::RESIDENT || slot.pins ||
       slot.pending_sector_mask.any() || (mask&~slot.ready_sector_mask).any())
      throw std::logic_error("RO host prefill disturbed an active or incomplete owner");
    if(generations[index]==~uint64_t(0))throw std::logic_error("RO Tag generation wrap");
    ++generations[index];
  }
  bool demand(const mem_fetch *m) const {
    return m->get_access_type()==GLOBAL_ACC_R && !m->get_is_write() && !m->isatomic() && !m->ro_lower_packet();
  }
  bool candidate(const mem_fetch *m) const { return demand(m)&&cert->contains(c.m_config.block_addr(m->get_addr())); }
  void check_access(mem_fetch *m) const {
    if(!open)throw std::logic_error("RO access outside full-commandlist epoch");
    if(m->get_is_write()||m->isatomic())tx->assert_no_epoch_write(1,c.m_config.block_addr(m->get_addr()));
  }
  void record_accepted(mem_fetch *m) {
    if(demand(m))++accepted_global_reads;
    if(candidate(m))++accepted_candidates;
  }
  bool matches(ep_l2_ro::tag_binding t,handle h,new_addr_type line) const override {
    return t.index<generations.size() && t.generation==generations[t.index] &&
      c.tag_payload_handle(t.index)==h &&
      !c.m_tag_array->get_block(t.index)->is_invalid_line() &&
      c.m_tag_array->get_block(t.index)->m_block_addr==line;
  }
  void ready(ep_l2_ro::tag_binding t,handle h,new_addr_type line,unsigned mask,bool) override {
    if(!matches(t,h,line))throw std::logic_error("RO late fill into reused Tag");
    for(unsigned n=0;n<4;++n)if(mask>>n&1)
      c.m_tag_array->get_block(t.index)->set_status(VALID,mem_access_sector_mask_t(1<<n));
    c.m_tag_array->l2_char_tracking_refresh_line(t.index);
  }
  void detach(ep_l2_ro::tag_binding t,handle h) override {
    if(!matches(t,h,c.m_ep_l2_payload.inspect(h).owner))throw std::logic_error("RO detach stale Tag");
    for(unsigned n=0;n<4;++n)c.m_tag_array->get_block(t.index)->set_status(INVALID,mem_access_sector_mask_t(1<<n));
    c.set_tag_payload_handle(t.index,handle());++generations[t.index];
    c.m_tag_array->l2_char_tracking_refresh_line(t.index);
  }
  void read(handle h,unsigned,const std::array<unsigned long long,4>&v) override {
    // Values stay in common RAM until same-cycle enqueue; no response buffer.
    (void)h;(void)v;
  }
  bool target(mem_fetch *m,handle &h) const {
    unsigned i=~0U;
    auto p=c.m_tag_array->probe(c.m_config.block_addr(m->get_addr()),i,m,false,true);
    if(p!=MISS && p!=RESERVATION_FAIL) {
      h=c.tag_payload_handle(i);
      if(tx->live(h))return true;
    }
    // This is the existing native Line-MSHR index, never a completion CAM.
    return !c.m_config.m_ro_line_mshr_bypass &&
      c.m_mshrs.lookup_ro_line(c.m_config.block_addr(m->get_addr()),h);
  }
  bool new_request(mem_fetch *m) const {
    if(!candidate(m))return false;
    handle h;if(target(m,h))return true;
    unsigned i=~0U;auto p=c.m_tag_array->probe(c.m_config.block_addr(m->get_addr()),i,m,false,true);
    if(p!=MISS && p!=RESERVATION_FAIL) {
      const handle resident=c.tag_payload_handle(i);
      if(c.m_tag_array->block_is_modified(i) || (c.m_ep_l2_payload.handle_live(resident) &&
          (c.m_ep_l2_payload.inspect(resident).pending_sector_mask.any() || c.m_ep_l2_payload.inspect(resident).pins)))return false;
    }
    return p!=HIT;
  }
  unsigned needed(mem_fetch *m,handle h) const {
    const unsigned mask=m->get_access_sector_mask().to_ulong();
    return mask&~tx->scheduled(h);
  }
  bool capacity(mem_fetch *m,bool existing) const {
    const auto line=c.m_config.block_addr(m->get_addr());
    return c.m_mshrs.descriptor_count_used()<512 && c.m_mshrs.targets_on_line(line)<32 &&
      (existing || c.m_config.m_ro_line_mshr_bypass ||
       (!c.m_mshrs.probe(line) && c.m_mshrs.num_entries_used()<128));
  }
  bool detachable(new_addr_type addr,unsigned &index) const {
    if(!c.m_config.m_ro_tag_detach)return false;
    const unsigned base=c.m_config.set_index(addr)*c.m_config.get_assoc();
    for(unsigned i=base;i<base+c.m_config.get_assoc();++i) {
      handle h=c.tag_payload_handle(i);
      if(tx->live(h) && tx->can_detach(h)){index=i;return true;}
    }
    return false;
  }
  void relieve(new_addr_type addr) {
    unsigned i=~0U;if(detachable(addr,i))tx->detach(c.tag_payload_handle(i));
  }
  bool preview(mem_fetch *m,l2_access_plan &p) const {
    if(!new_request(m))return false;
    p.exact=true;p.is_read=true;
    handle h;bool existing=target(m,h);
    unsigned i=~0U;
    p.probe_status=c.m_tag_array->probe(c.m_config.block_addr(m->get_addr()),i,m,false,true);
    if(existing) {
      p.probe_status=HIT_RESERVED;
    } else if(p.probe_status==RESERVATION_FAIL && detachable(m->get_addr(),i))p.probe_status=MISS;
    p.cache_index=i;
    if(p.probe_status==RESERVATION_FAIL){p.ep_l2_tag_set_all_reserved=true;return true;}
    const unsigned mask=m->get_access_sector_mask().to_ulong();
    unsigned missing=existing?needed(m,h):mask;
    if(!existing && p.probe_status!=MISS) {
      auto resident=c.tag_payload_handle(i);
      if(c.m_ep_l2_payload.handle_live(resident))missing&=~c.m_ep_l2_payload.inspect(resident).ready_sector_mask.to_ulong();
    }
    if(!existing && p.probe_status==MISS) {
      p.victim_dirty=c.m_tag_array->block_is_modified(i);
      if(p.victim_dirty){p.new_missq_entries++;p.will_send_writeback=true;p.victim_modified_bytes=c.m_tag_array->block_modified_size(i);}
    }
    p.will_send_lower_read=missing!=0;
    p.new_missq_entries+=__builtin_popcount(missing);
    p.needs_new_mshr=!existing;p.needs_mshr_merge=existing;
    p.mshr_hit=existing;p.mshr_entry_available=p.mshr_merge_available=capacity(m,existing);
    if(c.ep_l2_wad_contains(c.m_config.block_addr(m->get_addr()))) {p.probe_status=RESERVATION_FAIL;p.ep_l2_wad_same_address_hazard=true;}
    return true;
  }
  void issue(mem_fetch *m,handle h,unsigned mask,unsigned /*tag_time*/,std::list<cache_event>&events) {
    const uint64_t cycle=physical_cycle();
    for(unsigned n=0;n<4;++n)if(mask>>n&1) {
      mem_access_byte_mask_t bytes;for(unsigned b=n*32;b<n*32+32;++b)bytes.set(b);
      auto packet=c.m_memfetch_creator->alloc(c.m_config.block_addr(m->get_addr())+32*n,GLOBAL_ACC_R,
        m->get_access_warp_mask(),bytes,mem_access_sector_mask_t(1<<n),32,false,cycle,-1,-1,-1,NULL,m->get_streamID());
      packet->set_chip(m->get_tlx_addr().chip);packet->set_partition(m->get_sub_partition_id());
      packet->set_ft_wb_data(c.m_ep_l2_payload.inspect(h).versions);
      packet->set_ro_lower_packet();packet->set_ep_l2_payload_identity(h.payload_id,h.generation);
      packet->set_status(c.m_miss_queue_status,cycle);c.m_miss_queue.push_back(packet);
      events.push_back(cache_event(READ_REQUEST_SENT));
    }
  }
  bool merge_existing(mem_fetch *m,unsigned time,std::list<cache_event>&events,cache_request_status &r) {
    handle h;if(!candidate(m)||!target(m,h))return false;
    unsigned mask=m->get_access_sector_mask().to_ulong(),missing=needed(m,h);
    r=RESERVATION_FAIL;
    if(!capacity(m,true)||!c.miss_queue_has_slots(__builtin_popcount(missing)))return true;
    if(tx->merge(h,m,mask,1,true,ep_l2_ro::GLOBAL_DEMAND_READ)!=ep_l2_ro::ACCEPTED)return true;
    if(tx->attached(h)) {
      unsigned i=~0U;bool wb=false;evicted_block_info ev;
      c.m_tag_array->access(c.m_config.block_addr(m->get_addr()),time,i,wb,ev,m);assert(!wb);
    }
    m->set_ep_l2_payload_identity(h.payload_id,h.generation);
    issue(m,h,missing,time,events);++accepted_ro;r=MISS;return true;
  }
  cache_request_status accept_new(mem_fetch *m,unsigned index,cache_request_status probe,unsigned time,std::list<cache_event>&events) {
    const handle h=c.tag_payload_handle(index);
    bool wb=false;evicted_block_info ev;unsigned actual=index;
    c.m_tag_array->access(c.m_config.block_addr(m->get_addr()),time,actual,wb,ev,m);assert(actual==index);
    ++generations[index];
    unsigned mask=m->get_access_sector_mask().to_ulong();
    unsigned ready=c.m_ep_l2_payload.inspect(h).ready_sector_mask.to_ulong();
    auto admitted=tx->admit(1,c.m_config.block_addr(m->get_addr()),h,
      ep_l2_ro::tag_binding(index,generations[index]),m,mask,true,ep_l2_ro::GLOBAL_DEMAND_READ,ready);
    if(admitted!=ep_l2_ro::ACCEPTED)throw std::logic_error("RO preview/admit disagreement");
    issue(m,h,mask&~ready,time,events);++accepted_ro;
    if(wb) {
      auto packet=c.m_memfetch_creator->alloc(ev.m_block_addr,c.m_wrbk_type,m->get_access_warp_mask(),
        ev.m_byte_mask,ev.m_sector_mask,ev.m_modified_size,true,physical_cycle(),-1,-1,-1,NULL,m->get_streamID());
      packet->set_chip(m->get_tlx_addr().chip);packet->set_partition(m->get_sub_partition_id());
      c.send_write_request(packet,WRITE_BACK_REQUEST_SENT,time,events);
    }
    c.m_stats.inc_stats(m->get_access_type(),probe,m->get_streamID());
    c.m_stats.inc_stats_pw(m->get_access_type(),probe,m->get_streamID());
    return MISS;
  }
  bool has_response() const {handle h;return tx->ready_response(h,cursor);}
  bool prepare(uint64_t cycle) {
    if(cycle!=physical_cycle())throw std::logic_error("RO response bank cycle must be physical GPU time");
    if(selected && response_cycle==cycle)return true;
    if(selected){tx->commit_response(reply.transaction,false,response_cycle);selected=false;}
    handle h;if(!tx->ready_response(h,cursor))return false;
    if(!tx->prepare_response(h,cycle,reply))return false;
    selected=true;response_cycle=cycle;
    reply.request->set_ft_wb_data(c.m_ep_l2_payload.inspect(h).versions);return true;
  }
  void commit(uint64_t cycle) {
    if(!selected)throw std::logic_error("RO response commit without grant");
    tx->commit_response(reply.transaction,true,cycle);cursor=(reply.transaction.payload_id+1)%c.m_ep_l2_payload.physical_slots();selected=false;prefer_ro=false;
  }
};
void l2_cache::ro_init() {
  m_ro=NULL;
  if(m_config.m_ro_tag_detach||m_config.m_ro_line_mshr_bypass)m_ro=new ep_l2_ro_runtime(*this);
}
l2_cache::~l2_cache(){delete m_ro;}
void l2_cache::ro_begin_epoch(){if(m_ro){
  if(!m_ro->command_bound||m_ro->ever_open||!ft_drained()||m_mshrs.descriptor_count_used()||!m_miss_queue.empty()||ep_l2_wad_occupancy())
    throw std::logic_error("RO epoch activation requires complete prior drain");
  m_ro->tx->begin_epoch(1,m_ro->cert->manifest_sha256());m_ro->open=m_ro->ever_open=true;
}}
void l2_cache::ro_end_epoch(){if(m_ro){
  if(!m_ro->open||!ft_drained()||m_mshrs.descriptor_count_used()||!m_miss_queue.empty()||!m_ro->tx->end_epoch())
    throw std::logic_error("RO whole-commandlist end is not drained");
  m_ro->open=false;
}}
bool l2_cache::ro_drained() const {return !m_ro||m_ro->tx->drained();}
bool l2_cache::access_ready() const {return (m_ro&&m_ro->has_response())||baseline_cache::access_ready();}
bool l2_cache::ro_return(mem_fetch *m,unsigned long long cycle) {
  if(!m_ro||!m->ro_lower_packet())throw std::logic_error("RO return without runtime");
  if(!m->has_ft_wb_data())throw std::logic_error("RO return lacks reference versions");
  if(cycle!=m_ro->physical_cycle())throw std::logic_error("RO return bank cycle must be physical GPU time");
  return m_ro->port.accept_return(*m_ro->tx,ep_l2::PayloadHandle(m->get_ep_l2_payload_id(),m->get_ep_l2_payload_generation()),
    m->get_access_sector_mask().to_ulong(),cycle,m->get_ft_wb_data());
}
void l2_cache::ro_print(FILE *f,unsigned slice) const {if(m_ro){
  const auto &s=m_ro->tx->stats;
  fprintf(f,"EPL2ROV1|SUMMARY|slice=%u|detach=%u|bypass=%u|accepted_global_reads=%llu|accepted_oracle_candidates=%llu|accepted_ro=%llu|transactions=%llu|merges=%llu|detached=%llu|lower_sectors=%llu|concurrent_duplicate_sectors=%llu|responses=%llu|ro_bank_fill_ops=%llu|ro_bank_response_ops=%llu|live=%u|manifest=%s|scope=TRACE_MODEL_CLOSED_WORLD\n",
    slice,m_config.m_ro_tag_detach,m_config.m_ro_line_mshr_bypass,
    (unsigned long long)m_ro->accepted_global_reads,(unsigned long long)m_ro->accepted_candidates,(unsigned long long)m_ro->accepted_ro,
    (unsigned long long)s.admits,(unsigned long long)s.merges,(unsigned long long)s.detached,(unsigned long long)s.lower_sectors,
    (unsigned long long)s.duplicate_sectors,(unsigned long long)s.responses,
    m_ep_l2_payload.bank_ro_fill_write(),m_ep_l2_payload.bank_ro_response_read(),m_ro->tx->live_transactions(),m_ro->cert->manifest_sha256().c_str());
}}

void l2_cache::ro_bind_commandlist(const char *path){if(m_ro){
  std::ifstream f(path,std::ios::binary);if(!f)throw std::invalid_argument("RO commandlist missing");
  std::string bytes((std::istreambuf_iterator<char>(f)),{});
  if(ep_l2_ro::sha256(bytes)!=m_ro->command_sha)throw std::invalid_argument("RO commandlist identity mismatch");
  m_ro->command_bound=true;
}}
