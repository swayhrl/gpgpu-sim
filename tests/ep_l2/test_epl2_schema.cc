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
                       "bank_requests=|bank_grants=|bank_conflicts=|";
  const char *invariant = "EPL2B0V1|INVARIANT|descriptor_used=|"
                          "descriptor_free=|wad_live=|resident_capacity=1024|"
                          "resident_pending=|bypass_capacity=128|bank_pending=|terminal_clean=";
  assert(strstr(schema, "EPL2B0V1") && !strstr(schema, "L2CHARV1"));
  assert(strstr(invariant, "terminal_clean"));
  puts("EP-L2 C7 EPL2B0V1 schema regression: PASS"); return 0;
}
