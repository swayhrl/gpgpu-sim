#ifndef EP_L2_EVENT_TIMELINE_H
#define EP_L2_EVENT_TIMELINE_H
// EP-only passive bounded endpoint recorder. Never supplies modeled decisions.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <zlib.h>
namespace ep_et {
constexpr uint64_t U64=~uint64_t(0);
constexpr unsigned U16=65535,U8=255,SLICES=64,RING=512,PRE=512,POST=1536,SNAPS=4;
constexpr uint64_t STATE_CAP=8ull<<20,OUTPUT_CAP=16ull<<20,AUX_STATE_BOUND=8192;
constexpr const char *SCHEMA="EP_L2_ENDPOINT_TIMELINE_V1";
constexpr const char *SCHEMA_SHA="e5b078dc5a544c9ae5ba0d3415b017be92f2961b36e8a7ad2a219098388f8bfa";
enum domain : uint8_t { UNKNOWN_DOMAIN=0,CORE_DOMAIN=1,L2_DOMAIN=2,DRAM_DOMAIN=3,ICNT_DOMAIN=4,UNTIMED_DOMAIN=5 };
enum kind : uint8_t {
 L1_SEND=1,L1_SEND_DENY,L2_INGRESS,FRONTEND_WAIT,FRONTEND_ADMIT_DECISION,
 TAG_MUTATION,RO_STATE,RO_RETIRE,RO_LOWER_CREATE,RO_BIND,
 MISSQ_ENQUEUE,MISSQ_DEQUEUE,LOWER_ENQUEUE,LOWER_DEQUEUE,LOWER_REJECT,
 DRAM_DELAY_ENQUEUE,DRAM_MC_PUSH,SCHEDULER_ACCEPT,DRAM_RETURN,RO_READY,
 RESPONSE_ACCEPT,ICNT_INJECT,PACKET_SPLIT,PRESSURE_NEW,PRODUCTION_TAGFAIL,
 DRAM_COLUMN,RO_DETACH,MISSQ_REJECT,FRONTEND_COMMIT
};
// All serialized integer fields are explicitly little endian. No pointer values.
struct event {
 uint64_t ordinal=0,core=U64,l2=U64,dram=U64,address=U64,generation=U64,aux=U64,uid=U64,parent=U64;
 uint16_t slice=U16,set=U16;uint8_t type=0,phase=0,way=U8,sectors=U8;
};
static_assert(sizeof(event)==80,"reviewed80-byte event");
struct ring {std::array<event,RING> data{};uint64_t total=0;unsigned head=0,count=0;};
struct snapshot {
 std::array<event,PRE+POST> data{};
 uint64_t trigger=0,trigger_uid=U64,trigger_address=U64,pre_lost=0;
 unsigned slice=U16,set=U16,ways=0,reserved=0,rank=0,pre=0,post=0,count=0;
 bool used=false;
};
struct kernel_record {
 uint64_t uid=U64,start_order=0,end_order=0,start_core=U64,end_core=U64;
 unsigned name_length=0;bool name_truncated=false;std::array<char,256> name{};
};
struct observer {
 std::array<ring,SLICES> rings{};
 std::array<snapshot,SNAPS> snaps{};
 std::array<kernel_record,64> kernels{};
 std::array<unsigned char,512*1024> arena{};
 std::array<unsigned char,16384> zipout{};
 std::array<char,4*((16384+2)/3)+1> encoded{};
 uint64_t ordinal=0,core=U64,l2=U64,dram=U64,l2_edges=0,dram_edges=0;
 uint64_t first_core=U64,last_core=U64,notifications=0,unmapped=0,untimed=0,quiesced=0;
 uint64_t new_head_seen=0,frontend_notifications=0,frontend_wait_notifications=0,frontend_admit_decisions=0,frontend_commit_events=0;
 uint64_t high_new=0,full_new=0,production_fail=0,trigger_capacity_miss=0,trigger_repeat=0,critical_replaced=0;
 uint64_t full_trigger_not_captured=0,production_fail_not_captured=0,replacement_events_discarded=0,replacement_full_new_discarded=0;
 uint64_t invalid=0,wire_bytes=0,compressed_bytes=0,arena_peak=0;
 unsigned geometry_sets=0,geometry_ways=0,geometry_slices=0,nkernels=0;
 size_t arena_used=0;
 bool clock_known=false,finished=false,printed=false,output_overflow=false,output_error=false;
 uint64_t wire_limit=OUTPUT_CAP,compressed_limit=OUTPUT_CAP;
 // Budget reductions are used only by isolated engineering fixtures.
 explicit observer(uint64_t test_wire_limit=OUTPUT_CAP,uint64_t test_zip_limit=OUTPUT_CAP):
  wire_limit(std::min(test_wire_limit,OUTPUT_CAP)),compressed_limit(std::min(test_zip_limit,OUTPUT_CAP)){}
 bool add(uint64_t &a,uint64_t b=1){if(b>U64-a){invalid|=1;return false;}a+=b;return true;}
 void geometry(unsigned n,unsigned s,unsigned w) {
  if(geometry_slices && (geometry_slices!=n||geometry_sets!=s||geometry_ways!=w)){invalid|=2;return;}
  if(n!=64 || (s!=32&&s!=64) || (w!=8&&w!=16)){invalid|=2;return;}
  geometry_slices=n;geometry_sets=s;geometry_ways=w;
 }
 void clock(uint64_t c,unsigned mask) {
  if(finished)return;
  if(clock_known&&c<core){invalid|=4;return;}core=c;clock_known=true;last_core=c;if(first_core==U64)first_core=c;
  if(mask&2){l2=l2_edges;add(l2_edges);}
  if(mask&4){dram=dram_edges;add(dram_edges);}
 }
 bool full()const {
  for(auto &s:snaps)if(!s.used||s.post<POST)return false;
  return snaps[3].rank==2;
 }
 void record(event e) {
  if(finished)return;add(notifications);
  if(!add(ordinal))return;e.ordinal=ordinal;e.core=core;e.l2=l2;e.dram=dram;
  const bool critical=e.type==PRODUCTION_TAGFAIL;
  const bool full_new_event=e.type==PRESSURE_NEW && uint32_t(e.aux)==uint32_t(e.aux>>32);
  auto missed=[&](){if(critical)add(production_fail_not_captured);if(full_new_event)add(full_trigger_not_captured);};
  if(!clock_known){add(untimed);missed();return;}
  if(e.type==FRONTEND_WAIT||e.type==FRONTEND_ADMIT_DECISION)add(frontend_notifications);
  if(e.type==FRONTEND_WAIT)add(frontend_wait_notifications);
  if(e.type==FRONTEND_ADMIT_DECISION)add(frontend_admit_decisions);
  if(e.type==FRONTEND_COMMIT)add(frontend_commit_events);
  if(e.slice>=SLICES){add(unmapped);missed();return;}
  if(full()){add(quiesced);missed();return;}
  auto &r=rings[e.slice];r.data[r.head]=e;r.head=(r.head+1)%RING;if(r.count<RING)++r.count;add(r.total);
  bool captured=false;
  for(auto &s:snaps)if(s.used&&s.slice==e.slice&&s.post<POST){
   if(s.count>=s.data.size()){invalid|=8;continue;}s.data[s.count++]=e;++s.post;captured=true;
  }
  if(!captured)missed();
 }
 void pressure(unsigned si,unsigned st,unsigned ways,unsigned reserved,uint64_t uid,uint64_t addr,bool actual_fail=false) {
  if(finished)return;
  if(actual_fail)add(production_fail);
  else {add(new_head_seen);if(reserved<(ways==8?4u:8u))return;add(high_new);if(reserved==ways)add(full_new);}
  if(si>=64 || st>=geometry_sets || (ways!=8&&ways!=16) || (reserved>ways && reserved!=U16)){invalid|=16;return;}
  unsigned rank=actual_fail?2:reserved==ways?1:0;int selected=-1;
  if(rank){
   auto &s=snaps[3];
   if(!s.used || rank>s.rank){if(s.used){add(critical_replaced);add(replacement_events_discarded,s.count);for(unsigned i=0;i<s.count;++i)if(s.data[i].type==PRESSURE_NEW&&uint32_t(s.data[i].aux)==uint32_t(s.data[i].aux>>32))add(replacement_full_new_discarded);}selected=3;}
   else if(s.slice==si&&s.set==st){add(trigger_repeat);}
   else add(trigger_capacity_miss);
  }else {
   for(unsigned i=0;i<3;++i)if(snaps[i].used&&snaps[i].slice==si&&snaps[i].set==st){add(trigger_repeat);selected=-2;break;}
   if(selected!=-2)for(unsigned i=0;i<3;++i)if(!snaps[i].used){selected=i;break;}
   if(selected==-1)add(trigger_capacity_miss);
  }
  if(selected>=0){
   auto &s=snaps[selected];s=snapshot();s.used=true;s.slice=si;s.set=st;s.ways=ways;s.reserved=reserved;s.rank=rank;s.trigger_uid=uid;s.trigger_address=addr;
   if(ordinal==U64){invalid|=1;return;}s.trigger=ordinal+1;
   const auto &r=rings[si];s.pre=std::min<unsigned>(r.count,PRE);s.count=s.pre;s.pre_lost=r.total-s.pre;
   for(unsigned n=0;n<s.pre;++n)s.data[n]=r.data[(r.head+RING-s.pre+n)%RING];
  }
  event e;e.type=actual_fail?PRODUCTION_TAGFAIL:PRESSURE_NEW;e.phase=L2_DOMAIN;e.slice=si;e.set=st;e.uid=uid;e.address=addr;e.aux=(uint64_t(ways)<<32)|reserved;record(e);
 }
 void kernel_start(uint64_t uid,const char *name,uint64_t c) {
  if(finished)return;
  for(unsigned i=0;i<nkernels;++i)if(kernels[i].uid==uid){invalid|=32;return;}
  if(nkernels==kernels.size()){invalid|=32;return;}
  auto &k=kernels[nkernels++];k.uid=uid;k.start_core=c;if(!add(ordinal))return;k.start_order=ordinal;
  if(!name)name="";const size_t n=std::strlen(name);if(n>std::numeric_limits<unsigned>::max()){invalid|=32;return;}
  k.name_length=n;k.name_truncated=n>=k.name.size();if(k.name_truncated)invalid|=64;
  std::memcpy(k.name.data(),name,std::min(n,k.name.size()-1));
 }
 void kernel_end(uint64_t uid,uint64_t c) {
  if(finished)return;for(unsigned i=0;i<nkernels;++i)if(kernels[i].uid==uid){
   auto &k=kernels[i];if(k.end_order){invalid|=32;return;}if(!add(ordinal))return;k.end_order=ordinal;k.end_core=c;return;
  }invalid|=32;
 }
 bool write(FILE *f,const char *p,size_t n,bool terminal=false) {
  const uint64_t reserve=terminal?0:1024;
  if(wire_bytes>wire_limit || reserve>wire_limit-wire_bytes || n>wire_limit-wire_bytes-reserve){output_overflow=true;invalid|=128;return false;}
  if(std::fwrite(p,1,n,f)!=n || std::ferror(f)){output_error=true;invalid|=256;return false;}
  wire_bytes+=n;return true;
 }
 template<class...A> bool line(FILE*f,const char*fmt,A...args) {
  char b[2048];int n=std::snprintf(b,sizeof(b),fmt,args...);
  if(n<0||size_t(n)>=sizeof(b)){invalid|=128;return false;}return write(f,b,n);
 }
 static void *zallocate(void *self,uInt n,uInt size) {
  auto *o=static_cast<observer*>(self);
  if(n && size>o->arena.size()/n){o->invalid|=512;return nullptr;}
  size_t bytes=size_t(n)*size,start=(o->arena_used+15)&~size_t(15);
  if(start>o->arena.size()||bytes>o->arena.size()-start){o->invalid|=512;return nullptr;}
  void *p=o->arena.data()+start;o->arena_used=start+bytes;o->arena_peak=std::max<uint64_t>(o->arena_peak,o->arena_used);return p;
 }
 static void zrelease(void*,void*){}
 static void put64(unsigned char *p,uint64_t n){for(unsigned i=0;i<8;++i)p[i]=n>>(8*i);}
 static void encode_event(const event&e,unsigned char*p){
  const uint64_t fields[]={e.ordinal,e.core,e.l2,e.dram,e.address,e.generation,e.aux,e.uid,e.parent};
  for(unsigned i=0;i<9;++i)put64(p+8*i,fields[i]);
  p[72]=e.slice;p[73]=e.slice>>8;p[74]=e.set;p[75]=e.set>>8;p[76]=e.type;p[77]=e.phase;p[78]=e.way;p[79]=e.sectors;
 }
 bool chunk(FILE*f,unsigned slot,unsigned seq,const unsigned char*p,unsigned n) {
  if(compressed_bytes>compressed_limit || n>compressed_limit-compressed_bytes){invalid|=128;output_overflow=true;return false;}
  static const char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t q=0;for(unsigned i=0;i<n;i+=3){
   unsigned v=unsigned(p[i])<<16;if(i+1<n)v|=unsigned(p[i+1])<<8;if(i+2<n)v|=p[i+2];
   encoded[q++]=b64[v>>18];encoded[q++]=b64[(v>>12)&63];encoded[q++]=i+1<n?b64[(v>>6)&63]:'=';encoded[q++]=i+2<n?b64[v&63]:'=';
  }encoded[q]=0;
  char header[96];int h=std::snprintf(header,sizeof(header),"EPETV1|DATA|slot=%u|seq=%u|data=",slot,seq);
  if(h<0||size_t(h)>=sizeof(header)){invalid|=128;return false;}
  if(!write(f,header,h)||!write(f,encoded.data(),q)||!write(f,"\n",1))return false;
  compressed_bytes+=n;return true;
 }
 bool zip_snapshot(FILE*f,unsigned slot) {
  auto &s=snaps[slot];arena_used=0;z_stream z{};z.zalloc=zallocate;z.zfree=zrelease;z.opaque=this;
  if(deflateInit2(&z,Z_BEST_SPEED,Z_DEFLATED,15,1,Z_DEFAULT_STRATEGY)!=Z_OK){invalid|=512;return false;}
  unsigned seq=0;uint64_t before=compressed_bytes;uLong crc=crc32(0L,Z_NULL,0);bool okay=true;
  for(unsigned n=0;n<s.count&&okay;++n){
   unsigned char raw[80];encode_event(s.data[n],raw);crc=crc32(crc,raw,80);z.next_in=raw;z.avail_in=80;
   do{z.next_out=zipout.data();z.avail_out=zipout.size();int ret=deflate(&z,Z_NO_FLUSH);
    if(ret!=Z_OK){invalid|=512;okay=false;break;}unsigned bytes=zipout.size()-z.avail_out;
    if(bytes&&!chunk(f,slot,seq++,zipout.data(),bytes)){okay=false;break;}
   }while(z.avail_in);
  }
  if(okay){int ret;do{z.next_out=zipout.data();z.avail_out=zipout.size();ret=deflate(&z,Z_FINISH);
    if(ret!=Z_OK&&ret!=Z_STREAM_END){invalid|=512;okay=false;break;}
    unsigned bytes=zipout.size()-z.avail_out;if(bytes&&!chunk(f,slot,seq++,zipout.data(),bytes)){okay=false;break;}
   }while(ret!=Z_STREAM_END);}
  deflateEnd(&z);
  if(okay)return line(f,"EPETV1|SNAP_END|slot=%u|events=%u|raw_bytes=%llu|compressed_bytes=%llu|raw_crc32=%lu\n",slot,s.count,(unsigned long long)s.count*80,(unsigned long long)(compressed_bytes-before),(unsigned long)crc);
  return false;
 }
 void finish(FILE*f,uint64_t end_core,bool completed) {
  if(printed)return;finished=printed=true;last_core=end_core;
  for(unsigned i=0;i<nkernels;++i)if(!kernels[i].end_order)invalid|=1024;
  if(!geometry_slices)invalid|=2;
  line(f,"EPETV1|META|schema=%s|schema_sha256=%s|record_bytes=80|endian=little|slices=%u|sets=%u|ways=%u|ring=%u|pre=%u|post=%u|slots=4|state_bytes=%llu|state_object_bytes=%llu|aux_state_bound=8192|state_cap=%llu|compressed_cap=%llu|wire_cap=%llu|first_core=%llu|end_core=%llu|l2_edges=%llu|dram_edges=%llu|ordinal=%llu|notifications=%llu|untimed_ignored=%llu|unmapped=%llu|quiesced=%llu|new_head_seen=%llu|frontend_notifications=%llu|frontend_wait_notifications=%llu|frontend_admit_decisions=%llu|frontend_commit_events=%llu|high_new=%llu|full_new=%llu|production_fail=%llu|trigger_capacity_miss=%llu|trigger_repeat=%llu|critical_replaced=%llu|full_trigger_not_captured=%llu|production_fail_not_captured=%llu|replacement_events_discarded=%llu|replacement_full_new_discarded=%llu\n",SCHEMA,SCHEMA_SHA,geometry_slices,geometry_sets,geometry_ways,RING,PRE,POST,(unsigned long long)(sizeof(observer)+AUX_STATE_BOUND),(unsigned long long)sizeof(observer),(unsigned long long)STATE_CAP,(unsigned long long)compressed_limit,(unsigned long long)wire_limit,(unsigned long long)first_core,(unsigned long long)end_core,(unsigned long long)l2_edges,(unsigned long long)dram_edges,(unsigned long long)ordinal,(unsigned long long)notifications,(unsigned long long)untimed,(unsigned long long)unmapped,(unsigned long long)quiesced,(unsigned long long)new_head_seen,(unsigned long long)frontend_notifications,(unsigned long long)frontend_wait_notifications,(unsigned long long)frontend_admit_decisions,(unsigned long long)frontend_commit_events,(unsigned long long)high_new,(unsigned long long)full_new,(unsigned long long)production_fail,(unsigned long long)trigger_capacity_miss,(unsigned long long)trigger_repeat,(unsigned long long)critical_replaced,(unsigned long long)full_trigger_not_captured,(unsigned long long)production_fail_not_captured,(unsigned long long)replacement_events_discarded,(unsigned long long)replacement_full_new_discarded);
  for(unsigned i=0;i<nkernels;++i){auto &k=kernels[i];char name[511];size_t n=0;const char hex[]="0123456789abcdef";
   for(unsigned b=0;b<std::min<unsigned>(k.name_length,255);++b){unsigned c=uint8_t(k.name[b]);name[n++]=hex[c>>4];name[n++]=hex[c&15];}name[n]=0;
   line(f,"EPETV1|KERNEL|uid=%llu|start_ordinal=%llu|end_ordinal=%llu|start_core=%llu|end_core=%llu|name_length=%u|name_truncated=%u|name_hex=%s\n",(unsigned long long)k.uid,(unsigned long long)k.start_order,(unsigned long long)k.end_order,(unsigned long long)k.start_core,(unsigned long long)k.end_core,k.name_length,k.name_truncated,name);
  }
  unsigned used=0;
  for(unsigned i=0;i<SNAPS;++i)if(snaps[i].used){auto&s=snaps[i];++used;
   line(f,"EPETV1|SNAP|slot=%u|slice=%u|set=%u|ways=%u|reserved=%u|rank=%u|trigger_ordinal=%llu|trigger_uid=%llu|trigger_address=%llu|pre=%u|post=%u|pre_lost=%llu|post_complete=%u|events=%u\n",i,s.slice,s.set,s.ways,s.reserved,s.rank,(unsigned long long)s.trigger,(unsigned long long)s.trigger_uid,(unsigned long long)s.trigger_address,s.pre,s.post,(unsigned long long)s.pre_lost,s.post==POST,s.count);
   if(!zip_snapshot(f,i))break;
  }
  char end[1024];int n=std::snprintf(end,sizeof(end),"EPETV1|END|completed=%u|slots=%u|invalid=%llu|output_overflow=%u|output_error=%u|compressed_bytes=%llu|wire_bytes_before_END=%llu|z_arena_peak=%llu|closed=1\n",completed,used,(unsigned long long)invalid,output_overflow,output_error,(unsigned long long)compressed_bytes,(unsigned long long)wire_bytes,(unsigned long long)arena_peak);
  if(n>0&&size_t(n)<sizeof(end))write(f,end,n,true);
 }
};
static_assert(sizeof(observer)+AUX_STATE_BOUND<STATE_CAP,"new timeline state bound incl fixed compressor arena");
inline std::mutex guard;
inline observer *instance=nullptr;
inline bool allocation_failed=false;
inline bool enabled(){static const bool on=[](){const char*p=std::getenv("EP_L2_EVENT_TIMELINE");return p&&std::strcmp(p,"1")==0;}();return on;}
inline observer *get(){if(!instance&&!allocation_failed){instance=new(std::nothrow)observer();if(!instance)allocation_failed=true;}return instance;}
inline void geometry(unsigned slices,unsigned sets,unsigned ways){if(!enabled())return;std::lock_guard<std::mutex>g(guard);if(auto*o=get())o->geometry(slices,sets,ways);}
inline void clock(uint64_t core,unsigned mask){if(!enabled())return;std::lock_guard<std::mutex>g(guard);if(auto*o=get())o->clock(core,mask);}
inline void emit(kind k,domain d,unsigned si=U16,unsigned st=U16,unsigned way=U8,unsigned mask=U8,uint64_t uid=U64,uint64_t parent=U64,uint64_t addr=U64,uint64_t generation=U64,uint64_t aux=U64) {
 if(!enabled())return;std::lock_guard<std::mutex>g(guard);if(auto*o=get()){event e;e.type=k;e.phase=d;e.slice=si<=U16?si:U16;e.set=st<=U16?st:U16;e.way=way<=U8?way:U8;e.sectors=mask<=U8?mask:U8;e.uid=uid;e.parent=parent;e.address=addr;e.generation=generation;e.aux=aux;o->record(e);}
}
template<class MF> inline void packet(kind k,domain d,MF *m,uint64_t aux=U64,uint64_t parent=U64) {
 if(!enabled()||!m)return;
 emit(k,d,m->get_sub_partition_id(),U16,U8,m->get_access_sector_mask().to_ulong(),uint64_t(m->get_request_uid()),parent,m->get_addr(),m->has_ep_l2_payload_identity()?uint64_t(m->get_ep_l2_payload_generation()):U64,aux);
}
inline void pressure(unsigned slice,unsigned set,unsigned ways,unsigned reserved,uint64_t uid,uint64_t addr,bool actual_fail=false){if(!enabled())return;std::lock_guard<std::mutex>g(guard);if(auto*o=get())o->pressure(slice,set,ways,reserved,uid,addr,actual_fail);}
inline void kernel_launch(uint64_t uid,const char*name,uint64_t core){if(!enabled())return;std::lock_guard<std::mutex>g(guard);if(auto*o=get())o->kernel_start(uid,name,core);}
inline void kernel_finish(uint64_t uid,uint64_t core){if(!enabled())return;std::lock_guard<std::mutex>g(guard);if(auto*o=get())o->kernel_end(uid,core);}
inline void finish(FILE*f,uint64_t core,bool completed){if(!enabled())return;std::lock_guard<std::mutex>g(guard);if(auto*o=get())o->finish(f,core,completed);else std::fprintf(f,"EPETV1|END|completed=0|invalid=2048|output_error=1|allocation_failed=1|closed=1\n");}
} // namespace ep_et
#endif
