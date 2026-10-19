// SOURCE ONLY: requires A's future integrated W+V+T+M native cache/partition.
// Reuses B's actual partition + independent nonzero sector-version reference.
// No shadow allocator, fake response insertion, local readonly predicate or
// second request pool. The synthetic whole-epoch certificate is prepared from
// this complete bounded stimulus; it is never an application certificate.
#include "native_fixture.h"
#include <algorithm>
#include <deque>

namespace {
const new_addr_type stride=64*128,ro_base=0x400000;
using P=ep_l2_payload_store;
using H=ep_l2::PayloadHandle;

struct vrw_fixture {
  victim_fixture f;
  std::deque<mem_fetch*> ro;
  unsigned ro_packets=0,ro_bytes=0;
  unsigned full_uid=0;
  explicit vrw_fixture(char **argv,const char *command):f(argv,"512:2:64:1",
      "S:64:128:16,L:B:m:L:L,A:128:32,256:0,32") {
    assert(f.cache->ro_enabled() && f.cache->tvd_enabled());
    f.sp->ro_bind_commandlist(command);f.sp->ro_begin_epoch();
  }
  void lower() {
    while(!f.sp->L2_dram_queue_empty()) {
      auto p=f.sp->L2_dram_queue_top();f.sp->L2_dram_queue_pop();
      if(p->get_access_type()==L2_WRBK_ACC) {
        const auto t=f.cache->tvd_for_test()->find(p->get_addr());
        assert(f.cache->tvd_for_test()->writeback_matches(t,p->get_addr(),p->get_request_uid()));
        for(unsigned s=0;s<4;++s)if(p->get_access_sector_mask().test(s))
          assert(p->get_ft_wb_data()[s]==f.memory.at(p->get_addr())[s]);
        ++f.lower_wb;f.wb.push_back(p);
      } else {
        ++f.lower_read_packets;f.lower_read_bytes+=p->get_data_size();
        if(p->ro_lower_packet()) {ro.push_back(p);++ro_packets;ro_bytes+=p->get_data_size();}
        else assert(f.reads.insert({p->get_addr(),p}).second);
      }
    }
  }
  void step(bool collect=true) {
    const bool old=f.block_lower;f.block_lower=true;f.step(collect);f.block_lower=old;lower();
    assert(f.cache->ep_l2_descriptor_count_used()<=512);
  }
  void frontend_empty() {
    // ROP publishes the input at cycle end. Observe the real head first.
    bool observed=false;
    for(unsigned n=0;n<20000;++n) {
      step();observed|=f.sp->ep_l2_icntl2_occupancy()!=0;
      if(observed && !f.sp->ep_l2_icntl2_occupancy()){step();return;}
    }
    assert(false);
  }
  void wait(unsigned uid) {
    for(unsigned n=0;n<20000 && !f.replies.count(uid);++n)step();
    assert(f.replies.count(uid));
  }
  void done(new_addr_type address,bool collect=true) {
    auto it=std::find_if(f.wb.begin(),f.wb.end(),[&](mem_fetch*p){return p->get_addr()==address;});
    assert(it!=f.wb.end());auto p=*it;f.wb.erase(it);
    const auto original=p->get_ft_wb_data();
    f.sp->set_done(p);assert(p->get_ft_wb_data()==original);delete p;++f.completed_wb;
    step(collect); // One completion CAM event/physical cycle.
  }
  mem_fetch *make_full_ro() {
    active_mask_t active;active.set(0);mem_access_byte_mask_t bytes;bytes.set();
    mem_access_sector_mask_t sectors;sectors.set();
    auto p=f.alloc->alloc(ro_base,GLOBAL_ACC_R,active,bytes,sectors,128,false,
        f.gpu->gpu_sim_cycle,0,0,0,nullptr,0);
    if(!f.memory.count(ro_base))f.memory[ro_base]={{ro_base+11,ro_base+22,ro_base+33,ro_base+44}};
    full_uid=p->get_request_uid();f.expected[full_uid]=f.memory.at(ro_base);return p;
  }
  H submit_ro(unsigned line,bool full=false) {
    const unsigned before=ro_packets;
    auto p=full?make_full_ro():f.make(ro_base+line*stride,false);
    f.send(p);frontend_empty();
    // Observe real accepted lower packets and their actual payload identity.
    for(unsigned n=0;n<20000;++n) {
      if(ro_packets>before)for(auto it=ro.rbegin();it!=ro.rend();++it) {
        auto q=*it;
        if((q->get_addr()&~127ull)==ro_base+line*stride)
          return H(q->get_ep_l2_payload_id(),q->get_ep_l2_payload_generation());
      }
      step();
    }
    assert(false);return H();
  }
  void queue_ro(H h,unsigned sector) {
    auto it=std::find_if(ro.begin(),ro.end(),[&](mem_fetch*p){return
      p->get_ep_l2_payload_id()==h.payload_id && p->get_ep_l2_payload_generation()==h.generation &&
      p->get_access_sector_mask().test(sector);});
    assert(it!=ro.end());auto p=*it;ro.erase(it);
    p->set_ft_wb_data(f.memory.at(p->get_addr()&~127ull));p->set_reply();
    assert(!f.sp->dram_L2_queue_full());f.sp->dram_L2_queue_push(p);
  }
  void return_ro(H h,unsigned sector,bool collect=true) {
    queue_ro(h,sector);step(collect);
  }
};
}

int main(int argc,char **argv) {
  // argv: base.cfg combined+oracle.cfg commandlist 16 0 0 1; same FT geometry.
  assert(argc==8);char *native_args[]={argv[0],argv[1],argv[2],const_cast<char*>("2"),argv[4],argv[5],argv[6],argv[7]};
  vrw_fixture g(native_args,argv[3]);auto &f=g.f;auto &ram=f.cache->ep_l2_payload_for_test();
  auto dir=f.cache->tvd_for_test();assert(!ram.fixed_victim_entries());
  // Native-only read class is intentionally outside GLOBAL_ACC_R RO routing.
  // It really fills A0; the synthetic full-epoch certificate still proves A0
  // is never written. No !is_write shortcut is used for certification.
  f.memory[ro_base]={{ro_base+11,ro_base+22,ro_base+33,ro_base+44}};
  active_mask_t active;active.set(0);mem_access_byte_mask_t bytes;
  for(unsigned i=0;i<32;++i)bytes.set(i);
  auto initial=f.alloc->alloc(ro_base,TEXTURE_ACC_R,active,bytes,mem_access_sector_mask_t(1),32,
      false,f.gpu->gpu_sim_cycle,0,0,0,nullptr,0);
  const auto initial_uid=initial->get_request_uid();f.expected[initial_uid]=f.memory.at(ro_base);f.send(initial);
  for(unsigned n=0;n<20000 && !f.replies.count(initial_uid);++n){g.step();f.return_all_reads_reverse();}
  assert(f.replies.count(initial_uid) && !g.ro_packets);
  for(unsigned i=0;i<17;++i)f.write(i*stride);
  auto a=dir->find(ro_base);assert(dir->live(a) && dir->at(a.index).status==l2_cache::tvd_directory::CLEAN);
  const H clean_a=dir->at(a.index).payload;
  const auto before=f.lower_read_packets;
  const auto served=dir->stats().read_responses;
  auto hit=f.make(ro_base,false);const auto hit_uid=hit->get_request_uid();f.send(hit);
  for(unsigned n=0;n<20000 && !f.replies.count(hit_uid);++n){g.step();assert(!g.ro_packets);}
  assert(f.replies.count(hit_uid) && f.lower_read_packets==before);
  assert(dir->stats().read_responses==served+1); // V wins before new RO work.
  for(unsigned i=17;i<144;++i)f.write(i*stride);
  assert(dir->mandatory()==128 && dir->occupied()==128);
  assert(!dir->live(dir->find(ro_base)) && !ram.handle_live(clean_a));
  // Mandatory WB preempts optional A0. Actual completions free directory
  // credit for RO dirty replacements; no shadow row reservation is injected.
  g.done(32*stride);const H old=g.submit_ro(0,true);
  for(unsigned i=1;i<17;++i){g.done((32+i)*stride);g.submit_ro(i);}
  const H newer=g.submit_ro(0);
  assert(old!=newer && ram.handle_live(old) && ram.handle_live(newer));
  assert(g.ro_packets==21 && g.ro_bytes==21*32);
  assert(f.cache->ep_l2_descriptor_count_used()==18); // Shared real D512.
  unsigned duplicate_sectors=0;
  for(auto p:g.ro)duplicate_sectors+=(p->get_addr()&~127ull)==ro_base;
  assert(duplicate_sectors==5); // First full line4 plus later duplicate sector1.
  const auto old_saved=ram.inspect(old),new_saved=ram.inspect(newer);
  const auto offset=f.sp->ft_memcpy_offset_for_test();
  assert(!f.sp->ft_try_force_l2_tag_update(ro_base,f.gpu->gpu_sim_cycle,mem_access_sector_mask_t(1)));
  assert(f.sp->ft_memcpy_offset_for_test()==offset && ram.inspect(old).versions==old_saved.versions);
  assert(ram.inspect(newer).versions==new_saved.versions && ram.inspect(old).pins==old_saved.pins);
  for(unsigned s:{3u,1u,2u})g.return_ro(old,s);
  assert(!f.replies.count(g.full_uid) && ram.inspect(old).pending_sector_mask.to_ulong()==1);
  // Select from actual retained WB owners, rather than assuming an allocator
  // physical-id sequence. Keep B4 untouched for the later writer conflict.
  new_addr_type victim=0;bool selected=false;
  for(unsigned i=0;i<32 && !selected;++i)if(i!=4) {
    const auto candidate=dir->find(i*stride);
    if(dir->live(candidate) && dir->at(candidate.index).status==l2_cache::tvd_directory::WB_INFLIGHT &&
       dir->at(candidate.index).payload.payload_id%4!=old.payload_id%4) {
      victim=i*stride;selected=true;
    }
  }
  assert(selected);const auto t=dir->find(victim);
  assert(dir->live(t) && dir->at(t.index).status==l2_cache::tvd_directory::WB_INFLIGHT);
  const H v=dir->at(t.index).payload;assert(v.payload_id%4!=old.payload_id%4);
  auto read=f.make(victim,false);const auto uid=read->get_request_uid();f.send(read);
  for(unsigned n=0;n<1000 && !f.cache->tvd_lookup_started_for_test(read);++n)g.step();
  assert(f.cache->tvd_lookup_started_for_test(read));
  H contender;
  for(unsigned i=0;i<dir->capacity();++i){const auto &e=dir->at(i);
    if(e.payload.valid() && e.payload!=v && e.payload.payload_id%4==v.payload_id%4 &&
       ram.inspect(e.payload).ready_sector_mask.any() && !ram.inspect(e.payload).pins){contender=e.payload;break;}}
  // Explicit paid component contention; not a measured application event.
  assert(contender.valid() && ram.request(contender,false,f.gpu->gpu_sim_cycle)==P::GRANTED);
  g.queue_ro(old,0);g.step(false);
  assert(dir->at(t.index).readers==1 && ram.inspect(v).pins==1);
  g.step(false);assert(f.sp->ep_l2_l2icnt_occupancy()==1);
  assert(!ram.handle_live(old) && ram.handle_live(newer));
  assert(ram.inspect(newer).pending_sector_mask.to_ulong()==1);
  g.done(victim,false);
  for(unsigned n=0;n<4;++n){g.step(false);assert(ram.inspect(v).pins==1 && dir->at(t.index).readers==1);}
  assert(dir->at(t.index).status==l2_cache::tvd_directory::WB_COMPLETE);
  f.collect();g.wait(uid);
  assert(ram.inspect(v).status==P::TVD_VALID && !ram.inspect(v).pins);
  // Another same-address writer must not alter its still-inflight WB snapshot.
  const new_addr_type writer_addr=4*stride;const auto wt=dir->find(writer_addr);
  assert(dir->live(wt) && dir->at(wt.index).mandatory());
  const auto wh=dir->at(wt.index).payload;
  const auto old_wb_versions=ram.inspect(wh).versions;
  auto writer=f.make(writer_addr,true);const auto writer_uid=writer->get_request_uid();f.send(writer);
  for(unsigned n=0;n<8;++n)g.step();
  assert(!f.replies.count(writer_uid) && ram.inspect(wh).versions==old_wb_versions);
  g.done(writer_addr);f.memory[writer_addr][0]=(unsigned long long)writer_uid+1;
  // Real RO responses (including later A0) retire independently and let the
  // writer acquire a legitimate native Tag; no forced Tag invalidation.
  while(!g.ro.empty()) {
    auto p=g.ro.back();g.ro.pop_back();p->set_ft_wb_data(f.memory.at(p->get_addr()&~127ull));p->set_reply();
    assert(!f.sp->dram_L2_queue_full());f.sp->dram_L2_queue_push(p);g.step();
  }
  for(unsigned n=0;n<20000 && f.replies.size()!=f.expected.size();++n)g.step();
  assert(f.replies.size()==f.expected.size() && f.replies.count(writer_uid));
  while(!f.wb.empty())g.done(f.wb.front()->get_addr());
  assert(f.cache->ro_drained() && !f.cache->ep_l2_descriptor_count_used());
  assert(f.lower_read_packets==22 && f.lower_read_bytes==22*32);
  assert(!f.cache->ep_l2_payload_pending_operations());
  f.read(writer_addr);f.sp->ro_end_epoch();f.finish();
  puts("VRW_INTERLEAVE|native_expected_only_until_executed|priority_reclaim_RO_duplicate_late_fill_pin_WB_done_write_order_drain=PASS");
}
