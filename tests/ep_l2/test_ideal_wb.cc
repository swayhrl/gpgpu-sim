#include "ft_test_fixture.h"

static void extra_data_only(const char *base,const char *overlay,unsigned c) {
  fixture f(base,overlay,1,c,1,true);
  std::map<new_addr_type,std::array<unsigned long long,4>> reference;
  const unsigned writes=c+26;
  for(unsigned i=0;i<writes;++i) {
    auto w=f.make(i*128,true,i%2?L1_WRBK_ACC:GLOBAL_ACC_W);
    assert(f.access(w));
    reference[i*128]={{static_cast<unsigned long long>(w->get_request_uid())+1,0,0,0}};
    delete w;
  }
  assert(f.staged()==writes-1 && f.wad()==writes-1 && f.staged()>c);
  assert(f.cache->ideal_wb_data_full()==0);
  auto &pool=f.cache->ep_l2_payload_for_test();
  assert(pool.diagnostic_wb_extra_slots()>0);
  assert(pool.physical_slots()==1152+c+pool.diagnostic_wb_extra_slots());
  // Capacity relief grants no free copy service: all staged WBs performed
  // both the real source read and staging write through the common arbiter.
  assert(f.cache->ft_copy_lines()==writes-1);
  assert(f.cache->ft_staging_reads()==0 && f.cache->ft_lower_accepts()==0);
  f.drain_wb(reference);
  assert(f.cache->ft_staging_reads()==writes-1 && f.cache->ft_lower_accepts()==writes-1);
  assert(f.cache->ideal_wb_data_full()==0);
  f.cache->ft_print(stdout,0);
  std::printf("I_WB extra storage/copy/read/packet-version/drain C=%u PASS; extra=%u\n",c,pool.diagnostic_wb_extra_slots());
}

static void wad_still_binds(const char *base,const char *overlay) {
  fixture f(base,overlay,1,16,1,true);
  std::map<new_addr_type,std::array<unsigned long long,4>> reference;
  for(unsigned i=0;i<129;++i) {
    auto w=f.make(i*128,true);assert(f.access(w));
    reference[i*128]={{static_cast<unsigned long long>(w->get_request_uid())+1,0,0,0}};
    delete w;
  }
  assert(f.wad()==128 && f.staged()==128 && f.cache->ideal_wb_data_full()==0);
  auto resident=f.resident();const auto before=f.cache->ep_l2_payload_for_test().inspect(resident);
  auto blocked=f.make(129*128,true);
  assert(!f.access(blocked));
  assert(f.resident()==resident && !f.cache->ft_reservation_active());
  assert(f.cache->ep_l2_payload_for_test().inspect(resident).versions==before.versions);
  assert(f.cache->ep_l2_wad_full_blocks()>0 && f.cache->ideal_wb_data_full()==0);
  delete blocked;f.drain_wb(reference);
  std::puts("I_WB WAD128/ordering/no-partial-mutation remains binding PASS");
}

static void combined_pressure(const char *base,const char *overlay) {
  fixture f(base,overlay,1,16,1024,true,true);
  std::map<new_addr_type,std::array<unsigned long long,4>> memory;
  for(unsigned i=0;i<50;++i) {
    auto w=f.make(i*128,true);assert(f.access(w));
    memory[i*128]={{static_cast<unsigned long long>(w->get_request_uid())+1,0,0,0}};delete w;
  }
  for(unsigned i=0;i<50;++i) {
    auto w=f.make((1024+i)*128,true);assert(f.access(w));
    memory[(1024+i)*128]={{static_cast<unsigned long long>(w->get_request_uid())+1,0,0,0}};delete w;
  }
  std::set<mem_fetch *> expected,responded;
  for(unsigned i=200;i<360;++i) for(unsigned n=0;n<4;++n) {
    auto r=f.make(i*128,false,GLOBAL_ACC_R);assert(f.access(r));expected.insert(r);
  }
  for(unsigned n=0;n<40;++n) {
    auto r=f.make(200*128,false,GLOBAL_ACC_R);assert(f.access(r));expected.insert(r);
  }
  assert(f.staged()==50 && f.wad()==50 && f.cache->mshr_entries_used()==160);
  assert(f.cache->ep_l2_descriptor_count_used()==680);
  assert(f.cache->ideal_wb_data_full()==0);
  f.port.blocked=false;
  for(unsigned n=0;n<100000 && (f.staged() || f.cache->mshr_entries_used() ||
      !f.cache->miss_queue_empty() || !f.port.packets.empty());++n) {
    f.cycle();
    while(!f.port.packets.empty()) {
      auto packet=f.port.packets.front();f.port.packets.pop_front();
      if(packet->get_access_type()==L2_WRBK_ACC) {
        assert(packet->get_ft_wb_data()==memory.at(packet->get_addr()));
        assert(f.cache->ep_l2_wad_contains(packet->get_addr()));
        f.cache->ep_l2_wad_complete(packet->get_addr(),f.time);delete packet;
      } else {
        // These untouched lines have an explicitly initialized version-zero
        // reference; production fills/merged responses must retain that value.
        std::array<unsigned long long,4> initial={{0,0,0,0}};
        packet->set_ft_wb_data(initial);
        while(f.cache->ep_l2_payload_fill_request(packet,f.time)!=ep_l2_payload_store::GRANTED)f.cycle();
        f.cache->fill(packet,f.time);f.cycle();
      }
    }
    while(f.cache->access_ready()) {
      if(!f.cache->ft_response_ready(f.time)) {f.cycle();continue;}
      auto r=f.cache->peek_next_access();
      assert(expected.count(r) && responded.insert(r).second);
      assert(r->get_ft_wb_data()[0]==0);
      f.cache->commit_next_access();delete r;f.cycle();
    }
  }
  assert(responded==expected && !f.staged() && !f.wad());
  assert(f.cache->ep_l2_descriptor_count_used()==0 && f.cache->ep_l2_payload_pending_operations()==0);
  assert(f.cache->ft_copy_lines()==50 && f.cache->ft_staging_reads()==50 && f.cache->ft_lower_accepts()==50);
  std::puts("I_WB_META simultaneous 50 WB/160 lines/680 requester/version/retirement pressure PASS");
}

int main(int argc,char **argv) {
  assert(argc==3);
  extra_data_only(argv[1],argv[2],8);extra_data_only(argv[1],argv[2],16);
  wad_still_binds(argv[1],argv[2]);
  combined_pressure(argv[1],argv[2]);
  std::puts("Lane4 I_WB production diagnostic PASS");
}
