// Actual GPU-wide APIs, including the native/OFF no-op path.
#include <cassert>
#include <cstdio>
#include "../../libcuda/gpgpu_context.h"
#include "../../src/gpgpu-sim/gpu-sim.h"
#include "../../src/gpgpu-sim/icnt_wrapper.h"
#include "../../src/option_parser.h"
int main(int argc,char **argv) {
  assert(argc==7);
  gpgpu_context ctx;gpgpu_sim_config cfg(&ctx);
  option_parser_t p=option_parser_create();
  ctx.ptx_reg_options(p);ctx.func_sim->ptx_opcocde_latency_options(p);
  icnt_reg_options(p);cfg.reg_options(p);
  option_parser_cfgfile(p,argv[1]);option_parser_cfgfile(p,argv[2]);option_parser_cfgfile(p,argv[3]);
  const char *a[]={"ro-epoch","-gpgpu_ep_l2_ft_mode","2",
    "-gpgpu_ep_l2_wbuf_entries","16","-gpgpu_ep_l2_ro_tag_detach",argv[5],
    "-gpgpu_ep_l2_ro_line_mshr_bypass",argv[6],"-gpgpu_perf_sim_memcpy","1"};
  option_parser_cmdline(p,11,a);cfg.init();option_parser_destroy(p);
  exec_gpgpu_sim gpu(cfg,&ctx);
  const bool ro_on=std::stoul(argv[5])||std::stoul(argv[6]);
  assert(gpu.ro_enabled()==ro_on);
  if(ro_on) {
    bool rejected=false;try {gpu.ro_begin_epoch();}catch(const std::logic_error&){rejected=true;}
    assert(rejected);
  }
  gpu.ro_bind_commandlist(argv[4]);
  // Real host prefix uses actual complete addresses/sector masks and all slices.
  gpu.perf_memcpy_to_gpu(0,128);
  gpu.ro_begin_epoch();
  if(ro_on) {
    bool rejected=false;try {gpu.ro_begin_epoch();}catch(const std::logic_error&){rejected=true;}
    assert(rejected);
  }
  gpu.ro_end_epoch();
  // Closed whole epoch may accept later clean/quiescent initialization.
  gpu.perf_memcpy_to_gpu(0,128);
  puts("RO_GPU_WHOLE_EPOCH_BIND_PREFIX_BEGIN_DRAIN_END_NATIVE_PASS");
}
