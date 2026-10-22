// Included once by gpu-cache.cc. No separate payload/descriptor allocator.
#include "ep-l2-ro-store-port.h"
#include "ep-l2-ro-sha256.h"
#include <fstream>
#include <sstream>
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
  // One shared RESPONSE head. A denied native choice retains its existing
  // baseline queue identity until actual enqueue, just like reply does for RO.
  bool native_response_chosen=false;
  std::string command_sha;
  ep_l2_ro::response reply;
  uint64_t response_cycle;
  unsigned cursor;
  // A single frontend lookup ticket, not an address-indexed completion map.
  mutable uint64_t query_uid=~uint64_t(0),query_ready_at=0,query_count=0;
  mutable uint64_t query_issue_cycle=~uint64_t(0),query_port_waits=0;
  mutable uint64_t query_last_wait=~uint64_t(0),query_wait_cycles=0;
  mutable uint64_t observed_uid=~uint64_t(0),blocked_mask=0;
  mutable std::array<uint64_t,6> first_wait{},unique_blocks{},wait_cycles{};
  mutable std::array<std::array<uint64_t,16>,6> wait_hist{};
  uint64_t accepted_global_reads, accepted_candidates, accepted_ro;
  uint64_t epoch_begin_cycle=0,epoch_end_cycle=0,last_response_cycle=0;
  ep_l2_ro_runtime(l2_cache &cache) : c(cache), port(c.m_ep_l2_payload,*this,true),
    generations(c.m_config.get_num_lines(),1),open(false),ever_open(false),selected(false),command_bound(false),prefer_ro(true),
    reply(),response_cycle(0),cursor(0),accepted_global_reads(0),accepted_candidates(0),accepted_ro(0) {
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
    const std::string scope=doc.get<std::string>("scope");
    const bool fallback=doc.get<std::string>("status")=="SAFE_FALLBACK_NO_CERTIFIED_LINES";
    if(doc.get<std::string>("schema")!="EP_L2_RO_ORACLE_V1" ||
       doc.get<std::string>("epoch")!="whole_commandlist" ||
       scope!=cfg.m_ro_scope || scope!=(fallback?"NO_READONLY_CERTIFICATION":"TRACE_MODEL_CLOSED_WORLD") ||
       doc.get<std::string>("address_domain")!="CANONICAL_GPU_GLOBAL_VA_NO_TRANSLATION_ALIAS" ||
       (!fallback && !doc.get_child("unresolved_opcodes").empty()))throw std::invalid_argument("RO unsupported/incomplete epoch certificate");
    if(doc.get_child("artifacts").get<std::string>(boost::property_tree::ptree::path_type("certified_ranges.csv",'/'))!=cfg.m_ro_ranges_sha256)
      throw std::invalid_argument("RO ranges not bound to manifest");
    command_sha=doc.get<std::string>("commandlist_sha256");
    if(command_sha.size()!=64 || command_sha.find_first_not_of("0123456789abcdef")!=std::string::npos)
      throw std::invalid_argument("invalid RO command identity");
    std::istringstream ranges(bytes);
    cert.reset(new ep_l2_ro::oracle_certificate(ranges,cfg.m_ro_manifest_sha256,cfg.m_ro_scope));
    if(fallback && cert->range_count())throw std::invalid_argument("fallback cannot enable any RO line");
    tx.reset(new ep_l2_ro::transactions(c.m_mshrs,port,c.m_ep_l2_payload.physical_slots(),*cert,
      ep_l2_ro::switches(cfg.m_ro_tag_detach,cfg.m_ro_line_mshr_bypass)));
    tx->diagnostic_observer=ep_l2_detach_diag::get(&c);
  }
  uint64_t physical_cycle() const {
    return c.ft_physical_cycle();
  }
  bool prefill_allowed(new_addr_type address,uint64_t count=128) const {
    return !selected && tx->drained() &&
      !(open && cert->overlaps_bytes(address,count));
  }
  void prefill_begin(new_addr_type address,uint64_t count=128) const {
    if(!prefill_allowed(address,count))
      throw std::logic_error("RO host prefill conflicts with active ownership or certified epoch range");
  }
  void prefill_complete(unsigned index,handle h,new_addr_type owner,
                        mem_access_sector_mask_t mask) {
    prefill_begin(owner);unsigned actual=~0U;
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
  bool query_admissible(mem_fetch *m) const {
    return !new_request(m) || (query_uid==m->get_request_uid() && physical_cycle()>=query_ready_at);
  }
  // Called once at the actual timed frontend-head observation point. Preview
  // itself never launches a lookup, reserves ownership or changes counters.
  void frontend_cycle(mem_fetch *m) {
    if (!new_request(m) || !directory_ready(m)) return;
    const auto now=physical_cycle();
    if (query_uid!=m->get_request_uid()) {
      if (query_issue_cycle==now) {++query_port_waits;return;}
      query_issue_cycle=now;query_uid=m->get_request_uid();query_ready_at=now+1;++query_count;
      query_last_wait=~uint64_t(0);
    }
    if (now<query_ready_at) {
      if (query_last_wait!=now) {++query_wait_cycles;query_last_wait=now;}
      return;
    }
    l2_access_plan p;
    if (!preview(m,p)) return;
    if (p.ep_l2_tag_set_all_reserved) {observe_block(m,0);return;}
    const bool existing=p.mshr_hit || target_for_observer(m);
    if (c.m_mshrs.descriptor_count_used()>=512) observe_block(m,2);
    if (c.m_mshrs.targets_on_line(c.m_config.block_addr(m->get_addr()))>=32) observe_block(m,3);
    if (!existing && !c.m_config.m_ro_line_mshr_bypass && c.m_mshrs.num_entries_used()>=128) observe_block(m,1);
  }
  bool target_for_observer(mem_fetch *m) const {handle h;return target(m,h);}
  bool directory_ready(mem_fetch *m) const {
    if(!c.m_tvd)return true;
    unsigned index=~0U;
    const auto tag=c.m_tag_array->probe(c.m_config.block_addr(m->get_addr()),index,m,m->is_write(),true);
    if(tag!=MISS && tag!=RESERVATION_FAIL)return true;
    return c.m_tvd_lookup==m && c.m_tvd_lookup_done;
  }
  void observe_block(const mem_fetch *m,unsigned reason) const {
    if (observed_uid!=m->get_request_uid()) {observed_uid=m->get_request_uid();blocked_mask=0;}
    if (!(blocked_mask&(uint64_t(1)<<reason))) {
      blocked_mask|=uint64_t(1)<<reason;first_wait[reason]=physical_cycle();++unique_blocks[reason];
    }
  }
  void check_access(mem_fetch *m) const {
    if(!open)throw std::logic_error("RO access outside full-commandlist epoch");
    if(m->get_is_write()||m->isatomic())tx->assert_no_epoch_write(1,c.m_config.block_addr(m->get_addr()));
  }
  void record_accepted(mem_fetch *m) {
    if(demand(m))++accepted_global_reads;
    if(candidate(m))++accepted_candidates;
    if (observed_uid==m->get_request_uid()) {
      for (unsigned r=0;r<6;++r) if (blocked_mask&(uint64_t(1)<<r)) {
        uint64_t duration=physical_cycle()-first_wait[r];wait_cycles[r]+=duration;
        unsigned bin=0;while (duration>1 && bin<15) {duration>>=1;++bin;}++wait_hist[r][bin];
      }
      blocked_mask=0;
    }
    if (query_uid==m->get_request_uid()) query_uid=~uint64_t(0);
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
  bool native_owner_conflict(mem_fetch *m) const {
    if(candidate(m))return false;
    // A native waiter cannot borrow an RO completion's Tag/Line-MSHR: RO
    // returns do not queue native descriptor responses, and its pending
    // landing is not a RESIDENT lower-read owner. Retry after RO retirement.
    // Use only the existing Tag/native line index, never an address CAM.
    handle h;return target(m,h);
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
  bool capacity(mem_fetch *m,bool existing,ep_l2_detach_diag::preview *o=nullptr) const {
    const auto line=c.m_config.block_addr(m->get_addr());
    // Capture only actually evaluated original short-circuit operands.
    const bool descriptor=c.m_mshrs.descriptor_count_used()<512;
    if(o){o->capacity_consulted=true;o->descriptor_check=descriptor;}
    if(!descriptor)return false;
    const bool address=c.m_mshrs.targets_on_line(line)<32;
    if(o)o->address_check=address;
    if(!address)return false;
    if(existing||c.m_config.m_ro_line_mshr_bypass){if(o)o->line_na=true;return true;}
    const bool line_absent=!c.m_mshrs.probe(line);
    if(o)o->line_probe_check=line_absent;
    if(!line_absent)return false;
    const bool line_room=c.m_mshrs.num_entries_used()<128;
    if(o)o->line_slots_check=line_room;
    return line_room;
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
    unsigned i=~0U;const bool found=detachable(addr,i);
    if(auto*d=ep_l2_detach_diag::get(&c)){d->add(ep_l2_detach_diag::relieve_calls);d->add(!c.m_config.m_ro_tag_detach?ep_l2_detach_diag::relieve_T_disabled:found?ep_l2_detach_diag::relieve_eligible_selected:ep_l2_detach_diag::relieve_no_eligible);}
    if(auto*d=ep_l2_detach_diag::get(&c)){
      unsigned first=~0U;const unsigned base=c.m_config.set_index(addr)*c.m_config.get_assoc();
      if(c.m_config.m_ro_tag_detach)for(unsigned t=base;t<base+c.m_config.get_assoc();++t){auto h=c.tag_payload_handle(t);if(!tx->live(h))continue;const auto p=tx->diagnostic_state(h);if(p.attached&&!p.reuse&&(p.required&~p.ready)){first=t;break;}}
      if((found&&i!=first)||(!found&&first!=~0U))d->add(ep_l2_detach_diag::selected_identity_mismatch);
    }
    if(found)tx->detach(c.tag_payload_handle(i));
  }
  bool preview(mem_fetch *m,l2_access_plan &p,ep_l2_detach_diag::preview *o=nullptr) const {
    if(native_owner_conflict(m)) {
      p.exact=true;p.is_read=!m->get_is_write();p.is_write=m->get_is_write();
      p.probe_status=RESERVATION_FAIL;if(o)o->early=1;return true;
    }
    if(!new_request(m))return false;
    p.exact=true;p.is_read=true;
    // Read the existing paid frontend ticket; never issue a second/free TVD
    // address CAM from observational RO preview. TVD hits complete on its own
    // native frontend/ICNT path; RO owns only the resulting real cache miss.
    if(!directory_ready(m)) {p.probe_status=RESERVATION_FAIL;p.ep_l2_ro_lookup_wait=true;if(o)o->early=2;return true;}
    if (!query_admissible(m)) {p.probe_status=RESERVATION_FAIL;p.ep_l2_ro_lookup_wait=true;if(o)o->early=3;return true;}
    handle h;bool existing=target(m,h);
    unsigned i=~0U;
    p.probe_status=c.m_tag_array->probe(c.m_config.block_addr(m->get_addr()),i,m,false,true);
    if(o){o->production_reached=true;o->production_raw=p.probe_status;o->existing=existing;o->intermediate=p.probe_status;}
    if(existing) {
      p.probe_status=HIT_RESERVED;
    } else if(p.probe_status==RESERVATION_FAIL) {
      if(o)o->consulted=true;
      if(detachable(m->get_addr(),i))p.probe_status=MISS;
    }
    if(o)o->intermediate=p.probe_status;
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
    p.needs_new_mshr=!existing && !c.m_config.m_ro_line_mshr_bypass;
    p.needs_mshr_merge=existing && !c.m_config.m_ro_line_mshr_bypass;
    p.mshr_hit=existing && !c.m_config.m_ro_line_mshr_bypass;
    p.mshr_entry_available=p.mshr_merge_available=capacity(m,existing,o);
    // Shared descriptor/per-address bounds still block an M-mode request,
    // without misreporting that it needs a traditional line-MSHR.
    if(!capacity(m,existing)) p.probe_status=RESERVATION_FAIL;
    if(c.m_tvd?c.tvd_cached_wad_contains(m,c.m_config.block_addr(m->get_addr())):
        c.ep_l2_wad_contains(c.m_config.block_addr(m->get_addr()))) {p.probe_status=RESERVATION_FAIL;p.ep_l2_wad_same_address_hazard=true;}
    return true;
  }
  void issue(mem_fetch *m,handle h,unsigned mask,unsigned /*tag_time*/,std::list<cache_event>&events) {
    const uint64_t cycle=physical_cycle();
    for(unsigned n=0;n<4;++n)if(mask>>n&1) {
      mem_access_byte_mask_t bytes;for(unsigned b=n*32;b<n*32+32;++b)bytes.set(b);
      auto packet=c.m_memfetch_creator->alloc(c.m_config.block_addr(m->get_addr())+32*n,GLOBAL_ACC_R,
        m->get_access_warp_mask(),bytes,mem_access_sector_mask_t(1<<n),32,false,cycle,
        m->get_wid(),m->get_sid(),m->get_tpc(),NULL,m->get_streamID());
      packet->set_chip(m->get_tlx_addr().chip);packet->set_partition(m->get_sub_partition_id());
      packet->set_ft_wb_data(c.m_ep_l2_payload.inspect(h).versions);
      packet->set_ro_lower_packet();packet->set_ep_l2_payload_identity(h.payload_id,h.generation);
      packet->set_status(c.m_miss_queue_status,cycle);c.m_miss_queue.push_back(packet);
      events.push_back(cache_event(READ_REQUEST_SENT));
    }
  }
  bool merge_existing(mem_fetch *m,unsigned time,std::list<cache_event>&events,cache_request_status &r,ep_l2_detach_diag::field *observed_denial=nullptr) {
    handle h;if(!candidate(m)||!target(m,h))return false;
    unsigned mask=m->get_access_sector_mask().to_ulong(),missing=needed(m,h);
    r=RESERVATION_FAIL;
    if(!capacity(m,true)){if(observed_denial)*observed_denial=ep_l2_detach_diag::denied_capacity;return true;}
    if(!c.miss_queue_has_slots(__builtin_popcount(missing))){if(observed_denial)*observed_denial=ep_l2_detach_diag::denied_missq;return true;}
    if(tx->merge(h,m,mask,1,true,ep_l2_ro::GLOBAL_DEMAND_READ)!=ep_l2_ro::ACCEPTED){if(observed_denial)*observed_denial=ep_l2_detach_diag::denied_other;return true;}
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
      c.send_write_request(packet,WRITE_BACK_REQUEST_SENT,physical_cycle(),events);
    }
    c.m_stats.inc_stats(m->get_access_type(),probe,m->get_streamID());
    c.m_stats.inc_stats_pw(m->get_access_type(),probe,m->get_streamID());
    return MISS;
  }
  bool has_response() const {handle h;return reply.transaction.valid() || tx->ready_response(h,cursor);}
  bool prepare(uint64_t cycle) {
    if(cycle!=physical_cycle())throw std::logic_error("RO response bank cycle must be physical GPU time");
    if(selected && response_cycle==cycle)return true;
    if(selected){tx->commit_response(reply.transaction,false,response_cycle);selected=false;}
    // The existing reply identity also holds a denied candidate, but selected
    // means a real Bank grant in response_cycle. No data is read or exposed
    // while only waiting. Keep that identity until enqueue, including across
    // ICNT backpressure and a same-bank lower-index transaction becoming ready.
    if(!reply.transaction.valid()) {
      handle h;if(!tx->ready_response(h,cursor))return false;
      reply.transaction=h;
    }
    if(!tx->prepare_response(reply.transaction,cycle,reply))return false;
    selected=true;response_cycle=cycle;
    reply.request->set_ft_wb_data(c.m_ep_l2_payload.inspect(reply.transaction).versions);return true;
  }
  void commit(uint64_t cycle) {
    if(!selected)throw std::logic_error("RO response commit without grant");
    tx->commit_response(reply.transaction,true,cycle);cursor=(reply.transaction.payload_id+1)%c.m_ep_l2_payload.physical_slots();
    last_response_cycle=cycle;
    reply.transaction=handle();selected=false;prefer_ro=false;
  }
};
void l2_cache::ro_init() {
  ep_l2_detach_diag::state.add(this);
  m_ro=NULL;
  if(m_config.m_ro_tag_detach||m_config.m_ro_line_mshr_bypass)m_ro=new ep_l2_ro_runtime(*this);
}
l2_cache::~l2_cache(){ep_l2_detach_diag::state.erase(this);delete m_ro;}
void l2_cache::ro_begin_epoch(){if(m_ro){
  if(!m_ro->command_bound||m_ro->ever_open||!ft_drained()||m_mshrs.descriptor_count_used()||!m_miss_queue.empty()||ep_l2_wad_occupancy())
    throw std::logic_error("RO epoch activation requires complete prior drain");
  m_ro->tx->begin_epoch(1,m_ro->cert->manifest_sha256());m_ro->open=m_ro->ever_open=true;
  m_ro->epoch_begin_cycle=m_ro->physical_cycle();
}}
void l2_cache::ro_end_epoch(){if(m_ro){
  if(!m_ro->open||!ft_drained()||m_mshrs.descriptor_count_used()||!m_miss_queue.empty()||!m_ro->tx->end_epoch())
    throw std::logic_error("RO whole-commandlist end is not drained");
  m_ro->epoch_end_cycle=m_ro->physical_cycle();m_ro->open=false;
  if(auto*d=ep_l2_detach_diag::get(this))d->closed=true;
}}
bool l2_cache::ro_drained() const {return !m_ro||m_ro->tx->drained();}
bool l2_cache::access_ready() const {return (m_ro&&m_ro->has_response())||baseline_cache::access_ready();}
bool l2_cache::ro_return(mem_fetch *m,unsigned long long cycle) {
  if(!m_ro||!m->ro_lower_packet())throw std::logic_error("RO return without runtime");
  if(!m->has_ft_wb_data())throw std::logic_error("RO return lacks reference versions");
  if(cycle!=m_ro->physical_cycle())throw std::logic_error("RO return bank cycle must be physical GPU time");
  return m_ro->port.accept_return(*m_ro->tx,ep_l2::PayloadHandle(m->get_ep_l2_payload_id(),m->get_ep_l2_payload_generation()),
    m->get_access_sector_mask().to_ulong(),cycle,m->get_ft_wb_data(),m->get_request_uid());
}
void l2_cache::ro_print(FILE *f,unsigned slice) const {
  if(auto*d=ep_l2_detach_diag::get(this)){d->slice=slice;d->ro_runtime=bool(m_ro);d->T=m_config.m_ro_tag_detach;d->M=m_config.m_ro_line_mshr_bypass;if(!m_ro)d->closed=true;
    if(m_ro && d->total[ep_l2_detach_diag::detach_success]!=m_ro->tx->stats.detached)d->add(ep_l2_detach_diag::closure_errors);
    ep_l2_detach_diag::state.print(f,*d,ft_physical_cycle());}
  if(m_ro){
  const auto &s=m_ro->tx->stats;
  fprintf(f,"EPL2ROV1|SUMMARY|slice=%u|detach=%u|bypass=%u|accepted_global_reads=%llu|accepted_oracle_candidates=%llu|accepted_ro=%llu|transactions=%llu|merges=%llu|detached=%llu|lower_sectors=%llu|concurrent_duplicate_sectors=%llu|responses=%llu|ro_bank_fill_ops=%llu|ro_bank_response_ops=%llu|live=%u|manifest=%s|scope=%s\n",
    slice,m_config.m_ro_tag_detach,m_config.m_ro_line_mshr_bypass,
    (unsigned long long)m_ro->accepted_global_reads,(unsigned long long)m_ro->accepted_candidates,(unsigned long long)m_ro->accepted_ro,
    (unsigned long long)s.admits,(unsigned long long)s.merges,(unsigned long long)s.detached,(unsigned long long)s.lower_sectors,
    (unsigned long long)s.duplicate_sectors,(unsigned long long)s.responses,
    m_ep_l2_payload.bank_ro_fill_write(),m_ep_l2_payload.bank_ro_response_read(),m_ro->tx->live_transactions(),m_ro->cert->manifest_sha256().c_str(),m_config.m_ro_scope);
  fprintf(f,"EPL2ROC1|CLOSURE|slice=%u|native_descriptor_responses=%llu|ro_descriptor_responses=%llu|shared_descriptor_used=%u|shared_line_used=%u|ro_live=%u|epoch_closed=%u|scope=NATIVE_AND_RO_DISTINCT_SUBCOUNTERS_NOT_ALL_FRONTEND_REQUESTS\n",
    slice,m_ft_stats.descriptor_responses,(unsigned long long)s.responses,
    m_mshrs.descriptor_count_used(),m_mshrs.num_entries_used(),m_ro->tx->live_transactions(),
    unsigned(m_ro->ever_open&&!m_ro->open));
  fprintf(f,"EPL2ROC1|RESOURCE|slice=%u|normal_line_allocations=%llu|avoided_line_allocations=%llu|peak_transactions=%u|peak_shared_descriptors=%u|peak_shared_lines=%u|lower_read_bytes=%llu|concurrent_duplicate_read_bytes=%llu|query_count=%llu|query_latency_gpu_cycles=1|query_ports_per_slice=1|query_wait_cycles=%llu|query_port_wait_attempts=%llu|shared_descriptor_capacity=512|payload_capacity=1168|line_capacity=128|oracle=%s\n",
    slice,(unsigned long long)s.normal_line_allocations,(unsigned long long)s.avoided_line_allocations,
    s.peak_transactions,s.peak_shared_descriptors,s.peak_shared_lines,
    (unsigned long long)s.lower_sectors*32,(unsigned long long)s.duplicate_sectors*32,
    (unsigned long long)m_ro->query_count,(unsigned long long)m_ro->query_wait_cycles,(unsigned long long)m_ro->query_port_waits,m_config.m_ro_scope);
  const std::string begin=m_ro->ever_open?std::to_string(m_ro->epoch_begin_cycle):"NA";
  const std::string end=m_ro->ever_open&&!m_ro->open?std::to_string(m_ro->epoch_end_cycle):"NA";
  const std::string last=s.responses?std::to_string(m_ro->last_response_cycle):"NA";
  fprintf(f,"EPL2ROC1|LOWER_CONCURRENCY|slice=%u|peak_lower_transactions=%u|peak_independent_lower_lines=%u|epoch_begin_gpu_cycle=%s|epoch_end_gpu_cycle=%s|last_response_gpu_cycle=%s|completed_ro_requesters=%llu|scope=RO_ISSUED_NOT_RETURNED_PER_SLICE|measurement=OBSERVER_ONLY_NO_ADMISSION_LOOKUP\n",
    slice,s.peak_lower_transactions,s.peak_independent_lower_lines,begin.c_str(),end.c_str(),last.c_str(),(unsigned long long)s.responses);
  static const char *reasons[]={"TAG_SET_ALL_PENDING","LINE_MSHR_FULL","SHARED_DESCRIPTOR_FULL","PER_ADDRESS_FULL","LANDING_FULL","WAD"};
  for(unsigned r=0;r<4;++r) {
    fprintf(f,"EPL2ROC1|WAIT|slice=%u|reason=%s|unique_head_requests=%llu|first_to_accept_cycles=%llu|pending_head=%u|hist_log2=",
      slice,reasons[r],(unsigned long long)m_ro->unique_blocks[r],(unsigned long long)m_ro->wait_cycles[r],unsigned((m_ro->blocked_mask>>r)&1));
    for(unsigned b=0;b<16;++b)fprintf(f,"%s%llu",b?",":"",(unsigned long long)m_ro->wait_hist[r][b]);
    fprintf(f,"|scope=SLICE_FRONTEND_HEAD_NONEXCLUSIVE_WAIT\n");
  }
}}

void l2_cache::ro_bind_commandlist(const char *path){if(m_ro){
  std::ifstream f(path,std::ios::binary);if(!f)throw std::invalid_argument("RO commandlist missing");
  std::string bytes((std::istreambuf_iterator<char>(f)),{});
  if(ep_l2_ro::sha256(bytes)!=m_ro->command_sha)throw std::invalid_argument("RO commandlist identity mismatch");
  m_ro->command_bound=true;
}}

void l2_cache::ro_frontend_cycle(mem_fetch *mf) {if(m_ro){
  // Also protect R without V: preview may otherwise defer a certified writer
  // behind an RO owner before access() gets a chance to reject the violation.
  m_ro->check_access(mf);m_ro->frontend_cycle(mf);
}}
