# INFRA1B Trace Postcheck

start_ts=1780677468
end_ts=1780677468
elapsed_sec=0
branch=hrl/infra/paper-repro-framework-trace-v0
HEAD_before_commit=51702e6
base_branch=hrl/infra/paper-repro-framework-v0

## Validation

- git diff --check: pass
- build: pass (GPGPU-Sim version 4.2.0 (build gpgpu-sim_git-commit-51702e6-modified_2.0) configured with AccelWattch.
setup_environment succeeded

	Building GPGPU-Sim version 4.2.0 (build gpgpu-sim_git-commit-51702e6_modified_2.0) with CUDA version 11.8

if [ ! -d lib/gcc-11.4.0/cuda-11080/release ]; then mkdir -p lib/gcc-11.4.0/cuda-11080/release; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim; fi;
Warning: gpgpu-sim is building without opencl support. Make sure NVOPENCL_LIBDIR and NVOPENCL_INCDIR are set
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/decuda_pred_table ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/decuda_pred_table; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libopencl ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libopencl; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libopencl/bin ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libopencl/bin; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/intersim2 ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/intersim2; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuobjdump_to_ptxplus ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuobjdump_to_ptxplus; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch; fi;
if [ ! -d /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/cacti ]; then mkdir -p /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/cacti; fi;
make -C ./src/cuda-sim/ depend
make -C /workspace/tmp/infra1_trace_wt/src/accelwattch/ depend
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src/cuda-sim'
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src/accelwattch'
make[2]: Entering directory '/workspace/tmp/infra1_trace_wt/src/accelwattch'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/ Ucache.cc XML_Parse.cc arbiter.cc area.cc array.cc bank.cc basic_circuit.cc basic_components.cc cacti_interface.cc component.cc core.cc crossbar.cc decoder.cc htree2.cc interconnect.cc io.cc iocontrollers.cc logic.cc main.cc mat.cc memoryctrl.cc noc.cc nuca.cc parameter.cc processor.cc router.cc sharedcache.cc subarray.cc technology.cc uca.cc wire.cc xmlParser.cc gpgpu_sim_wrapper.cc  2> /dev/null
make -C ./cacti/ depend
make[3]: Entering directory '/workspace/tmp/infra1_trace_wt/src/accelwattch/cacti'
make[4]: Entering directory '/workspace/tmp/infra1_trace_wt/src/accelwattch/cacti'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/cacti/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/cacti/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/cacti/ area.cc bank.cc mat.cc main.cc Ucache.cc io.cc technology.cc basic_circuit.cc parameter.cc decoder.cc component.cc uca.cc subarray.cc wire.cc htree2.cc cacti_interface.cc router.cc nuca.cc crossbar.cc arbiter.cc  2> /dev/null
make[4]: Nothing to be done for 'depend'.
make[4]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/accelwattch/cacti'
make[3]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/accelwattch/cacti'
make[2]: Nothing to be done for 'depend'.
make[2]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/accelwattch'
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/accelwattch'
make -C /workspace/tmp/infra1_trace_wt/src/accelwattch/ 
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src/accelwattch'
make[2]: Entering directory '/workspace/tmp/infra1_trace_wt/src/accelwattch'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/ Ucache.cc XML_Parse.cc arbiter.cc area.cc array.cc bank.cc basic_circuit.cc basic_components.cc cacti_interface.cc component.cc core.cc crossbar.cc decoder.cc htree2.cc interconnect.cc io.cc iocontrollers.cc logic.cc main.cc mat.cc memoryctrl.cc noc.cc nuca.cc parameter.cc processor.cc router.cc sharedcache.cc subarray.cc technology.cc uca.cc wire.cc xmlParser.cc gpgpu_sim_wrapper.cc  2> /dev/null
make -C ./cacti/ depend
make[3]: Entering directory '/workspace/tmp/infra1_trace_wt/src/accelwattch/cacti'
make[4]: Entering directory '/workspace/tmp/infra1_trace_wt/src/accelwattch/cacti'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/cacti/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/cacti/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/cacti/ area.cc bank.cc mat.cc main.cc Ucache.cc io.cc technology.cc basic_circuit.cc parameter.cc decoder.cc component.cc uca.cc subarray.cc wire.cc htree2.cc cacti_interface.cc router.cc nuca.cc crossbar.cc arbiter.cc  2> /dev/null
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/Makefile.makedepend
make[4]: Nothing to be done for 'depend'.
make[4]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/accelwattch/cacti'
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ cuda_device_printf.cc cuda_device_runtime.cc cuda-sim.cc instructions.cc memory.cc ptx_ir.cc ptx_loader.cc ptx_parser.cc ptx_sim.cc ptx-stats.cc 2> /dev/null
make[3]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/accelwattch/cacti'
make[2]: Nothing to be done for 'all'.
make[2]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/accelwattch'
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/accelwattch'
make[1]: 'depend' is up to date.
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/cuda-sim'
make -C ./src/cuda-sim/
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src/cuda-sim'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ cuda_device_printf.cc cuda_device_runtime.cc cuda-sim.cc instructions.cc memory.cc ptx_ir.cc ptx_loader.cc ptx_parser.cc ptx_sim.cc ptx-stats.cc 2> /dev/null
g++  -c -O3 -g3 -Wall -Wno-unused-function -Wno-sign-compare -I/usr/local/cuda-11.8/include  -I/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ -I. -I/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release -fPIC  -DTRACING_ON=1 -DCUDART_VERSION=11080 -std=c++0x cuda-sim.cc -o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/cuda-sim.o
ar rcs /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/libgpgpu_ptx_sim.a /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptx.tab.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/lex.ptx_.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptxinfo.tab.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/lex.ptxinfo_.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptx_parser.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptx_loader.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/cuda_device_printf.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/instructions.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/cuda-sim.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptx_ir.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptx_sim.o  /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/memory.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptx-stats.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/decuda_pred_table/decuda_pred_table.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptx.tab.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/lex.ptx_.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/ptxinfo.tab.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/lex.ptxinfo_.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/cuda_device_runtime.o
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/cuda-sim'
make -C ./src/gpgpu-sim/ depend
make -C ./libcuda/ depend
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src/gpgpu-sim'
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/libcuda'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/ cuda_runtime_api.cc 2> /dev/null
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/ addrdec.cc dram.cc dram_sched.cc gpu-cache.cc gpu-misc.cc gpu-sim.cc hashing.cc histogram.cc icnt_wrapper.cc l2cache.cc local_interconnect.cc mem_fetch.cc mem_latency_stat.cc power_interface.cc power_stat.cc scoreboard.cc shader.cc stack.cc stat-tool.cc traffic_breakdown.cc visualizer.cc 2> /dev/null
make[1]: 'depend' is up to date.
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/libcuda'
make -C ./libcuda/
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/libcuda'
make[1]: 'depend' is up to date.
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/gpgpu-sim'
make -C ./src/gpgpu-sim/
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src/gpgpu-sim'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/ cuda_runtime_api.cc 2> /dev/null
:
echo /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuda_runtime_api.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuobjdump_lexer.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuobjdump_parser.o
/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuda_runtime_api.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuobjdump_lexer.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuobjdump_parser.o
ar rcs /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/libcuda.a /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuda_runtime_api.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuobjdump_lexer.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/cuobjdump_parser.o
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/ addrdec.cc dram.cc dram_sched.cc gpu-cache.cc gpu-misc.cc gpu-sim.cc hashing.cc histogram.cc icnt_wrapper.cc l2cache.cc local_interconnect.cc mem_fetch.cc mem_latency_stat.cc power_interface.cc power_stat.cc scoreboard.cc shader.cc stack.cc stat-tool.cc traffic_breakdown.cc visualizer.cc 2> /dev/null
ar rcs  /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/libgpu_uarch_sim.a /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/addrdec.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/dram.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/dram_sched.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/gpu-cache.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/gpu-misc.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/gpu-sim.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/hashing.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/histogram.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/icnt_wrapper.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/l2cache.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/local_interconnect.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/mem_fetch.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/mem_latency_stat.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/power_interface.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/power_stat.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/scoreboard.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/shader.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/stack.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/stat-tool.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/traffic_breakdown.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/visualizer.o
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/libcuda'
make -C ./cuobjdump_to_ptxplus/ depend
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/cuobjdump_to_ptxplus'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuobjdump_to_ptxplus/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuobjdump_to_ptxplus/Makefile.makedepend cuobjdumpInst.cc cuobjdumpInstList.cc cuobjdump_to_ptxplus.cc 2> /dev/null
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/cuobjdump_to_ptxplus'
make -C ./cuobjdump_to_ptxplus/
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/cuobjdump_to_ptxplus'
:
:
:
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/cuobjdump_to_ptxplus'
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/gpgpu-sim'
make "CREATE_LIBRARY=1" "DEBUG=0" -C ./src/intersim2
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src/intersim2'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/intersim2/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/intersim2/Makefile.makedepend -I-I. -Iarbiters -Iallocators -Irouters -Inetworks -Ipower -I/workspace/tmp/infra1_trace_wt/src -I/workspace/tmp/infra1_trace_wt/src/gpgpu-sim/ -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/intersim2/ config_utils.cpp booksim_config.cpp module.cpp buffer.cpp vc.cpp routefunc.cpp traffic.cpp flitchannel.cpp trafficmanager.cpp batchtrafficmanager.cpp packet_reply_info.cpp buffer_state.cpp stats.cpp credit.cpp outputset.cpp flit.cpp injection.cpp misc_utils.cpp rng_wrapper.cpp rng_double_wrapper.cpp power_module.cpp switch_monitor.cpp buffer_monitor.cpp main.cpp gputrafficmanager.cpp intersim_config.cpp interconnect_interface.cpp allocators/allocator.cpp allocators/islip.cpp allocators/loa.cpp allocators/maxsize.cpp allocators/pim.cpp allocators/selalloc.cpp allocators/separable.cpp allocators/separable_input_first.cpp allocators/separable_output_first.cpp allocators/wavefront.cpp arbiters/arbiter.cpp arbiters/matrix_arb.cpp arbiters/prio_arb.cpp arbiters/roundrobin_arb.cpp arbiters/tree_arb.cpp networks/anynet.cpp networks/cmesh.cpp networks/dragonfly.cpp networks/fattree.cpp networks/flatfly_onchip.cpp networks/fly.cpp networks/kncube.cpp networks/network.cpp networks/qtree.cpp networks/tree4.cpp power/buffer_monitor.cpp power/power_module.cpp power/switch_monitor.cpp routers/chaos_router.cpp routers/event_router.cpp routers/iq_router.cpp routers/router.cpp 2> /dev/null
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/intersim2'
make -C ./src/ depend
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/ abstract_hardware_model.cc debug.cc gpgpusim_entrypoint.cc option_parser.cc statwrapper.cc stream_manager.cc trace.cc 2> /dev/null
make[1]: 'depend' is up to date.
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src'
make -C ./src/
make[1]: Entering directory '/workspace/tmp/infra1_trace_wt/src'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/ abstract_hardware_model.cc debug.cc gpgpusim_entrypoint.cc option_parser.cc statwrapper.cc stream_manager.cc trace.cc 2> /dev/null
make   -C ./gpgpu-sim
make[2]: Entering directory '/workspace/tmp/infra1_trace_wt/src'
touch /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/Makefile.makedepend
makedepend -f/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/Makefile.makedepend -p/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/ addrdec.cc dram.cc dram_sched.cc gpu-cache.cc gpu-misc.cc gpu-sim.cc hashing.cc histogram.cc icnt_wrapper.cc l2cache.cc local_interconnect.cc mem_fetch.cc mem_latency_stat.cc power_interface.cc power_stat.cc scoreboard.cc shader.cc stack.cc stat-tool.cc traffic_breakdown.cc visualizer.cc 2> /dev/null
ar rcs  /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/libgpu_uarch_sim.a /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/addrdec.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/dram.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/dram_sched.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/gpu-cache.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/gpu-misc.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/gpu-sim.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/hashing.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/histogram.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/icnt_wrapper.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/l2cache.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/local_interconnect.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/mem_fetch.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/mem_latency_stat.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/power_interface.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/power_stat.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/scoreboard.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/shader.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/stack.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/stat-tool.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/traffic_breakdown.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/visualizer.o
make[2]: Leaving directory '/workspace/tmp/infra1_trace_wt/src/gpgpu-sim'
ar rcs  /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libgpgpusim.a /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/abstract_hardware_model.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/debug.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpusim_entrypoint.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/option_parser.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/statwrapper.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/stream_manager.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/trace.o /workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/*.o
make[1]: Leaving directory '/workspace/tmp/infra1_trace_wt/src'
g++ -shared -Wl,-soname,libcudart.so -Wl,--version-script=linux-so-version.txt\
		/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/libcuda/*.o \
		/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/*.o \
		/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/cuda-sim/decuda_pred_table/*.o \
		/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/gpgpu-sim/*.o \
		/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/intersim2/*.o \
		/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/*.o -lm -lz -lGL -pthread \
		/workspace/tmp/infra1_trace_wt/build/gcc-11.4.0/cuda-11080/release/accelwattch/*.o \
		-o lib/gcc-11.4.0/cuda-11080/release/libcudart.so
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.2 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.2; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.3 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.3; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.4 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.4; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.5.0 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.5.0; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.5.5 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.5.5; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.6.0 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.6.0; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.6.5 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.6.5; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.7.0 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.7.0; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.7.5 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.7.5; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.8.0 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.8.0; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.9.0 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.9.0; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.9.1 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.9.1; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.9.2 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.9.2; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.10.0 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.10.0; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.10.1 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.10.1; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart.so.11.0 ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart.so.11.0; fi
if [ ! -f lib/gcc-11.4.0/cuda-11080/release/libcudart_mod.so ]; then ln -s libcudart.so lib/gcc-11.4.0/cuda-11080/release/libcudart_mod.so; fi)
- collect_kernel_trace.py py_compile: pass
- paperrepro grep: pass
- source diff Mascar grep count: 0
- Mascar config count: 0
- paper-mascar path count: 0

## Trace defaults

- gpgpu_paperrepro_kernel_trace default: 0
- gpgpu_paperrepro_kernel_trace_max default: 4096
- gpgpu_paperrepro_kernel_trace_stats default: 0
