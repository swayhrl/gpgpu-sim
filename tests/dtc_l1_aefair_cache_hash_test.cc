#include "gpu-cache.h"
#include "dtc-l1-tag-index.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

static l1d_cache_config make_config(char* text, unsigned banks=4) {
  l1d_cache_config c;
  c.l1_banks=banks; c.l1_banks_byte_interleaving=32;
  c.l1_banks_hashing_function=0; c.m_unified_cache_size=96;
  c.init(text,FuncCachePreferNone); return c;
}
static void rejection(unsigned which) {
  char sector[]="S:32:128:24,L:T:m:L:B,A:512:8,16:0,32";
  char badsets[]="N:30:128:24,L:T:m:L:B,A:512:8,16:0,32";
  char badline[]="N:32:64:24,L:T:m:L:B,A:512:8,16:0,32";
  char good[]="N:32:128:24,L:T:m:L:B,A:512:8,16:0,32";
  if(which==0)make_config(sector);
  if(which==1)make_config(badsets);
  if(which==2)make_config(badline);
  if(which==3)make_config(good,2);
}
int main(int argc,char**argv) {
  const bool linear_only=argc>1 && !std::strcmp(argv[1],"--linear-only");
  char lintext[]="N:32:128:24,L:T:m:L:L,A:512:8,16:0,32";
  char hashtext[]="N:32:128:24,L:T:m:L:B,A:512:8,16:0,32";
  auto lin=make_config(lintext);
  assert(lin.get_nset()==32 && lin.get_num_lines()/lin.get_nset()==24 && lin.get_total_size_inKB()==96);
  assert(lin.get_mshr_entries()==512 && lin.get_line_sz()==128);
  uint64_t digest=1469598103934665603ULL;
  for(uint64_t line=0;line<(1ULL<<18);++line) {
    for(unsigned offset=0;offset<128;offset+=32) {
      const uint64_t a=(line<<7)+offset;
      assert(lin.set_index(a)==line%32 && lin.set_bank(a)==offset/32);
      assert(lin.tag(a)==(line<<7) && lin.block_addr(a)==(line<<7));
      digest=(digest ^ (lin.set_index(a)+37*lin.set_bank(a)+lin.tag(a)))*1099511628211ULL;
    }
  }
  std::printf("LINEAR observations=1048576 digest=%llu geometry=32x24 data=98304 mshr=512\n",(unsigned long long)digest);
  if(linear_only)return 0;
  auto hashed=make_config(hashtext);
  assert(hashed.get_nset()==lin.get_nset() && hashed.get_num_lines()==lin.get_num_lines());
  assert(hashed.get_total_size_inKB()==lin.get_total_size_inKB());
  assert(hashed.get_mshr_entries()==lin.get_mshr_entries());
  unsigned moved=0,collisions_separated=0;
  for(uint64_t line=0;line<(1ULL<<18);++line) {
    for(unsigned offset=0;offset<128;offset+=32) {
      const uint64_t a=(line<<7)+offset;
      const unsigned expected=4*(((line/4)^(line/32))%8)+line%4;
      const unsigned actual=hashed.set_index(a);
      assert(actual==expected && actual<32 && actual%4==lin.set_index(a)%4);
      assert(hashed.set_bank(a)==lin.set_bank(a));
      assert(hashed.tag(a)==lin.tag(a) && hashed.block_addr(a)==lin.block_addr(a));
      moved+=(actual!=lin.set_index(a));
      assert(dtc_l1::logical_tag_set(a,64,true)==4*(((line/4)^(line/64))%16)+line%4);
      assert(dtc_l1::logical_tag_set(a,64,false)==line%64);
    }
    if(line<32) {
      const uint64_t b=(line+32)<<7;
      assert(lin.set_index(line<<7)==lin.set_index(b));
      assert(hashed.tag(line<<7)!=hashed.tag(b));
      collisions_separated+=(hashed.set_index(line<<7)!=hashed.set_index(b));
    }
  }
  assert(moved>0 && collisions_separated==32);
  for(unsigned which=0;which<4;++which) {
    const pid_t child=fork();assert(child>=0);
    if(child==0){signal(SIGABRT,SIG_DFL);rejection(which);_exit(90);}
    int s=0;assert(waitpid(child,&s,0)==child);assert(WIFSIGNALED(s)&&WTERMSIG(s)==SIGABRT);
  }
  std::printf("BANKROW observations=1048576 moved=%u separated=%u negative_assertions=4 PASS\n",moved,collisions_separated);
}
