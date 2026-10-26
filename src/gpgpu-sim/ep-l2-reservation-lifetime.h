#ifndef EP_L2_RESERVATION_LIFETIME_H
#define EP_L2_RESERVATION_LIFETIME_H
// Host-only passive observer. Never supplies a cache decision or model resource.
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <limits>
#include <string>
namespace ep_l2_life {
constexpr unsigned SETS=64, WAYS=16, SECTORS=4, PAYLOADS=1168, BINS=33;
constexpr uint64_t WINDOW=8192, STATE_LIMIT=16ULL<<20, OUTPUT_LIMIT=16ULL<<20;
enum counter {
 ENTER, EXIT, ALLOC, REENTER, SECTOR_ADD, MUTATE, TICK_PEAK, EVENT_PEAK,
 NEW_UNIQUE, NEW_HEAD, RAW_FAIL_HEAD, NEW_COMMIT, MERGE, REQUESTER,
 INC_END, UNIQUE_SECTOR, SECTOR_TRANSITION, CLOSED_NEW, CLOSED_RE, CLOSED_LEFT,
 RIGHT, LEFT, SUM_NEW, SUM_RE, SUM_LEFT, SUM_RIGHT, MAX_NEW, MAX_RE,
 MAX_LEFT, RO_ADMIT, RO_RETIRE, RO_TIME, RO_LOWER_TIME, RO_RESPONSE_ONLY_TIME,
 RO_WAITER_ADDS, RO_MAX_WAITERS, RO_REQUIRED_ADD, RO_ISSUED_ADD, RO_READY_ADD,
 NEW_PENDING_SECTORS_SUM, NEW_CAN_DETACH_SUM, NEW_AT_2, NEW_AT_3, NEW_AT_4,
 NEW_AT_8, NEW_AT_12, NEW_AT_15, NEW_AT_16, NEW_WITH_PENDING, NEW_WITH_ELIGIBLE, WAIT_COUNT, WAIT_SUM, WAIT_MAX, PROD_TAGFAIL, RO_INITIAL_READY, NC
};
enum stage {
 L1_ACCESS, L1_MISS, L1_MERGE, L1_MISSQ_BLOCK, L1_SEND,
 ICNT_INJECT_ACCEPT, ICNT_INJECT_DENY, L2_INGRESS,
 FRONTEND_HEAD, FRONTEND_DENY, FRONTEND_COMMIT, TVD_EARLY,
 LOWER_QUEUE_BLOCK, L2_MISSQ_BLOCK, DRAM_ACCEPT, DRAM_RETURN,
 RO_READY_EVENT, RESPONSE_ACCEPT, DRAM_SERVICE, NSTAGE
};
enum reason { GENERIC, ALLOCATE, SECTOR_ALLOCATE, FILL, RESET, INVALIDATE,
 FLUSH, DETACH, TVD_BIND, STATUS_WRITE };
struct way_state {
 uint64_t address=0, generation=0, begin=0;
 uint32_t packets=0, merges=0, enters=0, exits=0;
 uint8_t reserved=0, valid=0, sectors_seen=0, kind=0;
 bool incarnation=false, carry=false, timed_incarnation=false;
 uint8_t request_mask=0;
};
struct set_state {
 std::array<uint64_t,17> area{};
 std::array<std::array<uint64_t,BINS>,2> hist{};
 std::array<uint64_t,NC> c{};
 std::array<uint64_t,17> new_head_reserved{},new_unique_reserved{};
 std::array<uint64_t,16> incarnation_stats{};
 uint64_t last=0;
 unsigned occupancy=0, pending_sectors=0, eligible=0;
};
struct window_set {
 std::array<uint64_t,17> area{};
 // enter,exit,allocation,sectoradd,newunique,newhead,rawfail,merge,ROadmit,ROready,reenter,new_with_pending,new_with_eligible,wait_cycles,wait_completions
 std::array<uint64_t,15> c{};
 uint64_t last=0;
};
struct ro_state {
 uint64_t generation=0, birth=0, last=0;
 uint16_t set=0, tag=0, max_waiters=0, waiter_count=0;
 uint8_t required=0, issued=0, ready=0;
 bool live=false, attached=false, eligible=false;
};
struct sample {
 uint64_t tick=0, gpu=0, ordinal=0, generation=0, address=0;
 unsigned index=0, oldmask=0, newmask=0, operation=0;
};
class observer;
inline std::array<observer*,64> registry{};
inline uint64_t allocated_bytes=0, output_bytes=0;
inline bool output_overflow=false, output_error=false;
inline bool enabled() {
 static const bool on=[](){const char *s=std::getenv("EPL2_TAG_LIFETIME");
   return s && std::strcmp(s,"1")==0;}();
 return on;
}
inline unsigned bits(unsigned x) {return unsigned(__builtin_popcount(x));}
// Bin0=zero; bin1=1; bin2=2..3; ... bin32=>=2^31. Max stays exact64bit.
inline unsigned logbin(uint64_t x) {
 if(!x)return 0;
 unsigned b=1;while(x>1 && b<BINS-1){x>>=1;++b;}return b;
}
inline bool emit(FILE *f,const char *fmt,...) {
 char buf[6144];va_list ap;va_start(ap,fmt);int n=vsnprintf(buf,sizeof(buf),fmt,ap);va_end(ap);
 if(n<0 || unsigned(n)>=sizeof(buf) || output_bytes>OUTPUT_LIMIT-8192 || uint64_t(unsigned(n))>OUTPUT_LIMIT-8192-output_bytes) {
   output_overflow=true;return false;
 }
 if(output_overflow || output_error)return false;
 size_t written=fwrite(buf,1,unsigned(n),f);output_bytes+=written;
 if(written!=unsigned(n) || ferror(f)){output_error=true;return false;}return true;
}
template<size_t N> inline std::string array_text(const std::array<uint64_t,N>& a) {
 std::string out;out.reserve(N*12);
 for(size_t i=0;i<N;++i){if(i)out+=',';out+=std::to_string(a[i]);}return out;
}
class observer {
 public:
 unsigned slice;
 uint64_t tick=0, gpu=0, begin_gpu=0, end_gpu=0, ticks=0, ordinal=0, invalid=0;
 uint64_t window_begin=0, window_gpu_begin=0, last_head_uid=~uint64_t(0), last_new_uid=~uint64_t(0);
 uint64_t new_first_tick=0;
 bool active=true, closed=false, printed=false;
 unsigned why=GENERIC;
 std::array<way_state,SETS*WAYS> way{};
 std::array<set_state,SETS> set{};
 std::array<window_set,SETS> window{};
 std::array<ro_state,PAYLOADS> ro{};
 std::array<uint64_t,NSTAGE> stage_total{}, stage_window{};
 std::array<sample,8> examples{};
 unsigned examples_count=0;
 uint64_t ro_right_count=0, ro_right_time=0, mask_events=0, observed_state_bytes=0;
 explicit observer(unsigned s):slice(s) {}
 void add(uint64_t &x,uint64_t n=1) {
   if(UINT64_MAX-x<n){x=UINT64_MAX;invalid|=1;}else x+=n;
 }
 void accumulate(unsigned s,uint64_t at) {
   if(s>=SETS){invalid|=2;return;}
   auto &v=set[s];auto &w=window[s];
   if(at<v.last || at<w.last || v.occupancy>WAYS){invalid|=4;return;}
   add(v.area[v.occupancy],at-v.last);v.last=at;
   add(w.area[v.occupancy],at-w.last);w.last=at;
 }
 void seed(unsigned index,uint64_t address,unsigned reserved,unsigned valid) {
   if(index>=way.size() || reserved>15 || valid>15 || (reserved&~valid)){invalid|=8;return;}
   auto &v=way[index];v.address=address;v.valid=valid;v.reserved=reserved;
   v.incarnation=bool(valid);v.generation=valid?1:0;v.sectors_seen=reserved;
   if(reserved){v.carry=true;v.kind=2;v.begin=0;auto &s=set[index/WAYS];
     ++s.occupancy;s.pending_sectors+=bits(reserved);add(s.c[LEFT]);
     s.c[EVENT_PEAK]=s.occupancy;}
 }
 void incarnation_close(unsigned index,bool right=false) {
   auto &v=way[index];auto &a=set[index/WAYS].incarnation_stats;
   unsigned b=right?8:0;add(a[b]);if(!v.timed_incarnation)add(a[b+1]);
   add(a[b+2],v.packets);a[b+3]=std::max<uint64_t>(a[b+3],v.packets);
   add(a[b+4],v.merges);a[b+5]=std::max<uint64_t>(a[b+5],v.merges);
   add(a[b+6],bits(v.request_mask));a[b+7]=std::max<uint64_t>(a[b+7],bits(v.request_mask));
   if(!right){add(set[index/WAYS].c[INC_END]);v.incarnation=false;}
 }
 void touch(unsigned index,unsigned sector_mask,bool merged=false) {
   if(!active || closed || index>=way.size() || sector_mask>15)return;
   auto &v=way[index];
   if(v.packets==UINT32_MAX || (merged&&v.merges==UINT32_MAX))invalid|=1048576;
   else {++v.packets;if(merged)++v.merges;}
   v.request_mask|=sector_mask;requester(index/WAYS,merged);
 }
 void episode_close(unsigned index) {
   auto &v=way[index];auto &s=set[index/WAYS];
   if(v.exits==UINT32_MAX)invalid|=262144;else ++v.exits;
   if(tick<v.begin){invalid|=16;return;}uint64_t dt=tick-v.begin;
   unsigned n=v.kind==0?CLOSED_NEW:v.kind==1?CLOSED_RE:CLOSED_LEFT;
   unsigned sum=v.kind==0?SUM_NEW:v.kind==1?SUM_RE:SUM_LEFT;
   unsigned mx=v.kind==0?MAX_NEW:v.kind==1?MAX_RE:MAX_LEFT;
   add(s.c[n]);add(s.c[sum],dt);s.c[mx]=std::max(s.c[mx],dt);
   if(v.kind<2)add(s.hist[v.kind][logbin(dt)]);
   add(s.c[EXIT]);add(window[index/WAYS].c[1]);
   if(!s.occupancy){invalid|=32;}else --s.occupancy;
 }
 void change(unsigned index,uint64_t address,unsigned reserved,unsigned valid,
             unsigned operation,bool new_incarnation=false) {
   if(!active || closed)return;
   if(index>=way.size() || reserved>15 || valid>15 || (reserved&~valid)){invalid|=64;return;}
   auto &v=way[index];unsigned si=index/WAYS;auto &s=set[si];auto &w=window[si];
   accumulate(si,tick);add(ordinal);add(s.c[MUTATE]);
   unsigned oldmask=v.reserved;const unsigned previous_pending=oldmask;
   if(new_incarnation){
     if(oldmask)episode_close(index);
     if(v.incarnation)incarnation_close(index);
     if(v.generation==UINT64_MAX)invalid|=128;else ++v.generation;
     v.address=address;v.incarnation=true;v.reserved=0;v.valid=0;
     v.sectors_seen=0;v.packets=0;v.merges=0;v.request_mask=0;v.timed_incarnation=true;oldmask=0;
     add(s.c[ALLOC]);add(w.c[2]);
   } else if(v.incarnation && v.address!=address)invalid|=256;
   unsigned newly_reserved=reserved&~oldmask;
   add(s.c[SECTOR_TRANSITION],bits(newly_reserved));
   add(s.c[UNIQUE_SECTOR],bits(newly_reserved&~v.sectors_seen));
   v.sectors_seen|=newly_reserved;
   if(!oldmask && reserved) {
     ++s.occupancy;v.begin=tick;v.kind=new_incarnation?0:1;
     if(v.enters==UINT32_MAX)invalid|=262144;else ++v.enters;
     add(s.c[ENTER]);add(w.c[0]);
     if(!new_incarnation){add(s.c[REENTER]);add(w.c[10]);}
   } else if(oldmask && !reserved)episode_close(index);
   if(oldmask && newly_reserved){add(s.c[SECTOR_ADD],bits(newly_reserved));add(w.c[3],bits(newly_reserved));}
   if(s.pending_sectors<bits(previous_pending))invalid|=512;
   else s.pending_sectors-=bits(previous_pending);
   s.pending_sectors+=bits(reserved);
   v.reserved=reserved;v.valid=valid;v.address=address;
   if(!valid && v.incarnation)incarnation_close(index);
   s.c[EVENT_PEAK]=std::max<uint64_t>(s.c[EVENT_PEAK],s.occupancy);
   unsigned cause=why==GENERIC?operation:why;
   if(examples_count<examples.size() &&
      (s.occupancy>=4 || (!reserved && oldmask && v.begin==tick) || cause==DETACH)) {
     examples[examples_count++]={tick,gpu,ordinal,v.generation,address,index,oldmask,reserved,cause};
   }
 }
 void close_ro_phase(ro_state &v) {
   if(tick<v.last){invalid|=1024;return;}
   uint64_t dt=tick-v.last;auto &s=set[v.set];
   if(v.issued&~v.ready)add(s.c[RO_LOWER_TIME],dt);
   if(v.live && !(v.required&~v.ready))add(s.c[RO_RESPONSE_ONLY_TIME],dt);
   v.last=tick;
 }
 // Called after actual RO mutations; flags describe the existing can_detach predicate.
 void ro_update(unsigned slot,uint64_t generation,unsigned si,unsigned index,
                unsigned required,unsigned issued,unsigned ready,unsigned waiters,
                bool attached,bool eligible,bool retiring=false) {
   if(!active || closed)return;
   if(slot>=ro.size() || si>=SETS || index>=way.size() ||
      (required|issued|ready)>15 || waiters>512){invalid|=2048;return;}
   auto &v=ro[slot];auto &s=set[si];
   if(v.live && (v.generation!=generation || v.set!=si)){invalid|=4096;return;}
   if(!v.live){
     if(retiring){invalid|=8192;return;}
     v=ro_state();v.live=true;v.birth=v.last=tick;v.generation=generation;v.set=si;v.tag=index;
     v.ready=ready;add(s.c[RO_INITIAL_READY],bits(ready));
     add(s.c[RO_ADMIT]);add(window[si].c[8]);
   }else close_ro_phase(v);
   if(v.eligible){if(!set[v.set].eligible)invalid|=16384;else --set[v.set].eligible;}
   add(s.c[RO_REQUIRED_ADD],bits(required&~v.required));
   add(s.c[RO_ISSUED_ADD],bits(issued&~v.issued));
   add(s.c[RO_READY_ADD],bits(ready&~v.ready));
   if((ready&~v.ready)){add(window[si].c[9],bits(ready&~v.ready));record_stage(RO_READY_EVENT,bits(ready&~v.ready));}
   if(waiters>v.waiter_count)add(s.c[RO_WAITER_ADDS],waiters-v.waiter_count);
   s.c[RO_MAX_WAITERS]=std::max<uint64_t>(s.c[RO_MAX_WAITERS],waiters);
   v.required=required;v.issued=issued;v.ready=ready;v.waiter_count=waiters;
   v.attached=attached;v.eligible=eligible&&!retiring;
   if(v.eligible)++s.eligible;
   if(retiring){add(s.c[RO_RETIRE]);add(s.c[RO_TIME],tick-v.birth);v.live=false;}
 }
 void merge_marker(unsigned index) {
   if(!active || closed || index>=way.size())return;
   auto &v=way[index];if(v.merges==UINT32_MAX)invalid|=1048576;else ++v.merges;
   add(set[index/WAYS].c[MERGE]);add(window[index/WAYS].c[7]);
 }
 void requester(unsigned si,bool merge) {
   if(!active || closed || si>=SETS)return;
   add(set[si].c[REQUESTER]);
   if(merge){add(set[si].c[MERGE]);add(window[si].c[7]);}
 }
 void frontend(unsigned si,uint64_t uid,bool new_tag,bool raw_fail,bool admitted) {
   if(!active || closed || si>=SETS)return;
   record_stage(FRONTEND_HEAD);
   if(last_head_uid!=uid)last_head_uid=uid; // FIFO head, never a global UID set.
   if(!admitted)record_stage(FRONTEND_DENY);
   if(!new_tag)return;
   auto &s=set[si];auto &w=window[si];
   add(s.c[NEW_HEAD]);add(w.c[5]);
   add(s.new_head_reserved[s.occupancy]);
   if(raw_fail){add(s.c[RAW_FAIL_HEAD]);add(w.c[6]);}
   if(last_new_uid==uid)return;
   // Main integration must prove FIFO single-head ordering before using this dedup.
   last_new_uid=uid;new_first_tick=tick;add(s.c[NEW_UNIQUE]);add(w.c[4]);
   add(s.new_unique_reserved[s.occupancy]);
   add(s.c[NEW_PENDING_SECTORS_SUM],s.pending_sectors);
   add(s.c[NEW_CAN_DETACH_SUM],s.eligible);
   if(s.pending_sectors){add(s.c[NEW_WITH_PENDING]);add(w.c[11]);}
   if(s.eligible){add(s.c[NEW_WITH_ELIGIBLE]);add(w.c[12]);}
   const unsigned thresholds[]={2,3,4,8,12,15,16};
   for(unsigned k=0;k<7;++k)if(s.occupancy==thresholds[k])add(s.c[NEW_AT_2+k]);
 }
 void record_stage(unsigned which,uint64_t n=1) {
   if(!active || closed || which>=NSTAGE)return;
   add(stage_total[which],n);add(stage_window[which],n);
 }
 void commit_new(unsigned si,uint64_t uid) {
   if(!active||closed||si>=SETS)return;
   add(set[si].c[NEW_COMMIT]);
   if(last_new_uid==uid && tick>=new_first_tick){
     auto dt=tick-new_first_tick;add(set[si].c[WAIT_COUNT]);add(set[si].c[WAIT_SUM],dt);
     set[si].c[WAIT_MAX]=std::max(set[si].c[WAIT_MAX],dt);
     add(window[si].c[13],dt);add(window[si].c[14]);
   }
 }
 void write_window(FILE *f,uint64_t end) {
   if(end<=window_begin)return;
   for(unsigned i=0;i<SETS;++i){
     accumulate(i,end);auto &w=window[i];
     emit(f,"EPL2LIFEV1|WINDOW_SET|slice=%u|set=%u|begin=%llu|end=%llu|area=%s|c=%s|invalid=%llu\n",
       slice,i,(unsigned long long)window_begin,(unsigned long long)end,
       array_text(w.area).c_str(),array_text(w.c).c_str(),(unsigned long long)invalid);
     w=window_set();w.last=end;
   }
   emit(f,"EPL2LIFEV1|WINDOW_STAGE|slice=%u|begin=%llu|end=%llu|begin_gpu=%llu|end_gpu=%llu|c=%s|invalid=%llu\n",
     slice,(unsigned long long)window_begin,(unsigned long long)end,
     (unsigned long long)window_gpu_begin,(unsigned long long)gpu,array_text(stage_window).c_str(),(unsigned long long)invalid);
   stage_window.fill(0);window_begin=end;window_gpu_begin=gpu;
 }
 void tick_begin(uint64_t physical_gpu,FILE *f=stdout) {
   if(!active || closed)return;
   if(ticks && physical_gpu<gpu){invalid|=32768;return;}
   tick=ticks;gpu=physical_gpu;if(!ticks){begin_gpu=gpu;window_gpu_begin=gpu;}
   if(tick && tick%WINDOW==0)write_window(f,tick);
 }
 void tick_end() {
   if(!active || closed)return;
   for(unsigned s=0;s<SETS;++s)set[s].c[TICK_PEAK]=std::max<uint64_t>(set[s].c[TICK_PEAK],set[s].occupancy);
   add(ticks);
 }
 void finish(uint64_t physical_gpu,FILE *f=stdout) {
   if(closed)return;
   tick=ticks;gpu=end_gpu=physical_gpu;write_window(f,tick);
   for(unsigned i=0;i<SETS;++i){
     accumulate(i,tick);auto &s=set[i];
     for(unsigned j=0;j<WAYS;++j){auto &v=way[i*WAYS+j];if(uint64_t(v.carry)+v.enters-v.exits!=unsigned(bool(v.reserved)))invalid|=524288;if(v.reserved){add(s.c[RIGHT]);add(s.c[SUM_RIGHT],tick-v.begin);}}
     uint64_t weighted=0;
     for(unsigned k=0;k<=WAYS;++k){
       if(k && s.area[k]>UINT64_MAX/k){invalid|=65536;break;}
       add(weighted,s.area[k]*k);
     }
     uint64_t durations=0;add(durations,s.c[SUM_NEW]);add(durations,s.c[SUM_RE]);add(durations,s.c[SUM_LEFT]);add(durations,s.c[SUM_RIGHT]);
     if(weighted!=durations || s.c[LEFT]+s.c[ENTER]-s.c[EXIT]!=s.occupancy)invalid|=131072;
   }
   for(unsigned i=0;i<way.size();++i)if(way[i].incarnation)incarnation_close(i,true);
   for(auto &v:ro)if(v.live){close_ro_phase(v);add(ro_right_count);add(ro_right_time,tick-v.birth);}
   closed=true;
 }
 void print(FILE *f=stdout) {
   if(printed)return;printed=true;
   emit(f,"EPL2LIFEV1|META|slice=%u|sets=64|ways=16|sectors=4|window=8192|unit=L2_tick|gpu_unit=core_cycle|begin=0|end=%llu|begin_gpu=%llu|end_gpu=%llu|closed=%u|state_bytes=%llu|bins=33|bin32=ge2pow31|scope=timed_L2_only|invalid=%llu\n",
     slice,(unsigned long long)ticks,(unsigned long long)begin_gpu,(unsigned long long)end_gpu,unsigned(closed),
     (unsigned long long)observed_state_bytes,(unsigned long long)invalid);
   for(unsigned i=0;i<SETS;++i){const auto &s=set[i];
     emit(f,"EPL2LIFEV1|SET|slice=%u|set=%u|area=%s|c=%s|new_hist=%s|re_hist=%s|pending=%u|eligible=%u|invalid=%llu\n",
       slice,i,array_text(s.area).c_str(),array_text(s.c).c_str(),
       array_text(s.hist[0]).c_str(),array_text(s.hist[1]).c_str(),s.pending_sectors,s.eligible,(unsigned long long)invalid);
   }
   for(unsigned i=0;i<SETS;++i) {
     const auto &s=set[i];
     emit(f,"EPL2LIFEV1|INCARNATION|slice=%u|set=%u|stats=%s|head_reserved=%s|unique_reserved=%s|invalid=%llu\n",
       slice,i,array_text(s.incarnation_stats).c_str(),array_text(s.new_head_reserved).c_str(),
       array_text(s.new_unique_reserved).c_str(),(unsigned long long)invalid);
   }
   emit(f,"EPL2LIFEV1|STAGE|slice=%u|c=%s|ro_right_count=%llu|ro_right_time=%llu|invalid=%llu\n",
     slice,array_text(stage_total).c_str(),(unsigned long long)ro_right_count,
     (unsigned long long)ro_right_time,(unsigned long long)invalid);
   for(unsigned i=0;i<examples_count;++i){const auto &x=examples[i];
     emit(f,"EPL2LIFEV1|CASE|slice=%u|tick=%llu|gpu=%llu|ordinal=%llu|index=%u|generation=%llu|address=%llu|old=%u|new=%u|reason=%u\n",
       slice,(unsigned long long)x.tick,(unsigned long long)x.gpu,(unsigned long long)x.ordinal,x.index,
       (unsigned long long)x.generation,(unsigned long long)x.address,x.oldmask,x.newmask,x.operation);}
   // Keep a separate small terminal allowance even if ordinary records exhausted the cap.
   char end[256];int n=snprintf(end,sizeof(end),"EPL2LIFEV1|END|slice=%u|closed=%u|invalid=%llu|output_overflow=%u|output_error=%u|global_output_bytes=%llu\n",
     slice,unsigned(closed),(unsigned long long)invalid,unsigned(output_overflow),unsigned(output_error),(unsigned long long)output_bytes);
   if(n>0 && output_bytes<=OUTPUT_LIMIT && uint64_t(unsigned(n))<=OUTPUT_LIMIT-output_bytes){size_t written=fwrite(end,1,unsigned(n),f);output_bytes+=written;if(written!=unsigned(n)||ferror(f))output_error=true;}
 }
};
static_assert(sizeof(observer)*64+(2ULL<<20)<STATE_LIMIT,"lifetime observer fixed-state budget exceeded");
inline observer* create(unsigned slice) {
 if(!enabled() || slice>=registry.size())return nullptr;
 if(registry[slice])return registry[slice];
 if(allocated_bytes+sizeof(observer)+(2ULL<<20)>STATE_LIMIT)return nullptr;
 observer *p=new observer(slice);p->observed_state_bytes=sizeof(observer);
 registry[slice]=p;allocated_bytes+=sizeof(observer);return p;
}
inline void destroy(observer *p) {
 if(!p)return;if(p->slice<registry.size()&&registry[p->slice]==p)registry[p->slice]=nullptr;
 allocated_bytes-=sizeof(observer);delete p;
}
inline void stage_record(unsigned slice,unsigned which,uint64_t n=1) {
 if(slice<registry.size() && registry[slice])registry[slice]->record_stage(which,n);
}
// Fixed host-only callback context, not per-cache-block storage.
struct mutation_context {observer *owner=nullptr;const void *block=nullptr;unsigned index=0;};
inline thread_local mutation_context current_mutation{};
inline thread_local bool timed_mutations=true;
struct mutation_scope {
 mutation_context saved;
 mutation_scope(observer *owner,const void *block,unsigned index):saved(current_mutation){
   current_mutation={owner,block,index};
 }
 ~mutation_scope(){current_mutation=saved;}
};
struct untimed_scope {
 bool saved;untimed_scope():saved(timed_mutations){timed_mutations=false;}
 ~untimed_scope(){timed_mutations=saved;}
};
inline bool observes(const void *block) {
 return current_mutation.owner && current_mutation.block==block;
}
inline void raw_notify(const void *block,uint64_t address,unsigned reserved,unsigned valid,
                       unsigned operation,bool fresh=false) {
 if(!observes(block))return;auto *o=current_mutation.owner;
 if(!timed_mutations){if(o->active&&!o->closed)o->invalid|=8388608;return;}
 o->change(current_mutation.index,address,reserved,valid,operation,fresh);
}
struct reason_scope {
 observer *o;unsigned saved;
 reason_scope(observer *p,unsigned r):o(p),saved(p?p->why:0){if(o)o->why=r;}
 ~reason_scope(){if(o)o->why=saved;}
};
} // namespace ep_l2_life
#endif
