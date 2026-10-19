// Native unit test; not a substitute for real partition observer ON/OFF replay.
#include "../../src/gpgpu-sim/ep-l2-mechanism-observer.h"
#include <string>
int main() {
  namespace M = ep_l2::mechanism_v1;
  M::Observer o; o.enabled = true;
  const auto wb = 1ULL << M::WB_DATA_SPACE;
  const auto bank = 1ULL << M::DATA_SERVICE;
  for (unsigned c = 10; c < 20; ++c) o.observe(7, c, wb | bank, false);
  o.observe(7, 20, bank, false);
  o.observe(7, 21, wb | bank, false);  // recurrence is not another request.
  o.observe(7, 24, 0, true);
  assert(o.waits[M::WB_DATA_SPACE].requests == 1);
  assert(o.waits[M::WB_DATA_SPACE].resolved == 1);
  assert(o.waits[M::WB_DATA_SPACE].sum == 10);
  assert(o.waits[M::DATA_SERVICE].requests == 1);
  assert(o.waits[M::DATA_SERVICE].sum == 14);
  assert(o.any_to_admission.requests == 1 && o.any_to_admission.sum == 14);
  assert(o.head_changes_without_admission == 0);
  o.observe(8, 25, wb, false); o.observe(8, 30, 0, true);
  assert(o.waits[M::WB_DATA_SPACE].requests == 2);
  assert(o.waits[M::WB_DATA_SPACE].histogram[M::Observer::bin(5)] == 1);
  o.observe(9, 40, wb, false);
  o.observe(9, 41, 0, false, 0);  // Masked check is UNKNOWN, not unblock.
  assert(o.waits[M::WB_DATA_SPACE].resolved == 2);
  o.observe(9, 44, 0, true);
  assert(o.waits[M::WB_DATA_SPACE].resolved == 3);
  assert(o.waits[M::WB_DATA_SPACE].unknown_while_unresolved == 1);
  o.sample(16, 18, 512, 1025, 1024, 1);
  assert(o.physical_resident_peak == 1025 && o.bound_tag_peak == 1024);
  o.lower_accept(false, false, 32); o.lower_accept(true, true, 64);
  assert(o.lower_reads == 1 && o.lower_read_bytes == 32 && o.lower_wb_bytes == 64);
  o.enabled = false; o.observe(99, 100, wb, false); o.sample(100, 100, 999, 9999, 9999, 9999);
  assert(o.waits[M::WB_DATA_SPACE].requests == 3 && o.wb_peak == 16);
  // Appended V2 lookup row must not reinterpret published V1 aggregate14.
  o.enabled = true;
  const auto lookup = 1ULL << M::VICTIM_LOOKUP;
  o.observe(10,110,lookup,false,lookup);
  o.observe(10,111,0,false,lookup); // Query finished before native admission.
  o.observe(10,115,0,true);
  assert(o.waits[M::VICTIM_LOOKUP].requests==1 &&
         o.waits[M::VICTIM_LOOKUP].sum==1);
  const auto ro_lookup=1ULL << M::READONLY_LOOKUP;
  o.observe(11,120,ro_lookup,false,ro_lookup);o.observe(11,121,0,true);
  assert(o.waits[M::READONLY_LOOKUP].requests==1 && o.waits[M::READONLY_LOOKUP].sum==1);
  FILE *fp=tmpfile(); assert(fp); o.print(fp,0); rewind(fp);
  char bytes[65536]={}; const auto used=fread(bytes,1,sizeof(bytes)-1,fp);
  assert(used>0 && !ferror(fp)); fclose(fp);
  const std::string text(bytes,used);
  assert(text.find("EPL2MSV1|WAIT|slice=0|reason=14|")!=std::string::npos);
  assert(text.find("EPL2MSV2|WAIT|slice=0|reason=15|")!=std::string::npos);
  assert(text.find("EPL2MSV2|WAIT|slice=0|reason=16|")!=std::string::npos);
  assert(text.find("EPL2MSV1|WAIT|slice=0|reason=15|")==std::string::npos);
  puts("MS_OBSERVER_BOUNDED_IDENTITY_FIRST_WAIT_AND_DISABLED_NATIVE_PASS");
}
