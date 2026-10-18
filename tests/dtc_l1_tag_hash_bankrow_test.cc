// CPU-only directed production-frontend test; no kernel/native application.
// Compile the SAME source against the fixed baseline and AE include roots.
// -fno-access-control exposes snapshots without changing production headers.
#include "dtc-l1-common.h"
#include "option_parser.cc"  // Actual parser, one translation unit per build.
#include <iostream>
#include <string>
#include <sys/resource.h>

#ifdef NDEBUG
#error Assertions required for this CPU qualification
#endif
using namespace dtc_l1;
static config settings(unsigned sets,unsigned ways,int setting){
 config c;c.selected_mode=mode::PAPER_OO;c.finite_service=true;
 c.logical_sets=sets;c.logical_ways=ways;
#ifdef DTC_AE_NEW
 if(setting>=0)c.tag_hash_bankrow=setting;
#else
 assert(setting<=0);
#endif
 return c;
}
static unsigned actual_set(const oo_frontend&f,uint64_t a){
#ifdef DTC_AE_NEW
 return f.tag_set(a);
#else
 return static_cast<unsigned>((a/128)%f.m_cfg.logical_sets);
#endif
}
static void state(const oo_frontend&f,const char*phase,uint64_t cycle,uint64_t uid,uint64_t a,int kind){
 f.assert_shadow_refs();std::cout<<"{\"phase\":\""<<phase<<"\",\"cycle\":"<<cycle<<",\"uid\":"<<uid<<",\"address\":"<<a<<",\"set\":"<<actual_set(f,a)<<",\"kind\":"<<kind<<",\"misses\":"<<f.new_misses()<<",\"hits\":"<<f.valid_hits()<<",\"pending\":"<<f.pending_hits()<<",\"evictions\":"<<f.tag_evictions()<<",\"allocated\":"<<f.allocated_lines()<<",\"refs\":"<<f.active_refs()<<",\"pib\":"<<f.occupancy()<<",\"clock\":"<<f.m_lru_clock<<",\"rr\":"<<f.m_rr_next<<",\"tag_requests\":"<<f.tag_requests()<<",\"conflicts\":"<<f.tag_conflicts()<<",\"map_loss\":["<<f.map_loss_to_free_sum()<<","<<f.map_loss_to_free_count()<<"],\"tags\":[";
 bool comma=false;for(unsigned i=0;i<f.m_tags.size();++i){const auto&t=f.m_tags[i];if(!t.valid)continue;if(comma)std::cout<<",";comma=true;std::cout<<"["<<i<<","<<t.line<<","<<t.physical.id<<","<<t.physical.generation<<","<<t.lru<<"]";}
 std::cout<<"],\"physical\":[";comma=false;for(unsigned i=0;i<f.m_phys.size();++i){const auto&p=f.m_phys[i];if(!p.allocated)continue;if(comma)std::cout<<",";comma=true;std::cout<<"["<<i<<","<<p.generation<<","<<p.tag_valid<<","<<p.ref_count<<","<<p.ready<<","<<p.waiters.size()<<"]";}std::cout<<"]}\n";
}
static io_access_result transaction(oo_frontend&f,uint64_t uid,uint64_t address){
 const uint64_t t=4*uid;f.advance_service(t);assert(f.try_serve_tag(t,address));
 service_entry service;service.tag_grant_cycle=t;assert(!service.tag_can_commit(t)&&service.tag_can_commit(t+1));
 // Preserve historical precheck recency, even though this test has no full queue.
 bool miss=f.will_new_miss(address);assert(f.admit(uid));f.advance_service(t+1);
 auto r=f.access(t+1,uid,address);assert((r.kind==io_access_kind::NEW_MISS)==miss);
 assert(r.kind!=io_access_kind::NO_FREE_LINE);if(miss)f.complete(r.physical);
 f.advance_service(t+2);assert(f.entry_ready_for(uid));assert(f.retire_ready_uid(t+3,uid));
 state(f,"request",t+3,uid,address,static_cast<int>(r.kind));return r;
}
static void trace(config c,const std::string&sequence){
 oo_frontend f(c);uint64_t uid=0;
 for(unsigned pass=0;pass<3;++pass){unsigned count=sequence=="adversary"?c.logical_ways+1:32;
  for(unsigned k=0;k<count;++k){uint64_t line=sequence=="stride4096"?32ULL*k:sequence=="neutral"?k:4ULL*k+uint64_t(k)*c.logical_sets;transaction(f,++uid,line*128);}}
 std::cout<<"{\"summary\":\"trace\",\"sequence\":\""<<sequence<<"\",\"requests\":"<<uid<<",\"misses\":"<<f.new_misses()<<",\"hits\":"<<f.valid_hits()<<",\"evictions\":"<<f.tag_evictions()<<",\"allocated\":"<<f.allocated_lines()<<",\"refs\":"<<f.active_refs()<<",\"tag_count\":"<<f.m_tags.size()<<",\"physical_capacity\":"<<f.m_phys.size()<<",\"pib_capacity\":"<<f.m_slots.size()<<",\"tag_entry_bytes\":"<<sizeof(f.m_tags[0])<<",\"physical_entry_bytes\":"<<sizeof(f.m_phys[0])<<"}\n";
}
static void mappings(config c){
 oo_frontend f(c);uint64_t digest=14695981039346656037ULL,count=0;
 auto visit=[&](uint64_t address){unsigned set=actual_set(f,address);assert(set<c.logical_sets);for(unsigned i=0;i<8;++i){digest^=(address>>(8*i))&255;digest*=1099511628211ULL;}for(unsigned i=0;i<4;++i){digest^=(set>>(8*i))&255;digest*=1099511628211ULL;}++count;};
 for(uint64_t line=0;line<4096;++line)for(unsigned offset:{0U,1U,127U})visit(line*128+offset);
 uint64_t x=0x123456789abcdef0ULL;for(unsigned i=0;i<64;++i){x^=x<<13;x^=x>>7;x^=x<<17;visit((x&((1ULL<<57)-1))<<7);}
 std::cout<<"{\"summary\":\"mapping\",\"count\":"<<count<<",\"fnv64\":"<<digest<<",\"stride_sets\":[";
 for(unsigned bank=0;bank<4;++bank){if(bank)std::cout<<",";std::cout<<"[";for(unsigned k=0;k<32;++k){if(k)std::cout<<",";std::cout<<actual_set(f,4096ULL*k+128*bank);}std::cout<<"]";}
 std::cout<<"]}\n";
}
static void conflicts(config c){
 oo_frontend f(c);for(unsigned bank=0;bank<4;++bank){assert(f.try_serve_tag(1,128*bank));assert(!f.try_serve_tag(1,4096+128*bank));}
 assert(f.tag_requests()==4&&f.tag_conflicts()==4);
 c.tag_requests_per_cycle=1;oo_frontend narrow(c);assert(narrow.try_serve_tag(1,0));assert(!narrow.try_serve_tag(1,128));assert(narrow.try_serve_tag(2,128));
 std::cout<<"{\"summary\":\"conflicts\",\"default_granted\":"<<f.tag_requests()<<",\"default_denied\":"<<f.tag_conflicts()<<",\"global1_granted\":"<<narrow.tag_requests()<<",\"global1_denied\":"<<narrow.tag_conflicts()<<"}\n";
}
static void lru(config c){
 oo_frontend f(c);std::vector<uint64_t>a;for(uint64_t line=0;line<65536&&a.size()<c.logical_ways+1;++line)if(actual_set(f,line*128)==0)a.push_back(line*128);assert(a.size()==c.logical_ways+1);
 for(unsigned i=0;i<c.logical_ways;++i)transaction(f,i+1,a[i]);
 auto clock=f.m_lru_clock;assert(!f.will_new_miss(a[0]));assert(f.m_lru_clock==clock+1);assert(f.select_victim(a.back())->line==a[1]);
 transaction(f,c.logical_ways+1,a.back());assert(f.will_new_miss(a[1]));assert(!f.will_new_miss(a[0]));
 std::cout<<"{\"summary\":\"lru\",\"evicted_address\":"<<a[1]<<",\"probe_promoted_address\":"<<a[0]<<",\"evictions\":"<<f.tag_evictions()<<",\"precheck_recency_preserved\":true}\n";
}
static void late(config c){
 oo_frontend f(c);physical_identity old{};
 for(unsigned uid=1;uid<=6;++uid){f.advance_service(2*uid);assert(f.try_serve_tag(2*uid,0));assert(f.admit(uid));f.advance_service(2*uid+1);auto r=f.access(2*uid+1,uid,0);if(uid==1){assert(r.kind==io_access_kind::NEW_MISS);old=r.physical;}else assert(r.kind==io_access_kind::PENDING_HIT);}
 assert(f.try_serve_tag(13,0));service_entry grant;grant.tag_grant_cycle=13;f.complete(old);f.advance_service(14);assert(f.wakeups()==4&&!f.entry_ready_for(1));assert(grant.tag_can_commit(14));assert(f.admit(7));assert(f.access(14,7,0).kind==io_access_kind::PENDING_HIT);
 auto e=f.invalidate_lookup(14);assert(e.changed==1&&e.retained==1&&e.reclaimed==0);assert(f.ref_count(old)==7&&!f.tag_visible(old));assert(f.will_new_miss(0));assert(!f.invalidate_lookup(14).changed);state(f,"old_hidden",14,7,0,-1);
 assert(f.try_serve_tag(14,0));f.advance_service(15);assert(f.wakeups()==7);assert(f.admit(8));auto next=f.access(15,8,0);assert(next.kind==io_access_kind::NEW_MISS&&next.physical.id!=old.id);assert(f.ref_count(old)==7&&f.fill_identity_matches(old));f.complete(next.physical);state(f,"same_address_new",15,8,0,int(next.kind));
 for(unsigned uid=1;uid<=7;++uid){f.advance_service(15+uid);assert(f.retire_ready_uid(15+uid,uid));assert(f.is_allocated(old)==(uid!=7));}
 f.advance_service(23);assert(f.retire_ready_uid(23,8));assert(f.invalidate_lookup(24).reclaimed==1);assert(f.active_refs()==0&&f.allocated_lines()==0&&f.occupancy()==0);assert(f.map_loss_to_free_sum()==8&&f.map_loss_to_free_count()==2);
 std::cout<<"{\"summary\":\"late_k1\",\"wakeups\":"<<f.wakeups()<<",\"pending_hits\":"<<f.pending_hits()<<",\"misses\":"<<f.new_misses()<<",\"retired\":"<<f.retires()<<",\"refs\":"<<f.active_refs()<<",\"allocated\":"<<f.allocated_lines()<<",\"map_loss_sum\":"<<f.map_loss_to_free_sum()<<",\"map_loss_count\":"<<f.map_loss_to_free_count()<<"}\n";
}
int main(int argc,const char**argv){
 rlimit limit{0,0};setrlimit(RLIMIT_CORE,&limit);if(argc<2)return 64;std::string test=argv[1];
 if(test=="options"){
  auto p=option_parser_create();bool hash=false;
#ifdef DTC_AE_NEW
  register_tag_hash_bankrow_option(p,&hash);
#endif
  option_parser_cmdline(p,argc-1,argv+1);option_parser_print(p,stdout);std::cout<<"{\"summary\":\"options\",\"hash\":"<<hash<<"}\n";return 0;
 }
 if(argc<5)return 64;unsigned sets=std::stoul(argv[2]),ways=std::stoul(argv[3]);int setting=std::string(argv[4])=="omit"?-1:std::stoi(argv[4]);config c=settings(sets,ways,setting);
 if(test=="trace"){if(argc!=6)return 64;trace(c,argv[5]);}
 else if(test=="mapping")mappings(c);
 else if(test=="conflicts")conflicts(c);
 else if(test=="lru")lru(c);
 else if(test=="late")late(c);
#ifdef DTC_AE_NEW
 else if(test=="reject"){
  if(argc!=6)return 64;std::string why=argv[5];if(why=="finite0")c.finite_service=false;if(why.rfind("banks",0)==0)c.tag_banks=std::stoul(why.substr(5));
  if(why=="LEGACY"||why=="BASE"){c.selected_mode=why=="BASE"?mode::PAPER_BASE:mode::LEGACY;paper_frontend reject(c);}
  else if(why=="IO"){c.selected_mode=mode::PAPER_IO;io_frontend reject(c);}
  else if(why=="MODERN"){c.selected_mode=mode::MODERN_OO_SECTOR;sector_oo_frontend reject(c);}
  else{oo_frontend reject(c);}return 3;
 }
#endif
 else return 64;return 0;
}
