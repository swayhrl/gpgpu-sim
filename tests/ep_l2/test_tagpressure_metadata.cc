// Bounded SYNTHETIC observer-metadata controls; not a complete application.
#ifdef NDEBUG
#error Tagpressure qualification controls require assertions enabled
#endif
#include "../../src/gpgpu-sim/ep-l2-tagpressure-diag.h"
#include <cassert>
#include <cstdio>
using namespace ep_l2_tagpressure;
static snapshot make(const void*owner,unsigned r,unsigned raw=2){
 snapshot s;s.cache_identity=owner;s.slice=0;s.physical_cycle=17;s.request_uid=23;
 s.assoc=16;s.raw=s.raw_r4=raw;s.ro_runtime=true;s.T=true;s.requester=CERTIFIED_RO_CANDIDATE;
 for(unsigned i=0;i<16;++i)s.ways[i].kind=i<r?RESERVED_WAY:INVALID_WAY;
 return s;
}
static uint64_t covered(const slice_state&s){uint64_t n=0;for(auto&c:s.phases)if(c)n+=c->covered;return n;}
int main(){int owner=0,other=1;unsigned checks=0;
 for(unsigned r:{0u,1u,15u,16u}){auto s=make(&owner,r,r==16?3:2);phase_counts c;c.observe(s,&owner,0,17,23);
  assert(c.basic_closure()&&c.covered==1&&c.heads_raw_reserved[s.raw*W+r]==1);
  assert(c.ways_raw_reserved_class[(s.raw*W+r)*4]==r);++checks;
 }
 {auto s=make(&owner,16,1);phase_counts c;c.observe(s,&owner,0,17,23);
  assert(c.basic_closure()&&c.heads_raw_reserved[W+16]==1);++checks;}
 {auto s=make(&owner,1);s.ways[0].live=s.ways[0].attached=true;s.ways[0].required=1;
  s.ways[1].live=s.ways[1].attached=s.ways[1].reuse=true;s.ways[1].required=3;s.ways[1].ready=3;
  s.ways[2].kind=REPLACEABLE_DIRTY;s.ways[3].kind=REPLACEABLE_CLEAN;
  phase_counts c;c.observe(s,&owner,0,17,23);unsigned k=2*W+1;
  assert(c.basic_closure()&&c.heads_raw_reserved_eligible[k*W+1]==1);
  assert(c.heads_raw_reserved_reasonmask[k*MASK+(HAS_ELIGIBLE|HAS_REUSE|HAS_NO_MISSING|HAS_NO_LIVE)]==1);
  assert(c.ways_raw_reserved_predmask[k*PRED+5]==1&&c.ways_raw_reserved_predmask[k*PRED+3]==1);++checks;
 }
 for(unsigned mismatch=0;mismatch<4;++mismatch){auto s=make(&owner,0);if(mismatch==0)s.cache_identity=&other;if(mismatch==1)s.slice=1;if(mismatch==2)++s.physical_cycle;if(mismatch==3)++s.request_uid;
  phase_counts c;c.observe(s,&owner,0,17,23);assert(c.invalid&&c.covered==0);
  std::unique_ptr<route_joint_table>j(new route_joint_table);j->record(0,0,&owner,17,23,s,NOT_CONSULTED,true,0);assert(j->invalid&&j->rows==0);++checks;
 }
 {auto s=make(&owner,0);s.ro_runtime=false;s.requester=RUNTIME_OFF;
  phase_counts c;c.observe(s,&owner,0,17,23);assert(c.basic_closure()&&c.ro_na==1&&c.ro_observed==0);
  for(auto n:c.heads_raw_reserved_eligible)assert(!n);++checks;
 }
 {auto s=make(&owner,0);s.T=false;s.ways[0].live=s.ways[0].attached=true;s.ways[0].required=1;
  phase_counts c;c.observe(s,&owner,0,17,23);assert(c.basic_closure()&&c.heads_raw_reserved_eligible[2*W*W+1]==1);++checks;
 }
 {std::unique_ptr<route_joint_table>j(new route_joint_table);auto s=make(&owner,16,3);
  for(unsigned req=0;req<2;++req){s.requester=req;j->record(0,0,&owner,17,23,s,req?PROD_NOT_REACHED:CONSULTED_MISS,!req,req?1:0);}
  assert(j->rows==2&&!j->invalid);unsigned seen=0;
  for(const auto&v:j->slots)if(v.key){auto a=route_joint_table::decode(v.key);assert(v.count==1&&a[2]==3&&a[3]==16&&a[4]<2);++seen;}
  assert(seen==2);++checks;
 }
 {auto s=make(&owner,0);s.ways[0].live=s.ways[0].attached=true;s.ways[0].required=15;
  s=snapshot();assert(!s.ways[0].live&&!s.ways[0].attached&&!s.ways[0].required&&!s.cache_identity);++checks;}
 {phase_counts c;c.covered=~uint64_t(0);c.add(c.covered);assert(c.invalid&&!c.basic_closure());++checks;}
 {assert(registry::budget_allows(0,OUTPUT_CAP-MARKER_RESERVE));assert(!registry::budget_allows(1,OUTPUT_CAP-MARKER_RESERVE));assert(!registry::budget_allows(~uint64_t(0),1));++checks;}
 {registry r;r.configured=r.enabled=true;r.add(&owner,true);r.bind(&owner,0);
  auto s=make(&owner,0);r.frontend(&owner,0,17,23,s,2,NOT_CONSULTED,true,0);
  assert(covered(*r.find(&owner))==1);r.skip(&owner,0,false,1);r.skip(&owner,0,true,0);
  legacy_view v;v.valid=v.closed=v.T=v.M=v.runtime=true;v.slice=0;
  v.total.observed=v.phase[0].observed=2;v.total.absent=v.phase[0].absent=1;
  v.total.skips[0]=v.phase[0].skips[0]=1;v.total.raw[2]=v.phase[0].raw[2]=1;
  v.total.requester[0]=v.phase[0].requester[0]=1;
  FILE*f=std::tmpfile();assert(f);r.print(f,&owner,v,17);assert(!r.output_invalid&&r.output_bytes>0);
  r.output_bytes=OUTPUT_CAP-MARKER_RESERVE;r.invalid_marker(f,"r5_summary_ceiling");assert(r.output_invalid&&r.marker&&r.output_bytes<=OUTPUT_CAP);std::fclose(f);++checks;
 }
 {registry r;r.configured=r.enabled=true;std::array<int,64>owners{};
  for(unsigned i=0;i<64;++i){r.add(&owners[i],true);r.bind(&owners[i],i);auto s=make(&owners[i],i%16);s.slice=i;r.frontend(&owners[i],0,17,23,s,2,NOT_CONSULTED,true,0);}
  FILE*f=std::tmpfile();assert(f);
  for(unsigned i=0;i<64;++i){legacy_view v;v.valid=v.closed=v.T=v.M=v.runtime=true;v.slice=i;
   v.total.observed=v.phase[0].observed=1;v.total.raw[2]=v.phase[0].raw[2]=1;v.total.requester[0]=v.phase[0].requester[0]=1;r.print(f,&owners[i],v,17);}
  assert(!r.invalid&&!r.output_invalid);std::rewind(f);char line[32768];bool global=false;
  while(std::fgets(line,sizeof(line),f))if(std::strstr(line,"\"record\":\"GLOBAL\"")){assert(std::strstr(line,"\"slice\":64")&&std::strstr(line,"\"epoch_closed\":true")&&std::strstr(line,"\"closure_ok\":true"));global=true;}
  assert(global);std::fclose(f);++checks;
 }
 {std::unique_ptr<route_joint_table>j(new route_joint_table);
  for(unsigned n=0;n<=route_joint_table::MAX_ROWS;++n){unsigned x=n;unsigned early=x%6;x/=6;unsigned admit=x%2;x/=2;unsigned route=x%6;x/=6;unsigned raw=x%8;x/=8;unsigned r=x%17;x/=17;unsigned phase=x%18;x/=18;unsigned slice=x%64;
   auto s=make(&owner,r,raw);s.slice=slice;j->record(slice,phase,&owner,17,23,s,route,admit,early);
  }
  assert(j->rows==route_joint_table::MAX_ROWS&&j->invalid);++checks;
 }
 std::printf("TAGPRESSURE_METADATA_CONTROLS|SYNTHETIC_FIXTURE=1|checks=%u|sizeof_phase=%zu|sizeof_joint=%zu|PASS\n",checks,sizeof(phase_counts),sizeof(route_joint_table));
}
