#define main original_capacity_control_main
#include "test_tag_capacity_geometry.cc"
#undef main
int main(int argc,char**argv){
 assert(argc==14);const std::string mode=argv[13];const bool write=mode=="WRITE",guard=mode=="REUSE"||mode=="T0"||mode=="MSHR_FULL";
 capacity_fixture f(argv,geometry(32,8));auto a=f.addresses(9);f.fill_pending(a);
 if(mode=="REUSE"){for(unsigned n=0;n<8;++n){f.send(f.make(a[n],false));f.await_frontend();}}
 if(mode=="MSHR_FULL"){
  unsigned made=0;for(unsigned set=1;set<32 && made<128;++set){auto others=f.addresses(5,false,set);for(auto x:others){if(made==128)break;f.send(f.make(x,false));f.await_lower(9+made);++made;}}
  assert(f.cache->mshr_entries_used()==128);
 }
 const auto addr=f.addresses(1,false)[0];auto request=f.make(addr,write);if(write)f.memory[addr][0]=f.expected.at(request->get_request_uid())[0];
 auto*d=ep_l2_detach_diag::get(f.cache);auto old=f.tag_for(a[0]);const auto grants=f.cache->ep_l2_payload_bank_grants(),attempts=f.cache->ep_l2_payload_bank_attempts();const auto descriptors=f.cache->ep_l2_descriptor_count_used();
 std::vector<unsigned char> ledger;if(d){ledger.resize(sizeof(*d));std::memcpy(ledger.data(),d,sizeof(*d));}
 for(unsigned n=0;n<32;++n){ep_l2_detach_diag::preview o;f.cache->detach_diag_snapshot(request,o);l2_access_plan p;f.cache->preview_access(addr,request,p,&o);
  assert(o.production_reached && o.production_raw==RESERVATION_FAIL && !o.consulted);
  assert(o.intermediate==RESERVATION_FAIL && p.probe_status==RESERVATION_FAIL);
  if(d)assert(o.raw==RESERVATION_FAIL);
  assert(f.tag_for(a[0])==old && f.cache->ep_l2_payload_bank_grants()==grants && f.cache->ep_l2_payload_bank_attempts()==attempts && f.cache->ep_l2_descriptor_count_used()==descriptors);
 }
 if(d)assert(!std::memcmp(ledger.data(),d,sizeof(*d)));
 f.send(request);f.ticks(64);
 if(guard){assert(f.sp->ep_l2_icntl2_occupancy()==1 && f.pending_reads.size()==(mode=="MSHR_FULL"?136:8));}
 else{assert(f.sp->ep_l2_icntl2_occupancy()==0 && f.tag_for(addr).valid() && f.pending_reads.size()==(write?8:9));}
 if(d){
  using namespace ep_l2_detach_diag;assert(d->partitions() && !d->total[closure_errors] && !d->total[selected_identity_mismatch]);
  assert(d->total[raw_reservation_fail]>0);
  if(guard){assert(d->total[detach_success]==0 && d->total[relieve_calls]==0);
   if(mode=="MSHR_FULL")assert(d->total[pressure_rewrite_miss]>0 && d->total[rewritten_final_denied]>0);
   else assert(d->total[pressure_kept_fail]>0 && d->total[pressure_rewrite_miss]==0);
  }else{assert(d->total[pressure_rewrite_miss]>=1 && d->total[eligible_rewrite_miss]>=1);
   assert(d->total[raw_tag_fail_commit]==1 && d->total[relieve_calls]==1 && d->total[relieve_eligible_selected]==1 && d->total[detach_success]==1 && d->total[trigger_accepted_after_detach]==1);
  }
  printf("DIAG_CONTRACT_V2|case=%s|raw_RF=%llu|virtual_miss=%llu|kept_fail=%llu|not_consulted=%llu|commit_RF=%llu|relieve=%llu|detach=%llu|after_detach_accept=%llu|closure_errors=%llu\n",mode.c_str(),(unsigned long long)d->total[raw_reservation_fail],(unsigned long long)d->total[pressure_rewrite_miss],(unsigned long long)d->total[pressure_kept_fail],(unsigned long long)d->total[pressure_relief_not_consulted],(unsigned long long)d->total[raw_tag_fail_commit],(unsigned long long)d->total[relieve_calls],(unsigned long long)d->total[detach_success],(unsigned long long)d->total[trigger_accepted_after_detach],(unsigned long long)d->total[closure_errors]);
 }
 f.finish_named(mode.c_str());
}
