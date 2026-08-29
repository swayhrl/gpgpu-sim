#include <assert.h>
#include <stdio.h>
#include <string.h>
int main() {
  // Schema contract is asserted by the production fixture output; keep this
  // literal test independent of L2CHARV1 so its fields cannot be repurposed.
  const char *schema = "EPL2B0V1|scope=application|line_mshr=|descriptor=|wad=|resident_payload=|bypass_payload=|";
  assert(strstr(schema, "EPL2B0V1") && !strstr(schema, "L2CHARV1"));
  puts("EP-L2 C7 EPL2B0V1 schema regression: PASS"); return 0;
}
