// Bounded production-path observer controls. No complete application workload.
#include "potential_fixture.h"
#include <cstring>
#include <limits>
static uint64_t witness=1469598103934665603ULL;
static void mix(uint64_t n){witness^=n;witness*=1099511628211ULL;}
static void diag_test_witness(unsigned kind,unsigned long long cycle,mem_fetch*m){mix(kind);mix(cycle);mix(m->get_request_uid());mix(m->get_addr());mix(m->get_access_type());mix(m->get_access_sector_mask().to_ulong());mix(m->get_ep_l2_payload_id());mix(m->get_ep_l2_payload_generation());if(m->has_ft_wb_data())for(auto v:m->get_ft_wb_data())mix(v);}
#define DIAG_TEST_WITNESS_ACTIVE 1
#include "ms_ro_combined/native_fixture.h"
#ifndef EP_L2_DIAG_REFERENCE
#include "../../src/gpgpu-sim/ep-l2-detach-diag.h"
#endif
static const uint64_t stride=8192;
static void fill(combined_fixture&f,unsigned n,bool ro=true,unsigned set=0){for(unsigned i=0;i<n;++i){f.send(f.make((ro?0:0x800000)+((ro&&n==16&&set==0)?(i+1)%16:i)*stride+set*128,false));f.await_lower(i+1);}}
static void tick(combined_fixture&f,unsigned n=16,bool consume=true){while(n--)f.step(consume);}
static void signature(combined_fixture&f){
 for(unsigned i=0;i<1024;++i){auto h=f.cache->ep_l2_tag_payload_handle_for_test(i);mix(i);mix(h.payload_id);mix(h.generation);if(f.cache->ep_l2_payload_for_test().handle_live(h)){const auto&s=f.cache->ep_l2_payload_for_test().inspect(h);mix(s.owner);mix(s.pins);mix(s.ready_sector_mask.to_ulong());mix(s.pending_sector_mask.to_ulong());for(auto v:s.versions)mix(v);}}
 printf("EPL2MS_TEST|SEMANTIC|cycles=%llu|digest=%llu|replies=%zu|lower=%llu|bytes=%llu|bank_grants=%llu|bank_conflicts=%llu|bank_attempts=%llu|wad=%u|descriptors=%u|drain=%u\n",f.gpu->gpu_sim_cycle+f.gpu->gpu_tot_sim_cycle,(unsigned long long)witness,f.replies.size(),f.lower_read_packets,f.lower_read_bytes,f.cache->ep_l2_payload_bank_grants(),f.cache->ep_l2_payload_bank_conflicts(),f.cache->ep_l2_payload_bank_attempts(),f.cache->ep_l2_wad_occupancy(),f.cache->ep_l2_descriptor_count_used(),unsigned(f.cache->ft_drained()));
}
#ifndef EP_L2_DIAG_REFERENCE
static ep_l2_detach_diag::ledger* ledger(combined_fixture&f){return ep_l2_detach_diag::get(f.cache);}
static void require_observer(combined_fixture&f,ep_l2_detach_diag::field key,uint64_t min){if(auto*d=ledger(f)){assert(d->total[key]>=min);assert(d->partitions());}}
#endif
int main(int argc,char**argv){assert(argc==10);const char*which=argv[9];
 const bool no_runtime=!std::strcmp(which,"T_OFF");
 const bool ready=!std::strcmp(which,"PRESSURE_NO_MISSING")||!std::strcmp(which,"RESPONSE_BACKPRESSURE");
 combined_fixture f(argv,!no_runtime,ready?"1024:16:64:1":"1024:16:64:64");
 if(!std::strcmp(which,"T_OFF")||!std::strcmp(which,"EMPTY_CERT")){fill(f,16,false);f.send(f.make(0x800000+16*stride,false));tick(f);assert(f.pending_reads.size()==16);}
 else if(!std::strcmp(which,"NO_PRESSURE_POSITIVE_RO")||!std::strcmp(which,"OBSERVER_NONINTERFERENCE")){f.read(0);f.read(32);f.read(0);}
 else if(!std::strcmp(which,"PRESSURE_NO_LIVE_RO")||!std::strcmp(which,"PREVIEW_BLOCKS_COMMIT")){fill(f,16,false);f.send(f.make(0,false));tick(f);assert(f.pending_reads.size()==16);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::pressure_no_live_ro,1);
#endif
 }
 else if(!std::strcmp(which,"PRESSURE_ONLY_OTHER_SET_RO")){fill(f,16,false);f.send(f.make(128,false));f.await_lower(17);f.send(f.make(0,false));tick(f);assert(f.pending_reads.size()==17);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::pressure_no_live_ro,1);
#endif
 }
 else if(!std::strcmp(which,"PRESSURE_STICKY_REUSE")){fill(f,16);for(unsigned i=0;i<16;++i){f.send(f.make(i*stride+32,false));f.await_lower(17+i);}for(unsigned i=f.pending_reads.size();i-->0;)if(f.pending_reads[i]->get_addr()%128==32)f.return_one(i);tick(f,128);assert(f.replies.size()==16);f.send(f.make(16*stride,false));tick(f);assert(f.pending_reads.size()==16);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::reuse_seen_first_transition,16);require_observer(f,ep_l2_detach_diag::way_reuse_seen,1);assert(!ledger(f)||ledger(f)->total[ep_l2_detach_diag::detach_success]==0);
#endif
 }
 else if(ready){fill(f,2);f.return_one(0);tick(f,32,false);assert(f.sp->ep_l2_l2icnt_occupancy()==1);auto h=f.tag_for(stride);f.return_one(0);tick(f,32,false);assert(f.cache->ep_l2_payload_for_test().inspect(h).ready_sector_mask.test(0));assert(f.cache->ep_l2_payload_for_test().inspect(h).pins==1);
#ifndef EP_L2_DIAG_REFERENCE
 if(auto*d=ledger(f)){auto p=f.cache->detach_diag_predicate_for_test(h);assert(p.attached&&!p.reuse&&(p.required&~p.ready)==0);assert(!f.cache->detach_diag_can_detach_for_test(h));}
#endif
 puts("CONTROL_SCOPE|ready_predicate=REAL_READY_PINNED_TRANSACTION|raw_all_reserved_pressure=NOT_ASSERTED|reason=ready_Tag_can_be_replacement_candidate_but_pinned_commit_stalls");
 }
 else if(!std::strcmp(which,"PRESSURE_ELIGIBLE")){fill(f,16);auto h=f.tag_for(0);f.send(f.make(16*stride,false));f.await_lower(17);assert(!f.tag_for(0).valid()&&f.cache->ep_l2_payload_for_test().handle_live(h));f.send(f.make(0,false));f.await_lower(18);auto n=f.tag_for(0);assert(n.valid()&&n!=h);for(unsigned i=0;i<f.pending_reads.size();++i)if(f.pending_reads[i]->get_ep_l2_payload_id()==h.payload_id){f.return_one(i);break;}tick(f,32);assert(f.tag_for(0)==n);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::pressure_rewrite_miss,1);require_observer(f,ep_l2_detach_diag::detach_success,1);
#endif
 }
 else if(!std::strcmp(which,"NATIVE_REQUESTER_RO_VICTIM")){fill(f,16);auto mf=f.make(0x800000,false);f.send(mf);tick(f);assert(f.pending_reads.size()==16);
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::pressure_relief_not_consulted,1);require_observer(f,ep_l2_detach_diag::pressure_eligible,1);
#endif
 auto direct=f.make(0x800000+stride,false);std::list<cache_event>events;auto s=f.cache->access(direct->get_addr(),direct,f.gpu->gpu_sim_cycle,events);assert(s==RESERVATION_FAIL);assert(!f.tag_for(0).valid());f.send(direct);puts("CONTROL_SCOPE|native_frontend=DENIED_NOT_CONSULTED|direct_access=REAL_RELIEVE_THEN_DIRECTORY_DENIAL");}
 else if(!std::strcmp(which,"PREVIEW_REPEAT")){fill(f,16);auto mf=f.make(16*stride,false);f.send(mf);tick(f,1);
#ifndef EP_L2_DIAG_REFERENCE
 auto*d=ledger(f);std::array<uint64_t,ep_l2_detach_diag::FIELD_COUNT>before{};if(d)before=d->total;
#endif
 for(unsigned i=0;i<32;++i){l2_access_plan p;f.cache->preview_access(mf->get_addr(),mf,p);
#ifndef EP_L2_DIAG_REFERENCE
 ep_l2_detach_diag::preview o;f.cache->detach_diag_snapshot(mf,o);f.cache->preview_access(mf->get_addr(),mf,p,&o);
#endif
 }
#ifndef EP_L2_DIAG_REFERENCE
 if(d)assert(d->total==before);
#endif
 f.await_lower(17);}
 else if(!std::strcmp(which,"DETACH_THEN_LATER_DENIAL")){fill(f,16);for(unsigned i=0;i<496;++i){auto a=((i%63)+1)*128+(i/63)*stride;f.send(f.make(a,false));f.await_lower(17+i);}assert(f.cache->ep_l2_descriptor_count_used()==512);auto mf=f.make(17*stride,false);f.send(mf);tick(f,16);auto h=f.tag_for(0);std::list<cache_event>events;auto s=f.cache->access(mf->get_addr(),mf,f.gpu->gpu_sim_cycle,events);assert(s==RESERVATION_FAIL&&!f.tag_for(0).valid()&&f.cache->ep_l2_payload_for_test().handle_live(h));
#ifndef EP_L2_DIAG_REFERENCE
 require_observer(f,ep_l2_detach_diag::trigger_denied_after_detach,1);require_observer(f,ep_l2_detach_diag::denied_capacity,1);
#endif
 }
 else if(!std::strcmp(which,"ROLLUP_RESIDUAL")){
#ifndef EP_L2_DIAG_REFERENCE
 f.cache->detach_diag_kernel(true,11);
#endif
 f.read(0);
#ifndef EP_L2_DIAG_REFERENCE
 f.cache->detach_diag_kernel(true,12);
#endif
 f.read(128);
#ifndef EP_L2_DIAG_REFERENCE
 f.cache->detach_diag_kernel(false,11);f.cache->detach_diag_kernel(false,12);
#endif
 f.read(256);
 }
 else if(!std::strcmp(which,"BOUNDED_OUTPUT")){
#ifndef EP_L2_DIAG_REFERENCE
 ep_l2_detach_diag::ledger d;d.total[ep_l2_detach_diag::observed_heads]=~uint64_t(0);d.add(ep_l2_detach_diag::observed_heads);assert(d.overflow&&!d.partitions());ep_l2_detach_diag::registry *local=new ep_l2_detach_diag::registry();ep_l2_detach_diag::preview p;for(unsigned i=0;i<100;++i)local->sample_pressure(d,p,i,i);assert(local->sample_count==64&&d.total[ep_l2_detach_diag::samples_dropped]==36);local->output_bytes=67108865;std::string path=std::string("observer_output_cap_")+(std::getenv("EPL2_DETACH_DIAG")?std::getenv("EPL2_DETACH_DIAG"):"0")+".json";FILE*sink=std::fopen(path.c_str(),"wx");assert(sink);local->print_counts(sink,"SLICE",0,d,d.total);assert(local->output_invalid&&local->output_marker);std::fclose(sink);delete local;
#endif
 f.read(0);puts("CONTROL_SCOPE|overflow_and_sample_cap=PASS_OBSERVER_DATA_STRUCTURE_ONLY|model_state_not_seeded=1");}
 else {fprintf(stderr,"unknown control %s\n",which);return 2;}
 f.finish();signature(f);
#ifndef EP_L2_DIAG_REFERENCE
 if(auto*d=ledger(f)){assert(d->partitions());assert(d->total[ep_l2_detach_diag::detach_success]==d->total[ep_l2_detach_diag::detached_retired]);}
#endif
 printf("DETACH_DIAGNOSTIC_CONTROL_PASS|case=%s\n",which);return 0;
}
