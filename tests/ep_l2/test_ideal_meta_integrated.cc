#include "ideal_test_fixture.h"
#include <set>

int main(int argc, char **argv) {
  assert(argc == 3);
  Fixture f(argv[1],argv[2]);
  std::set<mem_fetch *> expected;
  // 200 lines x 4 sectors x 2 independent requesters exceed both original
  // Line-MSHR128 and descriptor512 while preserving real sector lower reads.
  for (unsigned line=0; line<200; ++line) for (unsigned s=0;s<4;++s)
    for (unsigned copy=0;copy<2;++copy) {
      auto mf=f.make(0x100000+line*128+s*32);expected.insert(mf);f.push(mf);
    }
  for(unsigned n=0;n<40;++n) {
    auto mf=f.make(0x100000);expected.insert(mf);f.push(mf);
  }
  unsigned peak_lines=0,peak_descriptors=0;
  for(unsigned i=0;i<12000;++i) {
    f.cycle();
    peak_lines=std::max(peak_lines,f.sp->ep_l2_line_mshr_entries());
    peak_descriptors=std::max(peak_descriptors,f.sp->ep_l2_descriptor_count_used());
    if(f.sp->ep_l2_descriptor_count_used()==1640)break;
  }
  assert(peak_lines>128 && peak_descriptors>512);
  assert(f.sp->ep_l2_descriptor_count_used()==1640);
  assert(f.sp->ep_l2_line_mshr_entries()==200);
  f.part->l2_char_release_dram_issue_hold();
  std::set<mem_fetch *> received;
  for(unsigned i=0;i<100000 && f.part->busy();++i) {
    f.cycle();
    while(auto mf=f.sp->pop()) {
      assert(expected.count(mf) && received.insert(mf).second);delete mf;
    }
  }
  assert(received==expected);
  assert(f.sp->ep_l2_lower_read_issue_count()==800);
  assert(f.sp->ep_l2_descriptor_count_used()==0);
  assert(!f.part->busy() && f.sp->l2_char_no_resource_leak());
  f.sp->print_ep_l2_b0_snapshot(stdout,static_cast<unsigned long long>(-1));
  std::printf("Lane4 I_META production 1640-request/200-line/800-sector/per-address/backpressure: PASS; peak_lines=%u peak_descriptors=%u\n",peak_lines,peak_descriptors);
}
