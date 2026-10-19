#include <assert.h>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include "../../src/gpgpu-sim/ep-l2-ro-oracle.h"
using ep_l2_ro::oracle_certificate;
static std::string hash() { return std::string(64,'a'); }
int main(int argc,char **argv) {
  std::istringstream in("first_line,last_line_inclusive\n0000000000001000,0000000000001080\n0000000000002000,0000000000002000\n");
  oracle_certificate c(in,hash(),"TRACE_MODEL_CLOSED_WORLD");
  assert(c.contains(0x1000) && c.contains(0x1080) && c.contains(0x2000));
  assert(!c.contains(0x0f80) && !c.contains(0x1100) && !c.contains(0x1001));
  for(const char *s:{"wrong header\n",
      "first_line,last_line_inclusive\n0000000000001001,0000000000001080\n",
      "first_line,last_line_inclusive\n0000000000002000,0000000000001000\n",
      "first_line,last_line_inclusive\n0000000000001000,0000000000001080\n0000000000001080,0000000000002000\n",
      "first_line,last_line_inclusive\n0000000000001000,gggggggggggggggg\n"}) {
    bool rejected=false;std::istringstream bad(s);
    try {oracle_certificate b(bad,hash(),"TRACE_MODEL_CLOSED_WORLD");}
    catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
  }
  std::istringstream empty("first_line,last_line_inclusive\n");
  oracle_certificate e(empty,hash(),"TRACE_MODEL_CLOSED_WORLD");
  assert(!e.contains(0x1000));
  std::istringstream fallback("first_line,last_line_inclusive\n");
  oracle_certificate no_ro(fallback,hash(),"NO_READONLY_CERTIFICATION");
  assert(!no_ro.range_count() && !no_ro.contains(0x1000));
  bool refused=false;std::istringstream forged("first_line,last_line_inclusive\n0000000000001000,0000000000001000\n");
  try{oracle_certificate invalid(forged,hash(),"NO_READONLY_CERTIFICATION");}
  catch(const std::invalid_argument&){refused=true;}assert(refused);
  if(argc==3) {
    std::ifstream actual(argv[1]);
    oracle_certificate a(actual,argv[2],"TRACE_MODEL_CLOSED_WORLD");
    printf("Verified-input C++ range grammar accepted: %u ranges\n",a.range_count());
  }
  puts("EP-L2 Lane 3 runtime oracle range/eligibility tests: PASS");
}
