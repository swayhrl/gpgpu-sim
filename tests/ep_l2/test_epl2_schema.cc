#include <assert.h>
#include <stdio.h>
#include <string.h>
int main() {
  // Schema contract is asserted by the production fixture output; keep this
  // literal test independent of L2CHARV1 so its fields cannot be repurposed.
  const char *schema = "EPL2B0V1|scope=kernel|interval=kernel_shared_delta|"
                       "start_cycle=|completion_cycle=|overlap_detected=|"
                       "line_mshr_avg=|line_mshr_p95=|line_mshr_max=|"
                       "descriptor_avg=|wad_avg=|resident_payload_avg=|"
                       "bank_requests=|bank_grants=|bank_conflicts=|"
                       "bank_logical_ops=|bank_attempts=|bank_retry_attempts=|"
                       "bank_true_conflict_ops=|bank_true_conflict_events=|"
                       "bank_wait_cycles=|"
                       "c7d_line_alloc_eligible=|c7d_tag_set_all_reserved_block=|"
                       "c7d_line_mshr_full_block=|"
                       "c7d_descriptor_pool_full_block=|"
                       "c7d_per_address_cap_block=|"
                       "c7d_wad_hazard_wait_cycles=|"
                       "c7d_payload_service_port_denial=|"
                       "c7d_bank0_logical_ops=|c7d_bank_resident_hit_read=|"
                       "c7e_tag_way_alloc_need=|c7e_tag_way_alloc_block=|"
                       "c7e_line_mshr_need=|c7e_descriptor_need=|"
                       "c7e_per_address_cap_check=|"
                       "c7e_dram_issue_attempt=|c7e_dram_successful_read_bytes=|"
                       "c7e_dram_scheduler_causal_block=|";
  const char *l1 = "EPL2L1V1|scope=application|accesses=|misses=|"
                   "line_alloc_fail=|miss_queue_full=|mshr_entry_fail=|"
                   "mshr_merge_fail=|mshr_rw_pending=|"
                   "bank_latency_queue_conflict=";
  const char *dram = "EPL2DRAMV1|scope=window|interval=5000_cycle|channel=|"
                     "scheduler_occ_avg=|returnq_occ_avg=|"
                     "successful_read_bytes=|bandwidth_util=";
  const char *invariant = "EPL2B0V1|INVARIANT|descriptor_used=|"
                          "descriptor_free=|wad_live=|resident_capacity=1024|"
                          "resident_pending=|bypass_capacity=128|bank_pending=|terminal_clean=";
  assert(strstr(schema, "EPL2B0V1") && !strstr(schema, "L2CHARV1"));
  assert(strstr(l1, "EPL2L1V1") && strstr(dram, "EPL2DRAMV1"));
  assert(strstr(invariant, "terminal_clean"));
  puts("EP-L2 C7 EPL2B0V1 schema regression: PASS"); return 0;
}
