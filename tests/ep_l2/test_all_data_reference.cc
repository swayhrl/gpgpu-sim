#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../src/gpgpu-sim/ep-l2-all-data-reference.h"
using Service=ep_l2_all_data::Service;
static Service::Mask mask(unsigned from,unsigned count) {
  Service::Mask m{};for (unsigned b=from;b<from+count;++b)m[b]=true;return m;
}
int main(int argc,char **argv) {
  if (argc==2 && std::strcmp(argv[1],"--duplicate-access")==0) {
    Service s;auto a=s.begin(1,1,false,11),b=s.begin(2,1,false,11);
    s.issue(a,0,0,mask(0,4),0,nullptr,123);s.issue(b,0,128,mask(0,4),0,nullptr,123);assert(false);
  }
  if (argc==2) {Service s;s.begin(1,0,false,11);s.begin(1,0,false,11);assert(false);}
  (void)argv;
  {
    Service s;Service::Data first{},second{};first.fill(11);second.fill(22);
    auto w=s.begin(1,1,true,11);auto write=s.issue(w,0,0x1000,mask(0,4),0,&first);
    s.annotate(w,1,0x100,4,0,0,7);
    auto r=s.begin(2,1,false,11);auto read=s.issue(r,0,0x1000,mask(0,8),0);
    s.annotate(r,1,0x200,4,1,3,9); // Same producer address, another SM/warp.
    auto free=s.begin(3,1,false,11);auto independent=s.issue(free,0,0x1000,mask(32,4),0);
    auto younger=s.begin(4,1,true,11);auto overwrite=s.issue(younger,0,0x1000,mask(0,4),0,&second);
    assert(!s.inspect(read).eligible_valid && s.inspect(independent).ready==1);
    assert(!s.host_prefill(0x1000,128));
    s.advance(1);assert(s.ready(write) && s.ready(independent));
    assert(s.inspect(read).eligible==1 && s.inspect(read).ready==2);
    assert(s.inspect(overwrite).eligible==1 && s.inspect(overwrite).ready==2);
    auto snapshot=s.inspect(read).snapshot;
    s.consume(write,1);s.finish(w);s.consume(independent,1);s.finish(free);
    s.advance(2);assert(s.ready(read) && s.ready(overwrite));
    assert(snapshot==s.inspect(read).snapshot);
    auto value=s.consume(read,2);s.finish(r);s.consume(overwrite,2);s.finish(younger);
    for (unsigned b=0;b<8;++b) {
      if (b<4)assert(value[b].value_known && value[b].value==11 && value[b].version>0);
      else assert(!value[b].value_known && value[b].version==0);
    }
    assert(s.store_reference_commits==2 && s.drained());
    assert(s.host_prefill(0x1001,2));
    auto n=s.begin(5,1,false,11);auto rr=s.issue(n,0,0x1000,mask(0,4),2);
    s.advance(3);auto v=s.consume(rr,3);s.finish(n);
    assert(v[0].value_known && v[0].value==22);
    assert(!v[1].value_known && !v[2].value_known);
    assert(v[1].version==0 && v[2].version==0);
    assert(v[3].value_known && v[3].value==22);
    auto later=s.begin(6,1,true,11);auto ww=s.issue(later,0,0x1000,mask(1,1),3,&first);
    s.advance(4);s.consume(ww,4);s.finish(later);
    auto after=s.begin(7,1,false,11);auto ar=s.issue(after,0,0x1000,mask(0,4),4);
    s.advance(5);auto av=s.consume(ar,5);s.finish(after);
    assert(av[1].value_known && av[1].value==11 && av[1].version>v[0].version);
    assert(av[0]==v[0] && av[2]==v[2] && av[3]==v[3]);
    assert(s.store_reference_commits==3 && s.drained());
    s.print(stdout);
  }
  {
    // More simultaneous responses than every native response FIFO and quota.
    Service s;auto instruction=s.begin(100,1024,false,11);std::vector<uint64_t> ids;
    for (unsigned i=0;i<1024;++i) ids.push_back(s.issue(instruction,0,0x100000+i*128,mask(0,32),0));
    s.advance(1);
    assert(s.requests_ready==1024);
    for (auto id:ids) {assert(s.inspect(id).ready==s.inspect(id).eligible+1);s.consume(id,1);}
    s.finish(instruction);assert(s.drained() && s.requests_consumed==1024);s.print(stdout);
  }
  {
    Service s;auto a=s.new_shared_epoch(0,0),b=s.new_shared_epoch(1,0);
    Service::Data data{};data.fill(99);auto i=s.begin(1,1,true,3);auto w=s.issue(i,a,0,mask(0,4),0,&data);
    s.advance(1);s.consume(w,1);s.finish(i);
    auto j=s.begin(2,1,false,3);auto r=s.issue(j,b,0,mask(0,4),1);
    s.advance(2);for (auto v:s.consume(r,2)) assert(v.version==0 && !v.value_known);s.finish(j);
    auto c=s.new_shared_epoch(0,0);assert(c!=a);
    auto k=s.begin(3,1,false,3);auto fresh=s.issue(k,c,0,mask(0,4),2);
    s.advance(3);for (auto v:s.consume(fresh,3)) assert(v.version==0 && !v.value_known);s.finish(k);
    assert(s.drained());
  }
  std::puts("ALL_DATA_REFERENCE_RAW_WAR_MASK_HOST_CLOCK_BURST_SHARED_SCOPE PASS");
}
