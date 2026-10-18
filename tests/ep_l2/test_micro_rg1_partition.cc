// RG1 opportunity in actual partition FIFOs and production cycle ordering.
#include "micro_partition_fixture.h"
int main(int argc,char **argv) {
  assert(argc>=8 && argc<=10);bool enabled=argc>=9 && std::stoul(argv[8]);setvbuf(stdout,NULL,_IOLBF,0);fixture f(argv);
  f.gpu->gpu_tot_sim_cycle=argc==10?std::stoul(argv[9]):0;
  mem_access_sector_mask_t host_mask;host_mask.set(0);
  assert(f.cache->ft_try_force_tag_access(512,1000000,host_mask));
  assert(f.cache->ep_l2_payload_bank_grants()==0);
  const new_addr_type stride=64*128;
  auto seed=f.make(0,true);f.send(seed);f.until_frontend_empty();
  std::vector<mem_fetch *> reads;
  for(unsigned i=0;i<8;++i){auto r=f.make(128+i*stride,false);reads.push_back(r);f.send(r);f.until_frontend_empty();}
  assert(f.reads.size()==8 && !f.sp->ep_l2_l2icnt_occupancy());
  unsigned bank=reads.front()->get_ep_l2_payload_id()%4;
  auto h=f.cache->ep_l2_tag_payload_handle_for_test(15);
  assert(h.valid() && h.payload_id%4==bank);
  mem_fetch *other=nullptr;
  for(auto r:reads)if(r!=reads.front() && r->get_ep_l2_payload_id()%4==bank)other=r;
  assert(other);f.consume_responses=false;
  // Move an ordinary hit write through the real ROP into frontend queue.
  auto hit=f.make(0,true);f.send(hit);f.step();
  assert(f.sp->ep_l2_icntl2_occupancy()==1);
  // Fill first: its bank write denies the frontend hit write in this cycle.
  f.return_read(reads.front()->get_addr());f.step();
  assert(f.cache->access_ready() && f.sp->ep_l2_icntl2_occupancy()==1);
  // Older frontend operation consumes bank; hit WRITE_ACK fills actual ICNT FIFO.
  f.step();assert(f.sp->ep_l2_l2icnt_occupancy()==1 && f.cache->access_ready());
  f.return_read(other->get_addr());
  auto before=f.cache->ep_l2_payload_bank_grants(bank);
  f.step();
  assert(f.sp->ep_l2_l2icnt_occupancy()==1);
  assert(f.sp->ep_l2_draml2_occupancy()==(enabled?0u:1u));
  assert(f.cache->ep_l2_payload_bank_grants(bank)==before+(enabled?1u:0u));
#ifdef EPL2_MICROOPT_CANDIDATE
  if(enabled)assert(f.cache->ep_l2_payload_for_test().rg1_recovered(bank)>=1);
#endif
  printf("RG1 real partition response FIFO full, retained response bank%u, ReturnQ fill ready, services=%u enabled=%u PASS\n",bank,enabled?1u:0u,enabled);
  // Drain the real response FIFO and resume the retained oldest owner.
  auto ack=f.sp->pop();assert(ack==hit);assert(f.replies.insert(ack->get_request_uid()).second);delete ack;
  f.consume_responses=true;
  for(unsigned n=0;f.sp->ep_l2_draml2_occupancy();++n){assert(n<100);f.step();}
  std::vector<new_addr_type> remaining;for(auto &p:f.reads)remaining.push_back(p.first);
  for(auto a:remaining){f.return_read(a);for(unsigned n=0;f.sp->ep_l2_draml2_occupancy();++n){assert(n<100);f.step();}}
  for(unsigned n=0;!f.cache->ft_drained();++n){assert(n<100);f.step();}
  assert(f.replies.size()==10 && f.reads.empty() && !f.cache->ep_l2_payload_pending_operations());
  assert(!f.cache->ep_l2_wad_occupancy() && f.cache->miss_queue_empty());
  f.cache->ft_print(stdout,0);puts("RG1 actual partition exact-once/sector-version/physical-P1/drain PASS");
}
