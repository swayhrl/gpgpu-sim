// Real native cache/Tag/MSHR/WB path, no injected queue or arbiter state.
#include "micro_fixture.h"
int main(int argc,char **argv) {
  assert(argc==5 || argc==6);setvbuf(stdout,NULL,_IOLBF,0);
  unsigned mode=std::stoul(argv[3]);bool enabled=std::stoul(argv[4]);
  std::string variant=argc==6?argv[5]:"ordinary";
  bool bypass=enabled && variant=="ordinary";
  micro_fixture f(argv[1],argv[2],mode,16,64,1);
  const new_addr_type stride=64*128;
  std::map<new_addr_type,std::array<unsigned long long,4>> ref;
  for(unsigned i=0;i<16;++i){auto w=f.make(i*stride,true);assert(f.access(w));
    ref[w->get_addr()]={{(unsigned long long)w->get_request_uid()+1,0,0,0}};delete w;}
  std::vector<mem_fetch *> fills;
  for(unsigned i=0;i<4;++i){auto r=f.make(128+i*stride,false,GLOBAL_ACC_R);assert(f.access(r));fills.push_back(r);}
  f.port.blocked=false;for(unsigned i=0;i<4;++i)f.cycle();
  assert(f.port.packets.size()==4);f.port.packets.clear();f.port.blocked=true;
  auto w=f.make(16*stride,true);assert(f.access(w));delete w;
  assert(f.staged()==1 && f.wad()==1 && f.cache->miss_queue_occupancy()==1);
  // Same-line demand must remain behind the dirty WB's live WAD, before
  // even acquiring a MissQ descriptor. No illegal same-line queue injection.
  new_addr_type wb_owner=0;
  const auto &observed=f.cache->ep_l2_payload_for_test();
  for(unsigned i=0;i<observed.physical_slots();++i)
    if(observed.resident(i).status==ep_l2_payload_store::WB_STAGED)wb_owner=observed.resident(i).owner;
  auto same=f.make(wb_owner,false,GLOBAL_ACC_R,true);std::list<cache_event> ev;
  assert(f.cache->access(same->get_addr(),same,f.time,ev)==RESERVATION_FAIL);
  assert(f.cache->miss_queue_occupancy()==1 && f.wad()==1);delete same;
  auto read=f.make(256+32,false,GLOBAL_ACC_R,variant!="unknown",16);assert(f.access(read));
  assert(f.cache->miss_queue_occupancy()==2);
  const auto read_uid=read->get_request_uid();const auto read_bytes=read->get_access_byte_mask();
  const auto read_sectors=read->get_access_sector_mask();
  assert(read_bytes.count()==16 && read_sectors.count()==1 && read_sectors.test(1));
  const auto &store=f.cache->ep_l2_payload_for_test();unsigned wb_bank=99;
  for(unsigned i=0;i<store.physical_slots();++i)
    if(store.resident(i).status==ep_l2_payload_store::WB_STAGED)wb_bank=i%4;
  assert(wb_bank<4);mem_fetch *fill=nullptr;
  for(auto r:fills)if(r->get_ep_l2_payload_id()%4==wb_bank)fill=r;
  assert(fill);std::array<unsigned long long,4> data={{101,202,303,404}};fill->set_ft_wb_data(data);
  assert(f.cache->ep_l2_payload_fill_request(fill,f.time)==ep_l2_payload_store::GRANTED);
  f.cache->fill(fill,1000000);
  f.port.blocked=variant=="lower-full";
  unsigned bank=fill->get_ep_l2_payload_id()%4;
  unsigned before=f.cache->ep_l2_payload_bank_grants(bank);
  f.cycle();
  assert(f.cache->ep_l2_payload_bank_grants(bank)==before);
  assert(f.staged()==1 && f.wad()==1);
  if(bypass){assert(f.port.packets.size()==1 && f.port.packets.front()==read);f.port.packets.pop_front();}
  else assert(f.port.packets.empty());
  printf("MQ2 opportunity real fill-bank/WB-head/independent-read mode=%u enabled=%u bypass=%zu PASS\n",mode,enabled,bypass?size_t(1):size_t(0));
  // On recovery the original WB wins; identity/sector versions survive exactly.
  f.port.blocked=false;f.cycle();assert(!f.port.packets.empty());auto wb=f.port.packets.front();f.port.packets.pop_front();
  assert(wb->get_access_type()==L2_WRBK_ACC && wb->get_ft_wb_data()==ref.at(wb->get_addr()));
  assert(!f.staged() && f.cache->ep_l2_wad_contains(wb->get_addr()));
  f.cache->ep_l2_wad_complete(wb->get_addr(),f.time);delete wb;
  if(!bypass){f.cycle();assert(f.port.packets.size()==1 && f.port.packets.front()==read);f.port.packets.pop_front();}
  assert(read->get_request_uid()==read_uid && read->get_access_byte_mask()==read_bytes && read->get_access_sector_mask()==read_sectors);
  read->set_ft_wb_data(data);
  for(unsigned n=0;f.cache->ep_l2_payload_fill_request(read,f.time)!=ep_l2_payload_store::GRANTED;++n){assert(n<100);f.cycle();}
  f.cache->fill(read,2000000);
  for(auto r:fills)if(r!=fill){r->set_ft_wb_data(data);
    for(unsigned n=0;f.cache->ep_l2_payload_fill_request(r,f.time)!=ep_l2_payload_store::GRANTED;++n){assert(n<100);f.cycle();}
    f.cache->fill(r,2000000);}
  std::set<unsigned> seen;
  for(unsigned n=0;seen.size()!=5;++n){assert(n<100);f.cycle();
    if(f.cache->access_ready() && f.cache->ft_response_ready(f.time)){
      auto r=f.cache->peek_next_access();assert(seen.insert(r->get_request_uid()).second);
      for(unsigned s=0;s<4;++s)assert(r->get_ft_wb_data()[s]==(r->get_access_sector_mask().test(s)?data[s]:0));f.cache->commit_next_access();}}
  for(auto r:fills)delete r;delete read;
  assert(f.cache->ft_drained() && !f.wad() && f.cache->miss_queue_empty());
  assert(!f.cache->ep_l2_payload_pending_operations());
  for(unsigned b=0;b<4;++b)assert(f.cache->ep_l2_payload_for_test().bank_peak(b)<=1);
  f.cache->ft_print(stdout,0);
  puts("MQ2 native exact-once/versions/lower-accept/WAD/head-priority/physical-P1/drain PASS");
}
