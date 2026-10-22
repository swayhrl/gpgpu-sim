#ifndef EPL2_TAGPRESSURE_V1_H
#define EPL2_TAGPRESSURE_V1_H
// Passive target-set observer. No field supplies a modeled decision.
// All inputs below must be captured before preview/admission at the SAME
// pre-existing r4 observer probe. This reducer cannot query/mutate the model.
#include <array>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <stdexcept>
#include <utility>
namespace ep_l2_tagpressure {
constexpr unsigned RAW=8, W=17, MASK=32, PRED=8;
enum requester_kind { CERTIFIED_RO_CANDIDATE, NONRO_NATIVE_PATH, RUNTIME_OFF };
enum way_kind { RESERVED_WAY, INVALID_WAY, REPLACEABLE_CLEAN, REPLACEABLE_DIRTY };
// Original raw enum 0..WRITE_ALLOCATED maps identically; all other -> UNKNOWN.
// Wire schema supplies explicit raw labels; no integer enum inference by consumer.
enum reason_bit { HAS_ELIGIBLE=1, HAS_NOT_ATTACHED=2, HAS_REUSE=4,
                  HAS_NO_MISSING=8, HAS_NO_LIVE=16 };
struct way {way_kind kind=INVALID_WAY;bool live=false,attached=false,reuse=false;
            unsigned required=0,ready=0;};
struct snapshot {
 const void*cache_identity=nullptr;unsigned slice=~0U;
 uint64_t physical_cycle=~uint64_t(0),request_uid=~uint64_t(0);
 unsigned assoc=0,raw=RAW-1,raw_r4=RAW-1;
 unsigned requester=RUNTIME_OFF; // copy existing r4 runtime/candidate booleans
 bool ro_runtime=false,T=false; // per slice config; never changes a decision
 std::array<way,16> ways{};
};
inline bool context_matches(const snapshot&s,const void*cache,unsigned slice,
                            uint64_t cycle,uint64_t uid){
 return cache&&s.cache_identity==cache&&s.slice==slice&&
        s.physical_cycle==cycle&&s.request_uid==uid;
}
enum route { PROD_NOT_REACHED, NOT_CONSULTED, CONSULTED_MISS, CONSULTED_FAIL, EXISTING_HIT_RESERVED, ROUTE_OTHER };
struct phase_counts {
 // Keep PHASE counters only; SLICE/GLOBAL totals are exact reductions of these.
 uint64_t covered=0, skipped[4]={}, absent=0, ro_observed=0,ro_na=0;
 bool invalid=false;
 std::array<uint64_t,RAW*W> heads_raw_reserved{};
 std::array<uint64_t,RAW*W*W> heads_raw_reserved_eligible{};
 std::array<uint64_t,RAW*W*MASK> heads_raw_reserved_reasonmask{};
 std::array<uint64_t,RAW*W*PRED> ways_raw_reserved_predmask{};
 std::array<uint64_t,RAW*W*4> ways_raw_reserved_class{};
 std::array<uint64_t,RAW*W*W> heads_raw_reserved_invalid{};
 // Live predicate masks are (attached?1:0)|(reuse?2:0)|(missing?4:0).
 // They describe physical state even when T=false; operational eligibility
 // is separately T && structural mask==5. Runtime-off is NA, NOT no-live.
 static void checked(uint64_t&v,uint64_t n,bool&bad){
  if(n>std::numeric_limits<uint64_t>::max()-v){bad=true;return;}v+=n;
 }
 void add(uint64_t&v,uint64_t n=1){checked(v,n,invalid);}
 void observe(const snapshot&s,const void*cache,unsigned slice,uint64_t cycle,uint64_t uid){
  if(!context_matches(s,cache,slice,cycle,uid)||s.assoc!=16||s.raw>=RAW||s.raw!=s.raw_r4||s.requester>RUNTIME_OFF||(!s.ro_runtime)!=(s.requester==RUNTIME_OFF)){invalid=true;return;}
  unsigned r=0,ni=0,eligible=0,reasons=0;
  for(unsigned i=0;i<s.assoc;++i){
   const auto&w=s.ways[i]; if(unsigned(w.kind)>3){invalid=true;return;}
   r+=w.kind==RESERVED_WAY;ni+=w.kind==INVALID_WAY;
   if(!s.ro_runtime)continue;
   if(!w.live){reasons|=HAS_NO_LIVE;continue;}
   // Keep reasons independently, not priority masked: reuse+noMissing coexists.
   if(!w.attached)reasons|=HAS_NOT_ATTACHED;
   if(w.reuse)reasons|=HAS_REUSE;
   if(!(w.required&~w.ready))reasons|=HAS_NO_MISSING;
   if(w.attached&&!w.reuse&&(w.required&~w.ready)){
    reasons|=HAS_ELIGIBLE;++eligible;
   }
  }
  add(covered);const unsigned k=s.raw*W+r;
  add(heads_raw_reserved[k]);add(heads_raw_reserved_invalid[k*W+ni]);
  if(s.ro_runtime){add(ro_observed);add(heads_raw_reserved_eligible[k*W+eligible]);
   add(heads_raw_reserved_reasonmask[k*MASK+reasons]);
  }else add(ro_na);
  for(unsigned i=0;i<s.assoc;++i){const auto&w=s.ways[i];
   add(ways_raw_reserved_class[k*4+unsigned(w.kind)]);
   if(s.ro_runtime&&w.live){unsigned m=(w.attached?1:0)|(w.reuse?2:0)|((w.required&~w.ready)?4:0);
    add(ways_raw_reserved_predmask[k*PRED+m]);}
  }
 }
 bool basic_closure()const{
  uint64_t h=0,e=0,m=0,cl=0,iv=0,ro=0;bool bad=invalid;
  for(auto n:heads_raw_reserved)checked(h,n,bad);
  for(auto n:heads_raw_reserved_eligible)checked(e,n,bad);
  for(auto n:heads_raw_reserved_reasonmask)checked(m,n,bad);
  for(auto n:ways_raw_reserved_class)checked(cl,n,bad);
  for(auto n:heads_raw_reserved_invalid)checked(iv,n,bad);
  checked(ro,ro_observed,bad);checked(ro,ro_na,bad);
  if(covered>std::numeric_limits<uint64_t>::max()/16)bad=true;
  return !bad&&h==covered&&covered==ro&&e==ro_observed&&m==ro_observed&&cl==covered*16&&iv==covered;
 }
};
// One bounded, exact sparse joint shared by ALL64 slices/18 phases. This is
// observer metadata, not a second controller/authority/model port. No request log.
// Empty slot key0; nonempty stores packed key+1 and a checked uint64 head count.
// Key has slice6|phase5|raw3|reserved5|requester2|eligibility2|route3|admit1|early3.
// eligibility:0=NA(runtime off),1=no structural eligible,2=has eligible.
// Full tuple is preserved; no marginals are used to assign requester or gate.
struct route_joint_table {
 static std::array<unsigned,9> decode(uint64_t stored){
  uint64_t k=stored-1;std::array<unsigned,9>a{};
  a[8]=k&7;k>>=3;a[7]=k&1;k>>=1;a[6]=k&7;k>>=3;
  a[5]=k&3;k>>=2;a[4]=k&3;k>>=2;a[3]=k&31;k>>=5;
  a[2]=k&7;k>>=3;a[1]=k&31;k>>=5;a[0]=unsigned(k);return a;
 }
 static constexpr unsigned SLOTS=131072, MAX_ROWS=65536;
 struct slot {uint64_t key=0,count=0;};
 std::array<slot,SLOTS> slots{};unsigned rows=0;bool invalid=false;
 void record(unsigned slice,unsigned phase,const void*cache,uint64_t cycle,
             uint64_t uid,const snapshot&s,unsigned production_route,bool admitted,unsigned early){
  if(!context_matches(s,cache,slice,cycle,uid)||slice>=64||phase>=18||s.assoc!=16||s.raw>=RAW||s.raw!=s.raw_r4||
     s.requester>RUNTIME_OFF||(!s.ro_runtime)!=(s.requester==RUNTIME_OFF)||
     production_route>=6||early>=6){invalid=true;return;}
  unsigned r=0;bool eligible=false;
  for(unsigned i=0;i<16;++i){const auto&w=s.ways[i];
   if(unsigned(w.kind)>3){invalid=true;return;}
   r+=w.kind==RESERVED_WAY;
   eligible|=w.live&&w.attached&&!w.reuse&&(w.required&~w.ready);
  }
  unsigned e=!s.ro_runtime?0:eligible?2:1;
  uint64_t key=slice;
  key=(key<<5)|phase;key=(key<<3)|s.raw;key=(key<<5)|r;
  key=(key<<2)|s.requester;key=(key<<2)|e;
  key=(key<<3)|production_route;key=(key<<1)|unsigned(admitted);
  key=(key<<3)|early;++key;
  unsigned at=unsigned((key*11400714819323198485ull)>>(64-17));
  for(unsigned n=0;n<SLOTS;++n,at=(at+1)&(SLOTS-1)){
   auto&v=slots[at];
   if(v.key==key){phase_counts::checked(v.count,1,invalid);return;}
   if(!v.key){if(rows==MAX_ROWS){invalid=true;return;}
    v.key=key;v.count=1;++rows;return;}
  }
  invalid=true; // capacity/overflow is explicit invalid, never sampling/drop-zero
 }
};

constexpr unsigned SLICES=64, PHASES=18;
constexpr uint64_t OUTPUT_CAP=64ULL*1024*1024, MARKER_RESERVE=256;
static const char*const SCHEMA_SHA="cb973b3bc462ce33895a0f8cc31b428f6bf14b63f8497e8895eb314ea0894db7";
struct legacy_counts {
 uint64_t observed=0,skips[4]={},absent=0,raw[RAW]={},requester[3]={};
};
struct legacy_view {
 unsigned slice=~0U,kernel_count=0;std::array<uint64_t,16> kernel_uid{};
 bool closed=false,T=false,M=false,runtime=false,valid=false;
 legacy_counts total;std::array<legacy_counts,PHASES> phase{};
};
// Streaming two-pass writer. Never allocates a complete frame or writes before
// exact byte counting. Every fragment is bounded; write errors invalidate output.
struct writer {
 FILE*file=nullptr;uint64_t bytes=0;bool invalid=false;
 explicit writer(FILE*f=nullptr):file(f){}
 void emit(const char*fmt,...){
  char buf[2048];va_list ap;va_start(ap,fmt);
  int n=std::vsnprintf(buf,sizeof(buf),fmt,ap);va_end(ap);
  if(n<0||unsigned(n)>=sizeof(buf)){invalid=true;return;}
  phase_counts::checked(bytes,unsigned(n),invalid);
  if(file&&std::fwrite(buf,1,unsigned(n),file)!=unsigned(n))invalid=true;
 }
};
struct slice_state {
 const void*owner=nullptr;unsigned slice=~0U;uint64_t seq=0,last_cycle=0;
 std::array<std::unique_ptr<phase_counts>,PHASES> phases{};
 legacy_view last;
};
struct registry {
 bool configured=false,enabled=false,invalid=false,output_invalid=false,marker=false;
 uint64_t output_bytes=0;
 std::array<slice_state,SLICES> slices{};
 std::unique_ptr<route_joint_table> joint;
 slice_state*find(const void*owner){if(!enabled)return nullptr;for(auto&d:slices)if(d.owner==owner)return &d;return nullptr;}
 const slice_state*find(const void*owner)const{if(!enabled)return nullptr;for(auto&d:slices)if(d.owner==owner)return &d;return nullptr;}
 void add(const void*owner,bool old_enabled){
  if(!configured){const char*v=std::getenv("EPL2_TAGPRESSURE_DIAG");
   if(v&&std::strcmp(v,"0")&&std::strcmp(v,"1"))throw std::invalid_argument("EPL2_TAGPRESSURE_DIAG expects0or1");
   enabled=v&&!std::strcmp(v,"1");configured=true;
   if(enabled&&!old_enabled)throw std::invalid_argument("tagpressure requires EPL2_DETACH_DIAG=1");
  }
  if(!enabled)return;
  if(!joint)joint.reset(new route_joint_table);
  for(auto&d:slices)if(!d.owner){d=slice_state();d.owner=owner;return;}
  throw std::runtime_error("tagpressure64slice bound exceeded");
 }
 void erase(const void*owner){if(auto*d=find(owner)){*d=slice_state();}
  bool any=false;for(auto&d:slices)any|=bool(d.owner);
  if(!any)joint.reset();
 }
 void bind(const void*owner,unsigned slice){if(auto*d=find(owner)){
  if(slice>=SLICES||(d->slice!=~0U&&d->slice!=slice)){invalid=true;return;}
  for(auto&other:slices)if(other.owner&&other.owner!=owner&&other.slice==slice){invalid=true;return;}
  d->slice=slice;
 }}
 phase_counts*phase(slice_state&d,unsigned p){if(p>=PHASES){invalid=true;return nullptr;}
  if(!d.phases[p])d.phases[p].reset(new phase_counts);return d.phases[p].get();}
 void skip(const void*owner,unsigned p,bool absent,unsigned reason){if(auto*d=find(owner))if(auto*c=phase(*d,p)){
  if(absent)c->add(c->absent);else c->add(c->skipped[reason>=1&&reason<=3?reason-1:3]);
 }}
 void frontend(const void*owner,unsigned p,uint64_t cycle,uint64_t uid,
               const snapshot&s,unsigned raw,unsigned production_route,bool admit,unsigned early){
  auto*d=find(owner);if(!d)return;
  if(!context_matches(s,owner,d->slice,cycle,uid)||s.raw!=raw){invalid=true;return;}
  if(auto*c=phase(*d,p)){c->observe(s,owner,d->slice,cycle,uid);
   joint->record(d->slice,p,owner,cycle,uid,s,production_route,admit,early);
  }
 }
 static void reduce(phase_counts&dst,const phase_counts&src){
  dst.invalid|=src.invalid;
  dst.add(dst.covered,src.covered);dst.add(dst.absent,src.absent);
  dst.add(dst.ro_observed,src.ro_observed);dst.add(dst.ro_na,src.ro_na);
  for(unsigned i=0;i<4;++i)dst.add(dst.skipped[i],src.skipped[i]);
#define EPL2_TP_REDUCE(name) for(unsigned i=0;i<dst.name.size();++i)dst.add(dst.name[i],src.name[i]);
  EPL2_TP_REDUCE(heads_raw_reserved)
  EPL2_TP_REDUCE(heads_raw_reserved_eligible)
  EPL2_TP_REDUCE(heads_raw_reserved_reasonmask)
  EPL2_TP_REDUCE(ways_raw_reserved_predmask)
  EPL2_TP_REDUCE(ways_raw_reserved_class)
  EPL2_TP_REDUCE(heads_raw_reserved_invalid)
#undef EPL2_TP_REDUCE
 }
 static bool old_matches(const phase_counts&c,const legacy_counts&old){
  bool bad=false;uint64_t observed=c.covered;
  for(unsigned i=0;i<4;++i){phase_counts::checked(observed,c.skipped[i],bad);if(c.skipped[i]!=old.skips[i])return false;}
  if(bad||observed!=old.observed||c.absent!=old.absent)return false;
  for(unsigned raw=0;raw<RAW;++raw){uint64_t n=0;for(unsigned r=0;r<W;++r)phase_counts::checked(n,c.heads_raw_reserved[raw*W+r],bad);if(n!=old.raw[raw])return false;}
  return !bad;
 }
 bool requester_matches(const phase_counts&c,const legacy_counts&old,unsigned slice,int phase)const{
  if(!joint)return false;uint64_t requester[3]={},sum=0;bool bad=joint->invalid;
  for(const auto&slot:joint->slots)if(slot.key){const auto a=route_joint_table::decode(slot.key);
   if(a[0]!=slice||(phase>=0&&a[1]!=unsigned(phase)))continue;
   if(a[4]>=3){bad=true;continue;}
   phase_counts::checked(requester[a[4]],slot.count,bad);phase_counts::checked(sum,slot.count,bad);
  }
  for(unsigned i=0;i<3;++i)if(requester[i]!=old.requester[i])return false;
  return !bad&&sum==c.covered;
 }
 static void histogram(writer&w,const char*name,const uint64_t*values,unsigned second,unsigned third=0){
  w.emit("\"%s\":[",name);bool first=true;
  const unsigned total=RAW*second*(third?third:1);
  for(unsigned i=0;i<total;++i)if(values[i]){
   unsigned a=third?i/(second*third):i/second,b=third?(i/third)%second:i%second;
   w.emit("%s[%u,%u",first?"":",",a,b);if(third)w.emit(",%u",i%third);
   w.emit(",%llu]",(unsigned long long)values[i]);first=false;
  }w.emit("]");
 }
 bool valid_counts(const phase_counts&c)const{
  return c.basic_closure()&&!invalid&&joint&&!joint->invalid;
 }
 void serialize(writer&w,const char*record,unsigned slice,int p,const phase_counts&c,
                const legacy_view&v,uint64_t cycle,uint64_t seq,bool closure,bool closed){
  const char*scope=p<0?(!std::strcmp(record,"GLOBAL")?"GLOBAL_CUMULATIVE":"SLICE_CUMULATIVE"):p==0?"UNASSIGNED":p==1?"OVERLAP_UNKNOWN":"SINGLE_ACTIVE_INTERVAL";
  w.emit("EPL2TAGPRESSUREV1|{\"record\":\"%s\",\"schema_id\":\"EPL2_TAGPRESSURE_V1_R5\",\"schema_sha256\":\"%s\",\"source_base_core\":\"9243f769620653f2a9d4df5599e9581e9dae6067\",\"scope\":\"FRONTEND_HEAD_CYCLE_SAME_TARGET_SET\",\"slice\":%u,\"cycle\":%llu,\"snapshot_seq\":%llu,\"phase\":%d,\"kernel_scope\":\"%s\",\"kernel_uid\":",record,SCHEMA_SHA,slice,(unsigned long long)cycle,(unsigned long long)seq,p,scope);
  if(p>=2&&unsigned(p-2)<v.kernel_count)w.emit("%llu",(unsigned long long)v.kernel_uid[p-2]);else w.emit("null");
  w.emit(",\"kernel_catalog\":[");for(unsigned i=0;i<v.kernel_count;++i)w.emit("%s%llu",i?",":"",(unsigned long long)v.kernel_uid[i]);
  w.emit("],\"assoc\":16,\"T\":%s,\"M\":%s,\"ro_runtime\":%s,\"epoch_closed\":%s,\"overflow_invalid\":%s,\"output_invalid\":%s,\"closure_ok\":%s,\"counters\":{",v.T?"true":"false",v.M?"true":"false",v.runtime?"true":"false",closed?"true":"false",(c.invalid||invalid||(joint&&joint->invalid))?"true":"false",output_invalid?"true":"false",closure?"true":"false");
  bool bad=false;uint64_t skipped=0;for(auto n:c.skipped)phase_counts::checked(skipped,n,bad);uint64_t observed=c.covered;phase_counts::checked(observed,skipped,bad);if(bad)w.invalid=true;
  w.emit("\"observed_heads\":%llu,\"covered\":%llu,\"probe_skipped\":%llu,\"skip_tvd\":%llu,\"skip_ideal\":%llu,\"skip_disabled\":%llu,\"skip_other\":%llu,\"head_absent_cycles\":%llu,\"ro_observed\":%llu,\"ro_na\":%llu},\"histograms\":{",(unsigned long long)observed,(unsigned long long)c.covered,(unsigned long long)skipped,(unsigned long long)c.skipped[0],(unsigned long long)c.skipped[1],(unsigned long long)c.skipped[2],(unsigned long long)c.skipped[3],(unsigned long long)c.absent,(unsigned long long)c.ro_observed,(unsigned long long)c.ro_na);
  histogram(w,"heads_raw_reserved",c.heads_raw_reserved.data(),W);w.emit(",");
  histogram(w,"heads_raw_reserved_eligible",c.heads_raw_reserved_eligible.data(),W,W);w.emit(",");
  histogram(w,"heads_raw_reserved_reasonmask",c.heads_raw_reserved_reasonmask.data(),W,MASK);w.emit(",");
  histogram(w,"ways_raw_reserved_predmask",c.ways_raw_reserved_predmask.data(),W,PRED);w.emit(",");
  histogram(w,"ways_raw_reserved_class",c.ways_raw_reserved_class.data(),W,4);w.emit(",");
  histogram(w,"heads_raw_reserved_invalid",c.heads_raw_reserved_invalid.data(),W,W);
  w.emit("},\"route_joint\":[");bool first=true;
  if(joint)for(const auto&slot:joint->slots)if(slot.key){auto a=route_joint_table::decode(slot.key);
   const bool global=!std::strcmp(record,"GLOBAL");
   if((!global&&a[0]!=slice)||(p>=0&&a[1]!=unsigned(p)))continue;
   w.emit("%s[%u,%u,%u,%u,%u,%u,%u,%u,%u,%llu]",first?"":",",a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],(unsigned long long)slot.count);first=false;
  }w.emit("]}\n");
 }
 void invalid_marker(FILE*f,const char*reason){
  output_invalid=true;if(marker)return;marker=true;
  writer count;count.emit("EPL2TAGPRESSUREV1|{\"record\":\"OUTPUT_INVALID\",\"schema_id\":\"EPL2_TAGPRESSURE_V1_R5\",\"schema_sha256\":\"%s\",\"output_invalid\":true,\"reason\":\"%s\"}\n",SCHEMA_SHA,reason);
  if(count.invalid||count.bytes>MARKER_RESERVE||output_bytes>OUTPUT_CAP-count.bytes)return;
  writer out(f);out.emit("EPL2TAGPRESSUREV1|{\"record\":\"OUTPUT_INVALID\",\"schema_id\":\"EPL2_TAGPRESSURE_V1_R5\",\"schema_sha256\":\"%s\",\"output_invalid\":true,\"reason\":\"%s\"}\n",SCHEMA_SHA,reason);
  if(!out.invalid)output_bytes+=out.bytes;
 }
 static bool budget_allows(uint64_t written,uint64_t next){return written<=OUTPUT_CAP-MARKER_RESERVE&&next<=OUTPUT_CAP-MARKER_RESERVE-written;}
 void frame(FILE*f,const char*record,unsigned slice,int p,const phase_counts&c,const legacy_view&v,uint64_t cycle,uint64_t seq,bool closure,bool closed){
  if(output_invalid)return;
  writer size;serialize(size,record,slice,p,c,v,cycle,seq,closure,closed);
  if(size.invalid||!budget_allows(output_bytes,size.bytes)){invalid_marker(f,"r5_summary_ceiling");return;}
  writer out(f);serialize(out,record,slice,p,c,v,cycle,seq,closure,closed);
  if(out.invalid||out.bytes!=size.bytes){invalid_marker(f,"r5_output_write");return;}
  output_bytes+=out.bytes;
 }
 void print(FILE*f,const void*owner,const legacy_view&v,uint64_t cycle){
  auto*d=find(owner);if(!d)return;
  if(v.kernel_count>16||v.slice>=SLICES||d->slice!=v.slice){invalid=true;invalid_marker(f,"r5_scope_mismatch");return;}
  d->last=v;d->last_cycle=cycle;phase_counts cumulative;
  for(unsigned p=0;p<PHASES;++p)if(d->phases[p])reduce(cumulative,*d->phases[p]);
  bool ok=valid_counts(cumulative)&&old_matches(cumulative,v.total)&&requester_matches(cumulative,v.total,d->slice,-1);
  if(d->seq==~uint64_t(0)){invalid=true;ok=false;}else ++d->seq;
  frame(f,"SLICE",d->slice,-1,cumulative,v,cycle,d->seq,ok,v.closed);
  for(unsigned p=0;p<v.kernel_count+2;++p){phase_counts empty;const auto&c=d->phases[p]?*d->phases[p]:empty;
   frame(f,"PHASE",d->slice,p,c,v,cycle,d->seq,valid_counts(c)&&old_matches(c,v.phase[p])&&requester_matches(c,v.phase[p],d->slice,p),v.closed);
  }
  unsigned max=0,n=0;for(const auto&s:slices)if(s.owner){++n;if(s.slice!=~0U)max=std::max(max,s.slice);}
  if(d->slice!=max)return;
  phase_counts global;bool closed=true,good=true;
  for(const auto&s:slices)if(s.owner){closed&=s.last.valid&&s.last.closed;
   good&=s.seq==d->seq&&s.last_cycle==cycle&&s.slice!=~0U&&s.last.valid&&s.last.T==v.T&&s.last.M==v.M&&s.last.runtime==v.runtime&&s.last.kernel_count==v.kernel_count&&s.last.kernel_uid==v.kernel_uid;
   for(const auto&c:s.phases)if(c)reduce(global,*c);
  }
  frame(f,"GLOBAL",n,-1,global,v,cycle,d->seq,good&&valid_counts(global),closed);
 }
};
extern registry state;
static_assert(sizeof(phase_counts)*SLICES*PHASES+sizeof(route_joint_table)+sizeof(registry)+3*sizeof(phase_counts)+65536<=128ULL*1024*1024,"passive observer exceeds128MiB design bound");
}
#endif
