#pragma once
// Adapt the84359d4 production-partition fixture. External memory is a declared
// test producer, not a second simulator payload RAM. It injects nonzero sector
// versions through real lower/ReturnQ packets; actual cache responses are read
// and checked independently. This fixture does NOT model DRAM timing or bytes.
#include "../potential_fixture.h"
#ifndef DIAG_TEST_WITNESS_ACTIVE
inline void diag_test_witness(unsigned,unsigned long long,mem_fetch*){}
#endif

struct victim_fixture : fixture {
  unsigned long long lower_read_packets = 0, lower_read_bytes = 0;
  unsigned long long previous_queries = 0;
  explicit victim_fixture(char **argv, const char *queues="512:2:64:64",
      const char *geometry="S:64:128:16,L:B:m:L:L,A:128:1,256:0,32") : fixture(argv,queues,geometry) {
    assert(cache->ep_l2_payload_for_test().physical_slots()==1168);
    assert(cache->ep_l2_payload_for_test().bank_service_limit()==1);
    assert(cache->ep_l2_descriptor_pool_capacity()==512);
  }
  void lower() {
    while(!sp->L2_dram_queue_empty()) {
      auto r=sp->L2_dram_queue_top(); sp->L2_dram_queue_pop();
      if(r->get_access_type()==L2_WRBK_ACC) {
        assert(r->has_ft_wb_data());
        for(unsigned sector=0;sector<4;++sector)
          if(r->get_access_sector_mask().test(sector))
            assert(r->get_ft_wb_data()[sector]==memory.at(r->get_addr())[sector]);
        assert(cache->ep_l2_wad_contains(r->get_addr()));
        if (cache->tvd_enabled()) {
          const auto row=cache->tvd_for_test()->find(r->get_addr());
          assert(cache->tvd_for_test()->live(row));
          assert(cache->tvd_for_test()->at(row.index).line == r->get_addr());
          assert(cache->tvd_for_test()->at(row.index).status == l2_cache::tvd_directory::WB_INFLIGHT);
          assert(cache->tvd_for_test()->writeback_matches(row,r->get_addr(),r->get_request_uid()));
        }
        ++lower_wb; wb.push_back(r);
      } else {
        assert(!r->get_is_write());
        ++lower_read_packets; lower_read_bytes+=r->get_data_size();
        assert(reads.insert({r->get_addr(),r}).second);
      }
    }
  }
  void collect() {
    while(auto r=sp->pop()) {
      diag_test_witness(3,gpu->gpu_sim_cycle,r);
      const unsigned uid=r->get_request_uid();
      assert(expected.count(uid) && replies.insert(uid).second);
      if(!r->get_is_write()) {
        assert(r->has_ft_wb_data());
        for(unsigned sector=0;sector<4;++sector)
          if(r->get_access_sector_mask().test(sector))
            assert(r->get_ft_wb_data()[sector]==expected.at(uid)[sector]);
      }
      delete r;
    }
  }
  void step(bool response_consumer_ready=true) {
    unsigned long long grants[4];
    for(unsigned b=0;b<4;++b) grants[b]=cache->ep_l2_payload_bank_grants(b);
    part->cache_cycle(gpu->gpu_sim_cycle);
    for(unsigned b=0;b<4;++b) {
      auto used=cache->ep_l2_payload_bank_grants(b)-grants[b];
      assert(used<=1); bank_max=std::max(bank_max,used);
    }
    if(cache->tvd_enabled()) {
      auto q=cache->tvd_lookup_queries_for_test();
      assert(q>=previous_queries && q-previous_queries<=1);
      previous_queries=q;
    }
    cache->ft_assert_quota();
    assert(cache->ep_l2_payload_for_test().ownership_consistent());
    assert(cache->ep_l2_payload_for_test().fixed_quota_consistent());
    if(response_consumer_ready) collect();
    if(!block_lower) lower();
    ++gpu->gpu_sim_cycle;
  }
  void return_all_reads_reverse() {
    std::vector<new_addr_type> addresses;
    for(auto r:reads) addresses.push_back(r.first);
    for(auto i=addresses.rbegin();i!=addresses.rend();++i) return_read(*i);
  }
  void wait(unsigned uid, bool return_memory=true) {
    for(unsigned n=0;n<20000 && !replies.count(uid);++n) {
      step(); if(return_memory) return_all_reads_reverse();
    }
    assert(replies.count(uid));
  }
  unsigned read(new_addr_type address) {
    auto r=make(address,false); const auto uid=r->get_request_uid();send(r);wait(uid);return uid;
  }
  unsigned write(new_addr_type address) {
    auto r=make(address,true);const auto uid=r->get_request_uid();
    const auto line=address&~127ull;const unsigned sector=(address>>5)&3;
    if(!memory.count(line)) memory[line]={{line+11,line+22,line+33,line+44}};
    memory[line][sector]=expected.at(uid)[sector];send(r);wait(uid);return uid;
  }
  void complete_writebacks() {
    while(!wb.empty()) {
      auto packet=wb.front();wb.erase(wb.begin());
      assert(cache->ep_l2_wad_contains(packet->get_addr()));
      // The independent packet must preserve its original snapshot even if
      // a later cached writer eventually changes the same line.
      sp->set_done(packet);delete packet;++completed_wb;
      step(); // Respect the explicit single completion-CAM port per cycle.
    }
  }
  void finish() {
    for(unsigned n=0;n<20000 && (part->busy() || !reads.empty() || !wb.empty());++n) {
      step();return_all_reads_reverse();complete_writebacks();
    }
    assert(!part->busy() && reads.empty() && wb.empty());
    assert(cache->ft_drained() && !cache->ep_l2_wad_occupancy());
    assert(!cache->ep_l2_payload_pending_operations());
    assert(cache->tvd_actual_lower_read_packets()==lower_read_packets);
    assert(cache->tvd_actual_lower_read_bytes()==lower_read_bytes);
    sp->flushL2();
    if(cache->tvd_enabled()) assert(!cache->tvd_for_test()->occupied());
    assert(!cache->ep_l2_payload_for_test().tvd_used());
  }
};
