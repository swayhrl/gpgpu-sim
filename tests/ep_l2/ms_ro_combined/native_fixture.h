#pragma once
// Reuse B's production objects/reference checks. This specialization permits
// several real lower packets for one line; no cache/tag/pool state is seeded.
#include "../ms_victim/native_fixture.h"
struct combined_fixture : victim_fixture {
  std::vector<mem_fetch *> pending_reads;
  unsigned peak_shared=0;
  explicit combined_fixture(char **argv,bool require_ro=true,
      const char *queues="1024:16:64:64"):victim_fixture(argv,queues) {
    assert(cache->tvd_enabled() && (!require_ro || cache->ro_enabled()));
    sp->ro_bind_commandlist(argv[8]);sp->ro_begin_epoch();
  }
  void step(bool consume=true) {
    unsigned long long before[4];for(unsigned b=0;b<4;++b)before[b]=cache->ep_l2_payload_bank_grants(b);
    part->cache_cycle(gpu->gpu_sim_cycle);
    for(unsigned b=0;b<4;++b)assert(cache->ep_l2_payload_bank_grants(b)-before[b]<=1);
    cache->ft_assert_quota();auto &ram=cache->ep_l2_payload_for_test();
    assert(ram.ownership_consistent() && ram.fixed_quota_consistent());
    assert(ram.free_slots()+ram.resident_used()+ram.bypass_used()+ram.wb_used()+ram.tvd_used()+ram.ro_used()==1168);
    peak_shared=std::max(peak_shared,cache->ep_l2_descriptor_count_used());assert(peak_shared<=512);
    if(consume)collect();
    if(!block_lower)take_lower();
    ++gpu->gpu_sim_cycle;
  }
  void take_lower() {
    while(!sp->L2_dram_queue_empty()) {
      auto packet=sp->L2_dram_queue_top();sp->L2_dram_queue_pop();
      diag_test_witness(1,gpu->gpu_sim_cycle,packet);
      if(packet->get_access_type()==L2_WRBK_ACC) {
        assert(packet->has_ft_wb_data());
        for(unsigned s=0;s<4;++s)if(packet->get_access_sector_mask().test(s))
          assert(packet->get_ft_wb_data()[s]==memory.at(packet->get_addr())[s]);
        assert(cache->ep_l2_wad_contains(packet->get_addr()));wb.push_back(packet);++lower_wb;
      } else {pending_reads.push_back(packet);++lower_read_packets;lower_read_bytes+=packet->get_data_size();}
    }
  }
  void return_one(unsigned index) {
    assert(index<pending_reads.size() && !sp->dram_L2_queue_full());
    auto packet=pending_reads[index];pending_reads.erase(pending_reads.begin()+index);
    packet->set_ft_wb_data(memory.at(packet->get_addr()&~127ull));diag_test_witness(2,gpu->gpu_sim_cycle,packet);packet->set_reply();sp->dram_L2_queue_push(packet);
  }
  void complete_one() {
    if(wb.empty())return;
    auto packet=wb.front();wb.erase(wb.begin());sp->set_done(packet);diag_test_witness(4,gpu->gpu_sim_cycle,packet);delete packet;++completed_wb;
  }
  void await_lower(unsigned count) {
    for(unsigned n=0;n<20000 && pending_reads.size()<count;++n)step();
    assert(pending_reads.size()==count);
  }
  void await_frontend() {
    // push() first enters the real ROP queue, so ICNT occupancy can be
    // zero before the request has even reached the frontend. Step first,
    // then consume the zero-latency ROP transfer and actual frontend turn.
    for(unsigned n=0;n<20000;++n) {
      step();
      if(!sp->ep_l2_icntl2_occupancy()) {step();return;}
    }
    assert(false && "frontend did not consume request");
  }
  void wait(unsigned uid,bool return_memory=true) {
    for(unsigned n=0;n<20000 && !replies.count(uid);++n) {
      step();if(return_memory && !pending_reads.empty() && !sp->dram_L2_queue_full())return_one(pending_reads.size()-1);
    }
    assert(replies.count(uid));
  }
  unsigned read(new_addr_type address) {auto r=make(address,false);auto uid=r->get_request_uid();send(r);wait(uid);return uid;}
  unsigned write(new_addr_type address) {
    auto r=make(address,true);auto uid=r->get_request_uid();auto line=address&~127ull;unsigned s=(address>>5)&3;
    if(!memory.count(line))memory[line]={{line+11,line+22,line+33,line+44}};
    memory[line][s]=expected.at(uid)[s];send(r);wait(uid,false);return uid;
  }
  ep_l2::PayloadHandle tag_for(new_addr_type line) {
    for(unsigned i=0;i<1024;++i) {auto h=cache->ep_l2_tag_payload_handle_for_test(i);
      if(cache->ep_l2_payload_for_test().handle_owner_matches(h,line))return h;}
    return ep_l2::PayloadHandle();
  }
  void finish() {
    for(unsigned n=0;n<40000 && (part->busy() || !pending_reads.empty() || !wb.empty());++n) {
      step();if(!pending_reads.empty() && !sp->dram_L2_queue_full())return_one(pending_reads.size()-1);
      complete_one();
    }
    assert(!part->busy() && pending_reads.empty() && wb.empty() && replies.size()==expected.size());
    assert(cache->ft_drained() && !cache->ep_l2_descriptor_count_used() && !cache->ep_l2_payload_pending_operations());
    assert(cache->tvd_actual_lower_read_packets()==lower_read_packets && cache->tvd_actual_lower_read_bytes()==lower_read_bytes);
    sp->ro_end_epoch();cache->ro_print(stdout,0);sp->flushL2();
    assert(!cache->ep_l2_payload_for_test().ro_used() && !cache->ep_l2_payload_for_test().tvd_used());
    printf("A_COMMON_FIXTURE|SEMANTIC|cycles=%llu|replies=%zu|lower_reads=%llu|lower_bytes=%llu|drained=1\n",
           gpu->gpu_sim_cycle+gpu->gpu_tot_sim_cycle,replies.size(),lower_read_packets,lower_read_bytes);
  }
};
