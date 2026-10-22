cmake_minimum_required(VERSION 3.17)

# Project name and version
project(GPGPU-Sim 
        VERSION 4.2.0 
        DESCRIPTION "cycle-level simulator modeling contemporary graphics processing units (GPUs)" 
        HOMEPAGE_URL https://github.com/accel-sim/gpgpu-sim_distribution
        LANGUAGES CXX)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# GPGPU-Sim build option
option(GPGPUSIM_ENABLE_TRACE "Whether to enable GPGPU-Sim debug tracing" ON)

# GPGPU-Sim conditional build variable
set(GPGPUSIM_USE_POWER_MODEL OFF)
set(GPGPUSIM_USE_OPENCL OFF)

# Check for dependencies
include(gpgpusim_check.cmake)

# Create version file
add_custom_target(gen_build_string ALL 
                    COMMAND ${CMAKE_COMMAND} -D INPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR} -D OUTPUT_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/gpgpusim_gen_build_string.cmake
                    COMMENT "Generating build string file to ${CMAKE_CURRENT_BINARY_DIR}")

# CMake target
# GPGPU-Sim CUDA Runtime lib
# Use the entrypoint object files sources else CMake will complain
add_library(cudart SHARED $<TARGET_OBJECTS:gpgpusim_entrypoint>)
add_library(entrypoint STATIC $<TARGET_OBJECTS:gpgpusim_entrypoint>)

# Add global C/CXX compilation flags and definitions
# TODO Specify more build modes like gem5 with fast opt?
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_compile_definitions(DEBUG=1)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wno-unused-function;-Wno-sign-compare;-g;-fPIC>")
    add_compile_options("$<$<COMPILE_LANGUAGE:C>:-Wall;-Wno-unused-function;-Wno-sign-compare;-ggdb;-fPIC>")
else()
    add_compile_definitions(DEBUG=0)
    # Override the default release flags to keep asserts enabled
    set(CMAKE_CXX_FLAGS_RELEASE "-Wall -O3 -g3 -fPIC")
    set(CMAKE_C_FLAGS_RELEASE "-Wall -O3 -g3 -fPIC")
endif()

# Add CUDA version
add_compile_definitions(CUDART_VERSION=${CUDA_VERSION_NUMBER})

# OpenCL support
if(GPGPUSIM_USE_OPENCL)
    add_compile_definitions(OPENGL_SUPPORT)
endif()

# Tracing support
if(GPGPUSIM_ENABLE_TRACE)
    add_compile_definitions(TRACING_ON=1)
endif()

# Add subdirectory
add_subdirectory(src)
add_subdirectory(libcuda)
add_subdirectory(libopencl)

# Set linker option for libcudart.so
if(APPLE)
    target_link_options(cudart PUBLIC "-Wl,-headerpad_max_install_names,-undefined,dynamic_lookup,-compatibility_version,1.1,-current_version,1.1;-lm;-lz;-pthread")
else()
    target_link_options(cudart PUBLIC
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/linux-so-version.txt;-lm;-lz;-lGL;-pthread")
        target_link_options(entrypoint PUBLIC
        "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/linux-so-version.txt;-lm;-lz;-lGL;-pthread")
endif()
# cuda: CUDA API lib
# ptxsim: cuda-sim, functional simulator
# gpgpusim: gpu simulator (gpgpu-sim)
# intersim: interconnect simulator
# accelwattch: power simulator
# Rest of source files in src/ will be created with gpgpusim_entrypoint target
target_link_libraries(cudart PUBLIC cuda ptxsim gpgpusim intersim)
target_link_libraries(entrypoint PUBLIC cuda ptxsim gpgpusim intersim)
if(GPGPUSIM_USE_POWER_MODEL)
target_link_libraries(cudart PUBLIC cuda ptxsim gpgpusim intersim accelwattch)
target_link_libraries(entrypoint PUBLIC cuda ptxsim gpgpusim intersim accelwattch)
endif()

# TODO Conditionally build for Opencl?
# if(GPGPUSIM_USE_OPENCL)
# add_library(OpenCL)
# endif()

# Install and post-install
# Get configure
set(GPGPUSIM_CONFIG "gcc-${CMAKE_CXX_COMPILER_VERSION}/cuda-${CUDA_VERSION_NUMBER}/${GPGPUSIM_BUILD_MODE}")

# Env var setup script
include(gpgpusim_gen_setup_environment.cmake)

# Installation
set(GPGPUSIM_INSTALL_PATH ${PROJECT_SOURCE_DIR}/lib/${GPGPUSIM_CONFIG})
install(TARGETS cudart DESTINATION ${GPGPUSIM_INSTALL_PATH})

# Installing symlinks
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.2\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.3\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.4\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.5.0\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.5.5\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.6.0\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.6.5\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.7.0\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.7.5\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.8.0\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.9.0\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.9.1\)")
    install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.9.2\)")
    install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.10.0\)")
    install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.10.1\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.11.0\)")
install(CODE "execute_process\(\
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart> \
    ${GPGPUSIM_INSTALL_PATH}/$<TARGET_FILE_NAME:cudart>.12\)")

# Explicit, lightweight production-path FT test; never part of the normal build.
add_executable(ep_l2_ft_directed EXCLUDE_FROM_ALL tests/ep_l2/test_ft_wbuf.cc)
target_include_directories(ep_l2_ft_directed PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(ep_l2_ft_directed PRIVATE -Wl,--start-group
    entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
    ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
add_executable(ep_l2_ft_partition EXCLUDE_FROM_ALL tests/ep_l2/test_ft_partition.cc)
target_include_directories(ep_l2_ft_partition PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(ep_l2_ft_partition PRIVATE -Wl,--start-group
    entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
    ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)

foreach(repair_test ft_memcpy_clock ft_host_prefill)
  add_executable(ep_l2_${repair_test} EXCLUDE_FROM_ALL tests/ep_l2/test_${repair_test}.cc)
  target_include_directories(ep_l2_${repair_test} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_${repair_test} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()

add_executable(ep_l2_ft_a1 EXCLUDE_FROM_ALL tests/ep_l2/test_ft_a1.cc)
target_include_directories(ep_l2_ft_a1 PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(ep_l2_ft_a1 PRIVATE -Wl,--start-group
    entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
    ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)

# Lane2 minimal public atomic-sector regression, without TVD integration.
add_executable(ep_l2_common_delta_r16 EXCLUDE_FROM_ALL tests/ep_l2/test_common_delta_r16.cc)
target_include_directories(ep_l2_common_delta_r16 PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(ep_l2_common_delta_r16 PRIVATE -Wl,--start-group
    entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
    ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)

# Lane4 potential campaign: separately requested, never default build targets.
foreach(ideal_test ideal_wb ideal_meta ideal_meta_integrated ideal_read_hit
                   ideal_host_prefill_reference ideal_host_prefill_partition
                   all_data_reference all_data_ldst)
  add_executable(ep_l2_${ideal_test} EXCLUDE_FROM_ALL tests/ep_l2/test_${ideal_test}.cc)
  target_include_directories(ep_l2_${ideal_test} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_${ideal_test} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()

# Opt-in source-bound structural diagnostics, not application experiments.
foreach(potential_test potential_s64 potential_a1 potential_ucap potential_ucap_retry potential_p2 micro_mq2 micro_rg1_partition micro_rg1_arbiter)
  add_executable(ep_l2_${potential_test} EXCLUDE_FROM_ALL tests/ep_l2/test_${potential_test}.cc)
  target_include_directories(ep_l2_${potential_test} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_${potential_test} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()
target_compile_definitions(ep_l2_micro_mq2 PRIVATE EPL2_MICROOPT_CANDIDATE=1)

target_compile_definitions(ep_l2_micro_rg1_partition PRIVATE EPL2_MICROOPT_CANDIDATE=1)

# Mechanism study A: finite production diagnostics, opt-in targets only.
# No test or application executes as part of configure or the default build.
foreach(ms_test ms_local_service ms_partition ms_mode_guard)
  add_executable(ep_l2_${ms_test} EXCLUDE_FROM_ALL tests/ep_l2/test_${ms_test}.cc)
  target_include_directories(ep_l2_${ms_test} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_${ms_test} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()
add_executable(ep_l2_ms_observer EXCLUDE_FROM_ALL tests/ep_l2/test_ms_observer.cc)

# A-owned integrated victim candidate: finite source qualification targets only.
foreach(victim_target directory production)
  add_executable(ep_l2_ms_victim_${victim_target} EXCLUDE_FROM_ALL tests/ep_l2/test_tvd_${victim_target}.cc)
  target_include_directories(ep_l2_ms_victim_${victim_target} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_ms_victim_${victim_target} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()
add_executable(ep_l2_ms_victim_native EXCLUDE_FROM_ALL tests/ep_l2/ms_victim/test_native.cc)
target_include_directories(ep_l2_ms_victim_native PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(ep_l2_ms_victim_native PRIVATE -Wl,--start-group
    entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
    ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)

# B r2's real finite allocator/arbiter checks, A-owned integration only.
foreach(victim_test bank_identity fixed_partition)
  add_executable(ep_l2_ms_victim_${victim_test} EXCLUDE_FROM_ALL
      tests/ep_l2/ms_victim/test_${victim_test}.cc)
  target_include_directories(ep_l2_ms_victim_${victim_test} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_ms_victim_${victim_test} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()

# One shared B/C production-partition input; never built/run by default.
# Common RO source and the shared B/C input are integrated; both remain
# uncompiled. The switch only opts into the target, never native execution.
option(EP_L2_BUILD_VRW_FIXTURE "Build A-integrated shared V/RO/W fixture" OFF)
if(EP_L2_BUILD_VRW_FIXTURE)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim/ep-l2-ro-runtime.inc")
    message(FATAL_ERROR "VRW fixture requires A common RO runtime integration")
  endif()
  add_executable(ep_l2_ms_vrw_interleave EXCLUDE_FROM_ALL
      tests/ep_l2/ms_victim/test_vrw_interleave.cc)
  target_include_directories(ep_l2_ms_vrw_interleave PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_ms_vrw_interleave PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endif()

# C bounded production fixtures integrated by A. Bank facade needs source
# extraction, not a direct production target; see prepare_ro_bank_progress.py.
foreach(ro_test ro_runtime ro_partition ro_detailed_origin ro_transactions ro_store_port ro_oracle_certificate ro_epoch_entry)
  add_executable(ep_l2_${ro_test} EXCLUDE_FROM_ALL tests/ep_l2/test_${ro_test}.cc)
  target_include_directories(ep_l2_${ro_test} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_${ro_test} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()

# Explicit source-bound facade only. Never configured/run by default.
option(EP_L2_BUILD_RO_BANK_FIXTURE "Build source-bound RO bank facade" OFF)
set(EP_L2_RO_BANK_SOURCE_COMMIT "" CACHE STRING "Exact immutable RO Core source")
if(EP_L2_BUILD_RO_BANK_FIXTURE)
  string(LENGTH "${EP_L2_RO_BANK_SOURCE_COMMIT}" ro_source_length)
  if(NOT ro_source_length EQUAL 40 OR NOT EP_L2_RO_BANK_SOURCE_COMMIT MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "RO bank fixture requires exact40-hex source commit")
  endif()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(ro_bank_generated "${CMAKE_CURRENT_BINARY_DIR}/ro_bank_bound")
  add_custom_command(OUTPUT "${ro_bank_generated}/SOURCE_BINDING.json"
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/ep_l2/prepare_ro_bank_progress.py"
      --source "${EP_L2_RO_BANK_SOURCE_COMMIT}" --output "${ro_bank_generated}"
    DEPENDS tests/ep_l2/prepare_ro_bank_progress.py tests/ep_l2/test_ro_bank_progress.cc
      src/gpgpu-sim/gpu-cache.h src/gpgpu-sim/gpu-cache.cc
      src/gpgpu-sim/ep-l2-ro-mshr.inc src/gpgpu-sim/ep-l2-ro-runtime.inc
      src/gpgpu-sim/ep-l2-ro-transactions.h src/gpgpu-sim/ep-l2-ro-store-port.h
      src/gpgpu-sim/ep-l2-ro-oracle.h src/gpgpu-sim/ep-l2-ft-cache.inc
    COMMENT "Bind finite bank facade to exact RO source; no native execution"
    VERBATIM)
  add_custom_target(ep_l2_ro_bank_bind DEPENDS "${ro_bank_generated}/SOURCE_BINDING.json")
  add_executable(ep_l2_ro_bank_progress EXCLUDE_FROM_ALL tests/ep_l2/test_ro_bank_progress.cc)
  add_dependencies(ep_l2_ro_bank_progress ep_l2_ro_bank_bind)
  target_include_directories(ep_l2_ro_bank_progress PRIVATE "${ro_bank_generated}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim" "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/libcuda" ${CUDAToolkit_INCLUDE_DIRS})
  # Actual source methods are included once in this standalone facade.
  # Linking gpu-cache.o as well would redefine those methods.
  target_compile_options(ep_l2_ro_bank_progress PRIVATE -O2 -g0)
endif()

add_executable(ep_l2_ro_tvd_combined EXCLUDE_FROM_ALL tests/ep_l2/ms_ro_combined/test_combined.cc)
target_include_directories(ep_l2_ro_tvd_combined PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(ep_l2_ro_tvd_combined PRIVATE -Wl,--start-group entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)

# Isolated passive observer finite control and exact-d010 archive reference.
add_executable(ep_l2_detach_diagnostic EXCLUDE_FROM_ALL tests/ep_l2/test_detach_diagnostic.cc)
target_include_directories(ep_l2_detach_diagnostic PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(ep_l2_detach_diagnostic PRIVATE -Wl,--start-group entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
add_executable(ep_l2_detach_reference EXCLUDE_FROM_ALL tests/ep_l2/test_detach_diagnostic.cc)
target_compile_definitions(ep_l2_detach_reference PRIVATE EP_L2_DIAG_REFERENCE=1)
target_include_directories(ep_l2_detach_reference BEFORE PRIVATE /workspace/worktrees/gpgpu-sim-ep-l2-ms20261020-a-r39-own-pending-v1/src /workspace/worktrees/gpgpu-sim-ep-l2-ms20261020-a-r39-own-pending-v1/libcuda ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(ep_l2_detach_reference PRIVATE -Wl,--start-group
    /workspace/build/ep-l2-ms20261019-a-common-native-r1/gpgpu-sim/libentrypoint.a
    /workspace/build/ep-l2-ms20261019-a-common-native-r1/gpgpu-sim/libcuda/libcuda.a
    /workspace/build/ep-l2-ms20261019-a-common-native-r1/gpgpu-sim/src/cuda-sim/libptxsim.a
    /workspace/build/ep-l2-ms20261020-a-wad-pending-r40/libgpgpusim.a
    /workspace/build/ep-l2-ms20261019-a-common-native-r1/gpgpu-sim/src/intersim2/libintersim.a
    /workspace/build/ep-l2-ms20261019-a-common-native-r1/gpgpu-sim/src/accelwattch/libaccelwattch.a
    -Wl,--end-group ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)

# r5 passive metadata-only bounded controls; candidate header/archive stay consistent.
add_executable(ep_l2_tagpressure_metadata EXCLUDE_FROM_ALL tests/ep_l2/test_tagpressure_metadata.cc)
target_compile_options(ep_l2_tagpressure_metadata PRIVATE -UNDEBUG)
