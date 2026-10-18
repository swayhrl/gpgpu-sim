// Real native cache/Tag/MSHR/WB path, no injected queue or arbiter state.
#include "micro_fixture.h"
int main(int argc,char **argv) {
  assert(argc==5);setvbuf(stdout,NULL,_IOLBF,0);
  unsigned mode=std::stoul(argv[3]);bool enabled=std::stoul(argv[4]);
  micro_fixture f(argv[1],argv[2],mode,16,64,1);
  const new_addr_type stride=64*128;
  std::map<new_addr_type,std::array<unsigned long long,4>> ref;
  for(unsigned i=0;i<16;++i){auto w=f.make(i*stride,true);assert(f.access(w));
    ref[w->get_addr()]={{(unsigned long long)w->get_request_uid()+1,0,0,0}};delete w;}
  std::vector<mem_fetch *> fills;
  for(unsigned i=0;i<8;++i){auto r=f.make(128+i*stride,false,GLOBAL_ACC_R);assert(f.access(r));fills.push_back(r);}
  f.port.blocked=false;for(unsigned i=0;i<8;++i)f.cycle();
  assert(f.port.packets.size()==8);f.port.packets.clear();f.port.blocked=true;
  auto w=f.make(16*stride,true);assert(f.access(w));delete w;
  assert(f.staged()==1 && f.wad()==1 && f.cache->miss_queue_occupancy()==1);
  auto read=f.make(256,false,GLOBAL_ACC_R,true);assert(f.access(read));
  assert(f.cache->miss_queue_occupancy()==2);
  const auto &store=f.cache->ep_l2_payload_for_test();unsigned wb_bank=99;
  for(unsigned i=0;i<store.physical_slots();++i)
    if(store.resident(i).status==ep_l2_payload_store::WB_STAGED)wb_bank=i%4;
  assert(wb_bank<4);mem_fetch *fill=nullptr;
  for(auto r:fills)if(r->get_ep_l2_payload_id()%4==wb_bank)fill=r;
  assert(fill);std::array<unsigned long long,4> data={{101,0,0,0}};fill->set_ft_wb_data(data);
  assert(f.cache->ep_l2_payload_fill_request(fill,f.time)==ep_l2_payload_store::GRANTED);
  f.cache->fill(fill,1000000);
  f.port.blocked=false;
  unsigned bank=fill->get_ep_l2_payload_id()%4;
  unsigned before=f.cache->ep_l2_payload_bank_grants(bank);
  f.cycle();
  assert(f.cache->ep_l2_payload_bank_grants(bank)==before);
  assert(f.staged()==1 && f.wad()==1);
  if(enabled){assert(f.port.packets.size()==1 && f.port.packets.front()==read);f.port.packets.pop_front();}
  else assert(f.port.packets.empty());
  printf("MQ2 opportunity real fill-bank/WB-head/independent-read mode=%u enabled=%u bypass=%zu PASS\n",mode,enabled,enabled?size_t(1):size_t(0));
  // Real external lower FIFO backpressure makes the retained WB explicitly
  // unready. A second actual returned fill to that bank is ready, but denied
  // while zero physical services have been consumed this cycle.
  mem_fetch *other=nullptr;
  for(auto r:fills)if(r!=fill && r->get_ep_l2_payload_id()%4==bank)other=r;
  assert(other);other->set_ft_wb_data(data);f.port.blocked=true;
  unsigned held_before=f.cache->ep_l2_payload_bank_grants(bank);
  assert(f.cache->ep_l2_payload_fill_request(other,f.time)==ep_l2_payload_store::BANK_TRUE_CONTENTION);
  assert(f.cache->ep_l2_payload_bank_grants(bank)==held_before);
  f.cycle();assert(f.cache->ep_l2_payload_bank_grants(bank)==held_before);
  printf("RG1 opportunity retained WB unready(lower full), actual fill ready bank%u physical services=0 PASS\n",bank);
  f.port.blocked=false;
  // On recovery the original WB wins; identity/sector versions survive exactly.
  f.cycle();assert(!f.port.packets.empty());auto wb=f.port.packets.front();f.port.packets.pop_front();
  assert(wb->get_access_type()==L2_WRBK_ACC && wb->get_ft_wb_data()==ref.at(wb->get_addr()));
  assert(!f.staged() && f.cache->ep_l2_wad_contains(wb->get_addr()));
  f.cache->ep_l2_wad_complete(wb->get_addr(),f.time);delete wb;
  if(!enabled){f.cycle();assert(f.port.packets.size()==1 && f.port.packets.front()==read);f.port.packets.pop_front();}
  for(unsigned n=0;f.cache->ep_l2_payload_fill_request(other,f.time)!=ep_l2_payload_store::GRANTED;++n){assert(n<100);f.cycle();}
  f.cache->fill(other,1500000);
  read->set_ft_wb_data(data);
  for(unsigned n=0;f.cache->ep_l2_payload_fill_request(read,f.time)!=ep_l2_payload_store::GRANTED;++n){assert(n<100);f.cycle();}
  f.cache->fill(read,2000000);
  for(auto r:fills)if(r!=fill && r!=other){r->set_ft_wb_data(data);
    for(unsigned n=0;f.cache->ep_l2_payload_fill_request(r,f.time)!=ep_l2_payload_store::GRANTED;++n){assert(n<100);f.cycle();}
    f.cache->fill(r,2000000);}
  std::set<unsigned> seen;
  for(unsigned n=0;seen.size()!=9;++n){assert(n<100);f.cycle();
    if(f.cache->access_ready() && f.cache->ft_response_ready(f.time)){
      auto r=f.cache->peek_next_access();assert(r->get_ft_wb_data()==data && seen.insert(r->get_request_uid()).second);f.cache->commit_next_access();}}
  for(auto r:fills)delete r;delete read;
  assert(f.cache->ft_drained() && !f.wad() && f.cache->miss_queue_empty());
  assert(!f.cache->ep_l2_payload_pending_operations());
  for(unsigned b=0;b<4;++b)assert(f.cache->ep_l2_payload_for_test().bank_peak(b)<=1);
  f.cache->ft_print(stdout,0);
  puts("MQ2 native exact-once/versions/lower-accept/WAD/head-priority/physical-P1/drain PASS");
}
