#ifndef EP_L2_DETACH_DIAG_H
#define EP_L2_DETACH_DIAG_H
// Passive diagnostic state only. No field below supplies modeled decisions.
#include "ep-l2-tagpressure-diag.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ep_l2_detach_diag {
constexpr unsigned MAX_SLICES=64, MAX_KERNELS=16, PHASES=MAX_KERNELS+2, MAX_SAMPLES=64;
enum field {
observed_heads,
probe_skipped,
skip_tvd,
skip_ideal,
skip_disabled,
skip_other,
head_absent_cycles,
raw_hit,
raw_hit_reserved,
raw_miss,
raw_reservation_fail,
raw_sector_miss,
raw_mshr_hit,
raw_write_allocated,
raw_unknown,
requester_ro_candidate,
requester_nonro,
requester_ro_runtime_off,
certified_resident_hits,
candidate_deferred_or_native,
pressure_T_disabled,
pressure_T_enabled,
pressure_no_live_ro,
pressure_live_ro_no_eligible,
pressure_eligible,
ways_examined,
way_no_live_ro,
way_not_attached,
way_reuse_seen,
way_no_missing,
way_eligible,
pressure_prod_probe_reached,
pressure_prod_probe_not_reached,
pressure_rewrite_miss,
pressure_rewrite_hit_reserved,
pressure_kept_fail,
pressure_relief_not_consulted,
pressure_intermediate_other,
pressure_frontend_admitted,
pressure_frontend_denied,
rewritten_final_admitted,
rewritten_final_denied,
rewritten_final_unknown,
preview_early_owner,
preview_early_directory,
preview_early_query,
preview_early_wad,
preview_early_tvd,
preview_other,
primary_owner,
primary_directory,
primary_query,
primary_wad,
primary_descriptor,
primary_address,
primary_line,
primary_missq,
primary_data,
primary_response,
primary_unknown,
primary_none,
mask_descriptor,
mask_address,
mask_line,
mask_missq,
mask_data,
mask_response,
mask_wad,
mask_owner,
mask_directory,
mask_query,
ft_access_attempts,
commit_native_owner,
commit_merge,
commit_directory,
commit_promotion,
commit_raw_probe_reached,
raw_tag_fail_commit,
relieve_calls,
relieve_T_disabled,
relieve_no_eligible,
relieve_eligible_selected,
detach_calls,
detach_success,
detach_false,
detach_false_T_disabled,
detach_false_not_attached,
detach_false_reuse,
detach_false_ready,
detach_exception,
selected_identity_mismatch,
post_relief_hit,
post_relief_hit_reserved,
post_relief_miss,
post_relief_fail,
post_relief_sector_miss,
post_relief_mshr_hit,
post_relief_write_allocated,
post_relief_unknown,
commit_accepted,
commit_denied,
commit_exception,
trigger_accepted_after_detach,
trigger_denied_after_detach,
trigger_exception_after_detach,
denied_owner,
denied_directory,
denied_query,
denied_promotion,
denied_tag,
denied_capacity,
denied_held,
denied_wad,
denied_pin,
denied_mshr,
denied_missq,
denied_wad_full,
denied_wb,
denied_landing,
denied_quota,
denied_copy,
denied_bank,
denied_baseline,
denied_other,
ro_admits,
ro_merges,
reuse_seen_first_transition,
ro_responses,
ro_retired,
detached_return_packets,
detached_return_sectors,
detached_responses,
detached_retired,
response_backpressure,
closure_errors,
samples_seen,
samples_stored,
samples_dropped,
eligible_hist_0,
eligible_hist_1,
eligible_hist_2,
eligible_hist_3,
eligible_hist_4,
eligible_hist_5,
eligible_hist_6,
eligible_hist_7,
eligible_hist_8,
eligible_hist_9,
eligible_hist_10,
eligible_hist_11,
eligible_hist_12,
eligible_hist_13,
eligible_hist_14,
eligible_hist_15,
eligible_hist_16,
predicate_hist_0,
predicate_hist_1,
predicate_hist_2,
predicate_hist_3,
predicate_hist_4,
predicate_hist_5,
predicate_hist_6,
predicate_hist_7,
transition_joint_0,
transition_joint_1,
transition_joint_2,
transition_joint_3,
transition_joint_4,
transition_joint_5,
transition_joint_6,
transition_joint_7,
ro_fill_packets,
ro_capacity_population,
ro_capacity_consulted,
ro_capacity_not_consulted,
ro_line_na_M_or_existing,
ro_descriptor_checked,
ro_descriptor_pass,
ro_descriptor_fail,
ro_descriptor_unobserved,
ro_address_checked,
ro_address_pass,
ro_address_fail,
ro_address_unobserved,
ro_line_probe_checked,
ro_line_probe_pass,
ro_line_probe_fail,
ro_line_probe_unobserved,
ro_line_slots_checked,
ro_line_slots_pass,
ro_line_slots_fail,
ro_line_slots_unobserved,
eligible_frontend_admitted,
eligible_frontend_denied,
eligible_prod_probe_reached,
eligible_prod_probe_not_reached,
eligible_rewrite_miss,
eligible_rewrite_hit_reserved,
eligible_kept_fail,
eligible_relief_not_consulted,
eligible_intermediate_other,
eligible_capacity_consulted,
eligible_capacity_not_consulted,
eligible_descriptor_fail,
eligible_address_fail,
eligible_line_probe_fail,
eligible_line_slots_fail,
eligible_early_owner,
eligible_early_directory,
eligible_early_query,
eligible_early_wad,
eligible_early_tvd,
FIELD_COUNT
};
static const char *const names[]={"observed_heads","probe_skipped","skip_tvd","skip_ideal","skip_disabled","skip_other","head_absent_cycles","raw_hit","raw_hit_reserved","raw_miss","raw_reservation_fail","raw_sector_miss","raw_mshr_hit","raw_write_allocated","raw_unknown","requester_ro_candidate","requester_nonro","requester_ro_runtime_off","certified_resident_hits","candidate_deferred_or_native","pressure_T_disabled","pressure_T_enabled","pressure_no_live_ro","pressure_live_ro_no_eligible","pressure_eligible","ways_examined","way_no_live_ro","way_not_attached","way_reuse_seen","way_no_missing","way_eligible","pressure_prod_probe_reached","pressure_prod_probe_not_reached","pressure_rewrite_miss","pressure_rewrite_hit_reserved","pressure_kept_fail","pressure_relief_not_consulted","pressure_intermediate_other","pressure_frontend_admitted","pressure_frontend_denied","rewritten_final_admitted","rewritten_final_denied","rewritten_final_unknown","preview_early_owner","preview_early_directory","preview_early_query","preview_early_wad","preview_early_tvd","preview_other","primary_owner","primary_directory","primary_query","primary_wad","primary_descriptor","primary_address","primary_line","primary_missq","primary_data","primary_response","primary_unknown","primary_none","mask_descriptor","mask_address","mask_line","mask_missq","mask_data","mask_response","mask_wad","mask_owner","mask_directory","mask_query","ft_access_attempts","commit_native_owner","commit_merge","commit_directory","commit_promotion","commit_raw_probe_reached","raw_tag_fail_commit","relieve_calls","relieve_T_disabled","relieve_no_eligible","relieve_eligible_selected","detach_calls","detach_success","detach_false","detach_false_T_disabled","detach_false_not_attached","detach_false_reuse","detach_false_ready","detach_exception","selected_identity_mismatch","post_relief_hit","post_relief_hit_reserved","post_relief_miss","post_relief_fail","post_relief_sector_miss","post_relief_mshr_hit","post_relief_write_allocated","post_relief_unknown","commit_accepted","commit_denied","commit_exception","trigger_accepted_after_detach","trigger_denied_after_detach","trigger_exception_after_detach","denied_owner","denied_directory","denied_query","denied_promotion","denied_tag","denied_capacity","denied_held","denied_wad","denied_pin","denied_mshr","denied_missq","denied_wad_full","denied_wb","denied_landing","denied_quota","denied_copy","denied_bank","denied_baseline","denied_other","ro_admits","ro_merges","reuse_seen_first_transition","ro_responses","ro_retired","detached_return_packets","detached_return_sectors","detached_responses","detached_retired","response_backpressure","closure_errors","samples_seen","samples_stored","samples_dropped","eligible_hist_0","eligible_hist_1","eligible_hist_2","eligible_hist_3","eligible_hist_4","eligible_hist_5","eligible_hist_6","eligible_hist_7","eligible_hist_8","eligible_hist_9","eligible_hist_10","eligible_hist_11","eligible_hist_12","eligible_hist_13","eligible_hist_14","eligible_hist_15","eligible_hist_16","predicate_hist_0","predicate_hist_1","predicate_hist_2","predicate_hist_3","predicate_hist_4","predicate_hist_5","predicate_hist_6","predicate_hist_7","transition_joint_0","transition_joint_1","transition_joint_2","transition_joint_3","transition_joint_4","transition_joint_5","transition_joint_6","transition_joint_7","ro_fill_packets","ro_capacity_population","ro_capacity_consulted","ro_capacity_not_consulted","ro_line_na_M_or_existing","ro_descriptor_checked","ro_descriptor_pass","ro_descriptor_fail","ro_descriptor_unobserved","ro_address_checked","ro_address_pass","ro_address_fail","ro_address_unobserved","ro_line_probe_checked","ro_line_probe_pass","ro_line_probe_fail","ro_line_probe_unobserved","ro_line_slots_checked","ro_line_slots_pass","ro_line_slots_fail","ro_line_slots_unobserved","eligible_frontend_admitted","eligible_frontend_denied","eligible_prod_probe_reached","eligible_prod_probe_not_reached","eligible_rewrite_miss","eligible_rewrite_hit_reserved","eligible_kept_fail","eligible_relief_not_consulted","eligible_intermediate_other","eligible_capacity_consulted","eligible_capacity_not_consulted","eligible_descriptor_fail","eligible_address_fail","eligible_line_probe_fail","eligible_line_slots_fail","eligible_early_owner","eligible_early_directory","eligible_early_query","eligible_early_wad","eligible_early_tvd"};
struct predicate { bool live=false,attached=false,reuse=false;unsigned required=0,issued=0,ready=0,waiters=0; };
struct preview {
 ep_l2_tagpressure::snapshot tagpressure;
 bool capacity_consulted=false,line_na=false;int descriptor_check=-1,address_check=-1,line_probe_check=-1,line_slots_check=-1;
 int raw=-1,production_raw=-1,intermediate=-1,final_status=-1,early=0;
 uint64_t victim_payload=~uint64_t(0),victim_generation=0; predicate victim;
 bool production_reached=false,consulted=false,existing=false,T=false,candidate=false,runtime=false;
 unsigned set=0,ways=0,eligible=0,first=~0U;
 std::array<unsigned,5> reasons{};std::array<unsigned,8> masks{};
};
struct ledger {
 const void *owner=nullptr;unsigned slice=~0U;bool overflow=false,closed=false,ro_runtime=false,T=false,M=false;
 std::array<uint64_t,FIELD_COUNT> total{};
 std::array<std::array<uint64_t,FIELD_COUNT>,PHASES> phases{};
 std::array<uint64_t,MAX_KERNELS> kernel_uid{};std::array<bool,MAX_KERNELS> active{};
 unsigned kernel_count=0;uint64_t last_cycle=~uint64_t(0),last_uid=~uint64_t(0);unsigned last_sample=~0U;
 static void checked(uint64_t &x,uint64_t n,bool &invalid) {if(n>~uint64_t(0)-x){x=~uint64_t(0);invalid=true;}else x+=n;}
 unsigned phase()const{unsigned n=0,p=0;for(unsigned i=0;i<kernel_count;++i)if(active[i]){++n;p=i+2;}return n==0?0:n==1?p:1;}
 void add(field f,uint64_t n=1){checked(total[f],n,overflow);checked(phases[phase()][f],n,overflow);}
 bool once(uint64_t cycle,uint64_t uid){if(last_cycle==cycle&&last_uid==uid)return false;last_cycle=cycle;last_uid=uid;last_sample=~0U;return true;}
 void begin(uint64_t uid){for(unsigned i=0;i<kernel_count;++i)if(kernel_uid[i]==uid){active[i]=true;return;}if(kernel_count==MAX_KERNELS){overflow=true;return;}kernel_uid[kernel_count]=uid;active[kernel_count++]=true;}
 void end(uint64_t uid){for(unsigned i=0;i<kernel_count;++i)if(kernel_uid[i]==uid){active[i]=false;return;}overflow=true;}
 void joint(predicate p){unsigned mask=(p.attached?1:0)|(p.reuse?2:0)|((p.required&~p.ready)?4:0);add(field(transition_joint_0+mask));}
 bool partitions()const {
  uint64_t probes=total[probe_skipped];for(unsigned i=raw_hit;i<=raw_unknown;++i)probes+=total[i];
  if(total[observed_heads]!=probes)return false;
  if(total[raw_reservation_fail]!=total[pressure_T_disabled]+total[pressure_T_enabled])return false;
  if(total[pressure_T_enabled]!=total[pressure_no_live_ro]+total[pressure_live_ro_no_eligible]+total[pressure_eligible])return false;
  if(total[ways_examined]!=total[way_no_live_ro]+total[way_not_attached]+total[way_reuse_seen]+total[way_no_missing]+total[way_eligible])return false;
  if(total[raw_reservation_fail]!=total[pressure_prod_probe_reached]+total[pressure_prod_probe_not_reached])return false;
  if(total[pressure_prod_probe_reached]!=total[pressure_rewrite_miss]+total[pressure_rewrite_hit_reserved]+total[pressure_kept_fail]+total[pressure_relief_not_consulted]+total[pressure_intermediate_other])return false;
  if(total[relieve_calls]!=total[relieve_T_disabled]+total[relieve_no_eligible]+total[relieve_eligible_selected])return false;
  if(total[detach_calls]!=total[detach_success]+total[detach_false]+total[detach_exception])return false;
  if(total[ft_access_attempts]!=total[commit_accepted]+total[commit_denied]+total[commit_exception])return false;
  if(total[detach_success]<total[detached_retired])return false;
  if(total[ro_capacity_population]!=total[ro_capacity_consulted]+total[ro_capacity_not_consulted])return false;
  for(unsigned f=ro_descriptor_checked;f<=ro_line_slots_checked;f+=4){if(total[f]!=total[f+1]+total[f+2]||total[ro_capacity_population]!=total[f]+total[f+3])return false;}
  if(total[pressure_eligible]!=total[eligible_frontend_admitted]+total[eligible_frontend_denied])return false;
  if(total[pressure_eligible]!=total[eligible_prod_probe_reached]+total[eligible_prod_probe_not_reached])return false;
  if(total[eligible_prod_probe_reached]!=total[eligible_rewrite_miss]+total[eligible_rewrite_hit_reserved]+total[eligible_kept_fail]+total[eligible_relief_not_consulted]+total[eligible_intermediate_other])return false;
  if(total[pressure_eligible]!=total[eligible_capacity_consulted]+total[eligible_capacity_not_consulted])return false;
  uint64_t h=0;for(unsigned i=eligible_hist_0;i<=eligible_hist_16;++i)h+=total[i];if(h!=total[pressure_T_enabled])return false;
  h=0;for(unsigned i=predicate_hist_0;i<=predicate_hist_7;++i)h+=total[i];if(h!=total[ways_examined]-total[way_no_live_ro])return false;
  h=0;for(unsigned i=transition_joint_0;i<=transition_joint_7;++i)h+=total[i];if(h!=total[ro_admits]+total[ro_merges]+total[ro_fill_packets])return false;
  h=0;for(unsigned i=primary_owner;i<=primary_none;++i)h+=total[i];if(h!=total[observed_heads]-total[probe_skipped])return false;
  for(unsigned f=0;f<FIELD_COUNT;++f){uint64_t n=0;for(unsigned p=0;p<PHASES;++p)n+=phases[p][f];if(total[f]!=n)return false;}
  return !overflow && !total[closure_errors] && !total[selected_identity_mismatch] && !total[commit_exception] && !total[detach_exception];
 }
};
struct sample {uint64_t cycle=0,uid=0,payload=0,generation=0;unsigned slice=0,set=0,eligible=0,first=0,required=0,issued=0,ready=0;bool reuse=false,attached=false,candidate=false;int immediate_result=-1;};
struct registry {
 bool enabled=false,configured=false,output_invalid=false,output_marker=false;uint64_t output_bytes=0,sample_count=0,printed_samples=0;
 std::array<ledger,MAX_SLICES> slices{};std::array<sample,MAX_SAMPLES> examples{};
 ledger *find(const void *p){if(!enabled)return nullptr;for(auto &d:slices)if(d.owner==p)return &d;return nullptr;}
 void add(const void *p){if(!configured){const char*v=std::getenv("EPL2_DETACH_DIAG");if(v&&std::strcmp(v,"0")&&std::strcmp(v,"1"))throw std::invalid_argument("EPL2_DETACH_DIAG expects0or1");enabled=v&&!std::strcmp(v,"1");configured=true;}if(!enabled)return;for(auto &d:slices)if(!d.owner){d=ledger();d.owner=p;return;}throw std::runtime_error("diagnostic64slice bound exceeded");}
 void erase(const void *p){if(auto*d=find(p))d->owner=nullptr;}
 void sample_pressure(ledger &d,const preview &p,uint64_t cycle,uint64_t uid){d.add(samples_seen);if(sample_count==MAX_SAMPLES){d.add(samples_dropped);return;}d.last_sample=sample_count;auto &s=examples[sample_count++];s.cycle=cycle;s.uid=uid;s.slice=d.slice;s.set=p.set;s.eligible=p.eligible;s.first=p.first;s.payload=p.victim_payload;s.generation=p.victim_generation;s.required=p.victim.required;s.issued=p.victim.issued;s.ready=p.victim.ready;s.reuse=p.victim.reuse;s.attached=p.victim.attached;s.candidate=p.candidate;d.add(samples_stored);}
 void print_counts(FILE *f,const char*record,unsigned slice,const ledger&d,const std::array<uint64_t,FIELD_COUNT>&counts,int phase=-1,uint64_t cycle=0) {
  if(output_bytes>64ULL*1024*1024){output_invalid=true;if(!output_marker){std::fprintf(f,"EPL2DETACH_DIAG_V1|{\"record\":\"OUTPUT_INVALID\",\"output_invalid\":true,\"reason\":\"64MiB summary ceiling\"}\n");output_marker=true;}return;}
  int bytes=std::fprintf(f,"EPL2DETACH_DIAG_V1|{\"record\":\"%s\",\"slice\":%u,\"cycle\":%llu,\"phase\":%d,\"kernel_scope\":\"%s\",\"kernel_uid\":",record,slice,(unsigned long long)cycle,phase,phase<0?"SLICE_CUMULATIVE":phase==0?"UNASSIGNED":phase==1?"OVERLAP_UNKNOWN":"SINGLE_ACTIVE_INTERVAL");
  if(phase>=2)bytes+=std::fprintf(f,"%llu",(unsigned long long)d.kernel_uid[phase-2]);else bytes+=std::fprintf(f,"null");
  bytes+=std::fprintf(f,",\"candidate_owner_kernel\":null,\"trigger_later_linkage\":\"NOT_MEASURED\",\"overflow_invalid\":%s,\"output_invalid\":%s,\"closure_ok\":%s,\"epoch_closed\":%s,\"T\":%s,\"M\":%s,\"ro_runtime\":%s,\"live_detached\":%llu,\"counters\":{",d.overflow?"true":"false",output_invalid?"true":"false",d.partitions()?"true":"false",d.closed?"true":"false",d.T?"true":"false",d.M?"true":"false",d.ro_runtime?"true":"false",(unsigned long long)(d.total[detach_success]-d.total[detached_retired]));
  for(unsigned i=0;i<FIELD_COUNT;++i)bytes+=std::fprintf(f,"%s\"%s\":%llu",i?",":"",names[i],(unsigned long long)counts[i]);
  bytes+=std::fprintf(f,"}}\n");if(bytes<0)output_invalid=true;else output_bytes+=unsigned(bytes);
 }
 void print(FILE*f,ledger&d,uint64_t cycle){print_counts(f,"SLICE",d.slice,d,d.total,-1,cycle);for(unsigned p=0;p<d.kernel_count+2;++p)print_counts(f,"PHASE",d.slice,d,d.phases[p],p,cycle);
  unsigned max=0;for(auto&s:slices)if(s.owner&&s.slice!=~0U)max=std::max(max,s.slice);
  if(d.slice==max){ledger sum;sum.closed=true;sum.T=d.T;sum.M=d.M;sum.ro_runtime=d.ro_runtime;unsigned n=0;for(auto&s:slices)if(s.owner){++n;sum.closed &=s.closed;sum.overflow|=s.overflow;for(unsigned f=0;f<FIELD_COUNT;++f)ledger::checked(sum.total[f],s.total[f],sum.overflow);}sum.phases[0]=sum.total;print_counts(f,"GLOBAL",n,sum,sum.total,-1,cycle);for(uint64_t i=printed_samples;i<sample_count;++i){auto&s=examples[i];int bytes=std::fprintf(f,"EPL2DETACH_DIAG_V1|{\"record\":\"SAMPLE\",\"slice\":%u,\"cycle\":%llu,\"request_uid\":%llu,\"set\":%u,\"eligible\":%u,\"first_eligible_way\":%u,\"victim_payload\":%llu,\"victim_generation\":%llu,\"required\":%u,\"issued\":%u,\"ready\":%u,\"reuse_seen\":%s,\"attached\":%s,\"requester_ro_candidate\":%s,\"immediate_commit_result\":%d,\"candidate_owner_kernel\":null}\n",s.slice,(unsigned long long)s.cycle,(unsigned long long)s.uid,s.set,s.eligible,s.first,(unsigned long long)s.payload,(unsigned long long)s.generation,s.required,s.issued,s.ready,s.reuse?"true":"false",s.attached?"true":"false",s.candidate?"true":"false",s.immediate_result);if(bytes>0)output_bytes+=unsigned(bytes);else output_invalid=true;}printed_samples=sample_count;}
 }
};
extern registry state;
inline ledger *get(const void*p){return state.find(p);}
struct detach_scope {ledger*d;bool done=false;explicit detach_scope(ledger*p):d(p){if(d)d->add(detach_calls);}void success(){done=true;if(d)d->add(detach_success);}void refused(field reason){done=true;if(d){d->add(detach_false);d->add(reason);}}~detach_scope(){if(d&&!done)d->add(detach_exception);}};
struct commit_scope {ledger*d;uint64_t before;bool done=false;uint64_t uid,cycle;explicit commit_scope(const void*p,uint64_t u,uint64_t c):d(get(p)),before(d?d->total[detach_success]:0),uid(u),cycle(c){if(d)d->add(ft_access_attempts);}int finish(int result,bool accepted,field gate){done=true;if(d){if(d->last_cycle==cycle&&d->last_uid==uid&&d->last_sample<state.sample_count)state.examples[d->last_sample].immediate_result=result;d->add(accepted?commit_accepted:commit_denied);if(!accepted)d->add(gate);if(d->total[detach_success]>before)d->add(accepted?trigger_accepted_after_detach:trigger_denied_after_detach);}return result;}~commit_scope(){if(d&&!done){d->add(commit_exception);if(d->total[detach_success]>before)d->add(trigger_exception_after_detach);}}};
}
#endif
