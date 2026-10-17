#include "potential_fixture.h"
#include <sys/wait.h>
#include <unistd.h>
static void layout() {
  using P=ep_l2_payload_store;
  P p(2,1,16,false,true);assert(p.physical_slots()==1168 && p.bypass_capacity()==80);
  unsigned counts[3][4]={};
  for(unsigned r=0;r<3;++r) {
    unsigned n=r==0?1024:r==1?80:64;
    for(unsigned i=0;i<n;++i) {
      auto h=p.preview_free((P::role)r);assert(h.valid());
      unsigned first=r==0?0:r==1?1024:1104;
      assert(h.payload_id==first+i);
      p.reserve(h.payload_id,(P::role)r,128*(h.payload_id+1),r==2?P::WB_RESERVED:r==1?P::BYPASS_FILL_PENDING:P::RESIDENT_FILL_PENDING);
      ++counts[r][h.payload_id%4];
    }
    assert(!p.preview_free((P::role)r).valid());
  }
  for(unsigned b=0;b<4;++b)assert(counts[0][b]==256 && counts[1][b]==20 && counts[2][b]==16);
  assert(p.free_slots()==0 && p.ownership_consistent());
  for(unsigned id=0;id<1168;++id)p.release(P::payload_handle(id,1));
  assert(p.free_slots()==1168 && !p.wb_used());
  for(unsigned c:{8u,16u}) {P old(2,1,c);assert(old.physical_slots()==1152+c && old.bypass_capacity()==128 && old.wb_begin()==1152);}
  bool rejected=false;try{P invalid(2,2,16,false,true);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
  for(unsigned which=0;which<3;++which) {
    pid_t pid=fork();assert(pid>=0);
    if(!pid){if(which==0)p.reserve(1168,P::WB,1,P::WB_RESERVED);if(which==1)p.reserve_bypass(80,1,nullptr);if(which==2)p.reserve(1103,P::WB,1,P::WB_RESERVED);_exit(0);}
    int status;assert(waitpid(pid,&status,0)==pid && WIFSIGNALED(status) && WTERMSIG(status)==SIGABRT);
  }
  puts("S64 layout boundaries/bank counts/full-release/illegal combinations/default B8 B16 PASS");
}
int main(int argc,char **argv) {
  assert(argc==8 && std::stoul(argv[3])==1 && std::stoul(argv[5])==1);
  setvbuf(stdout,NULL,_IOLBF,0);layout();fixture f(argv);f.block_lower=true;
  const new_addr_type stride=64*128;
  // No synthetic array occupancy: native dirty writes, real WB packets and
  // a full two-entry lower FIFO accumulate 64 local staging responsibilities.
  for(unsigned i=0;i<82;++i) {
    // Make way0 (bank0) the first LRU victim to cover a true same-bank
    // source-read/staging-write pair; the normal descending sequence also
    // supplies cross-bank pairs. Touches use actual frontend hit writes.
    if(i==16) for(unsigned j=0;j<15;++j) {
      auto hit=f.make(j*stride,true);f.memory[j*stride]=f.expected.at(hit->get_request_uid());
      f.send(hit);f.until_frontend_empty();
    }
    auto r=f.make(i*stride,true);f.memory[i*stride]=f.expected.at(r->get_request_uid());
    f.send(r);f.until_frontend_empty();
  }
  assert(f.sp->ep_l2_l2dram_occupancy()==2 && f.cache->ft_wb_staged()==64);
  assert(f.cache->ep_l2_wad_occupancy()==66 && f.cache->ep_l2_wad_capacity()==128);
  auto r=f.make(82*stride,true);unsigned uid=r->get_request_uid();f.memory[82*stride]=f.expected.at(uid);f.send(r);
  for(unsigned n=0;n<8;++n)f.step();
  assert(!f.replies.count(uid) && !f.cache->ft_reservation_active() && f.cache->ft_wb_staged()==64);
  const auto &p=f.cache->ep_l2_payload_for_test();
  assert(p.wb_used()==64 && p.ownership_consistent());
  for(unsigned n=0;n<8;++n)f.step();
  assert(f.cache->ft_wb_staged()==64 && f.cache->ep_l2_wad_occupancy()==66 && !f.cache->ft_reservation_active());
  puts("S64 production local WB peak=64 (>16), 65th refusal/no partial commit/lower full PASS");
  f.block_lower=false;f.drain_lower();
  for(unsigned n=0;n<2000 && (!f.replies.count(uid) || f.cache->ft_wb_staged() || !f.cache->miss_queue_empty() || !f.sp->L2_dram_queue_empty());++n)f.step();
  assert(f.replies.count(uid) && f.lower_wb==67 && !f.cache->ft_wb_staged());
  assert(f.cache->ep_l2_wad_occupancy()==67);
  for(auto w:f.wb){f.sp->set_done(w);delete w;++f.completed_wb;}f.wb.clear();
  for(unsigned n=0;n<1000 && f.part->busy();++n)f.step();
  assert(f.completed_wb==67 && f.replies.size()==98 && f.replies.size()==f.expected.size());
  assert(f.sp->l2_char_no_resource_leak() && f.cache->ft_drained() && !f.part->busy());
  assert(!f.cache->ep_l2_payload_pending_operations() && !f.cache->ep_l2_wad_occupancy());
  assert(f.cache->ep_l2_payload_bank_wb_readout()>=134 && f.bank_max==1);
  printf("S64 real copy source/staging banks: same=%u cross=%u\n",f.same_bank_copy,f.cross_bank_copy);
  assert(f.same_bank_copy && f.cross_bank_copy);
  f.cache->ft_print(stdout,0);
  puts("S64 production versions/exact-once/release-resume/copy stages/WAD128/drain PASS");
}
