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

# Enable position-independent code for all targets (required for shared libraries)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# GPGPU-Sim build option
option(GPGPUSIM_ENABLE_TRACE "Whether to enable GPGPU-Sim debug tracing" ON)
option(GPGPUSIM_BUILD_DTC_L1_TESTS
       "Build deterministic Decoupled-Tag L1 directed tests" OFF)
option(GPGPUSIM_SEM1_CONDITIONAL_VALUES
       "Isolated SEM1 fixture payload data plane (not architectural values)" OFF)
if(GPGPUSIM_SEM1_CONDITIONAL_VALUES)
    add_compile_definitions(DTC_SEM1_CONDITIONAL_VALUES=1)
endif()

# GPGPU-Sim conditional build variable
set(GPGPUSIM_USE_POWER_MODEL OFF)
set(GPGPUSIM_USE_OPENCL OFF)

# Check for dependencies
include(gpgpusim_check.cmake)
find_package(ZLIB REQUIRED)

# Create version file
add_custom_target(gen_build_string ALL 
                    COMMAND ${CMAKE_COMMAND} -D INPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR} -D OUTPUT_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/gpgpusim_gen_build_string.cmake
                    COMMENT "Generating build string file to ${CMAKE_CURRENT_BINARY_DIR}")

# CMake target
# GPGPU-Sim CUDA Runtime lib
# Use the entrypoint object files sources else CMake will complain
add_library(cudart SHARED $<TARGET_OBJECTS:gpgpusim_entrypoint>)
add_library(entrypoint STATIC $<TARGET_OBJECTS:gpgpusim_entrypoint>)

# Remove -DNDEBUG from Release and RelWithDebInfo to keep assertions enabled
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

# Add global C/CXX compilation flags and definitions
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
    add_compile_definitions(DEBUG=1)
else()
    add_compile_definitions(DEBUG=0)
endif()

# include debug symbols and frame pointers for profiling (perf, gdb)
# add_compile_options(-g -fno-omit-frame-pointer)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-Wall -Wno-unused-function -Wno-sign-compare)
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

if(GPGPUSIM_BUILD_DTC_L1_TESTS)
    enable_testing()
    add_executable(dtc_l1_m1_common_test tests/dtc_l1_m1_common_test.cc)
    target_include_directories(dtc_l1_m1_common_test PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim)
    add_test(NAME dtc_l1_m1_common_test COMMAND dtc_l1_m1_common_test)
    add_executable(dtc_l1_bad_generation_test
                   tests/dtc_l1_bad_generation_test.cc)
    target_include_directories(dtc_l1_bad_generation_test PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim)
    add_test(NAME dtc_l1_bad_generation_test
             COMMAND dtc_l1_bad_generation_test)
    add_executable(dtc_l1_completion_accounting_test
                   tests/dtc_l1_completion_accounting_test.cc)
    target_include_directories(dtc_l1_completion_accounting_test PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim)
    add_test(NAME dtc_l1_completion_accounting_test
             COMMAND dtc_l1_completion_accounting_test)
    add_executable(dtc_l1_finite_service_test tests/dtc_l1_finite_service_test.cc)
    target_include_directories(dtc_l1_finite_service_test PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim)
    add_test(NAME dtc_l1_finite_service_test COMMAND dtc_l1_finite_service_test)
    add_executable(dtc_l1_source_invalidation_test
                   tests/dtc_l1_source_invalidation_test.cc)
    target_include_directories(dtc_l1_source_invalidation_test PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim)
    add_test(NAME dtc_l1_source_invalidation_test
             COMMAND dtc_l1_source_invalidation_test)
    add_executable(dtc_l1_source_invalidation_integration_test
                   tests/dtc_l1_source_invalidation_integration_test.cc)
    target_link_libraries(dtc_l1_source_invalidation_integration_test PRIVATE cudart)
    target_include_directories(dtc_l1_source_invalidation_integration_test PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim
                               ${CUDAToolkit_INCLUDE_DIRS})
    add_test(NAME dtc_l1_source_invalidation_integration_test
             COMMAND dtc_l1_source_invalidation_integration_test)
    set_tests_properties(dtc_l1_source_invalidation_integration_test PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cudart>")
    add_executable(dtc_l1_k0_equivalence_test tests/dtc_l1_k0_equivalence_test.cc)
    target_include_directories(dtc_l1_k0_equivalence_test PRIVATE
                               ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim)
    add_test(NAME dtc_l1_k0_equivalence_test COMMAND dtc_l1_k0_equivalence_test)
    add_executable(dtc_l1_tag_hash_bankrow_test tests/dtc_l1_tag_hash_bankrow_test.cc)
    target_include_directories(dtc_l1_tag_hash_bankrow_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(dtc_l1_tag_hash_bankrow_test PRIVATE DTC_AE_NEW=1)
    target_compile_options(dtc_l1_tag_hash_bankrow_test PRIVATE -fno-access-control)
    add_test(NAME dtc_l1_tag_hash_bankrow_test COMMAND dtc_l1_tag_hash_bankrow_test conflicts 64 8 1)
    add_executable(dtc_l1_ag3_aead_interleave_test tests/dtc_l1_ag3_aead_interleave_test.cc)
    target_include_directories(dtc_l1_ag3_aead_interleave_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim)
    add_test(NAME dtc_l1_ag3_aead_interleave_test COMMAND dtc_l1_ag3_aead_interleave_test)
    add_executable(dtc_l1_progressive_wakeup_test tests/dtc_l1_progressive_wakeup_test.cc)
    target_include_directories(dtc_l1_progressive_wakeup_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim)
    add_test(NAME dtc_l1_progressive_wakeup_test COMMAND dtc_l1_progressive_wakeup_test)
    add_executable(dtc_l1_progressive_ldst_test tests/dtc_l1_progressive_ldst_test.cc)
    target_include_directories(dtc_l1_progressive_ldst_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/libcuda
        ${CUDAToolkit_INCLUDE_DIRS})
    target_compile_options(dtc_l1_progressive_ldst_test PRIVATE -fno-access-control)
    target_link_libraries(dtc_l1_progressive_ldst_test PRIVATE cudart)
    # The CPU fixture explicitly uses one channel. Inherited IPOLY indexing
    # requires 16/32/64 channels; use supported linear translation for these
    # synthetic mem_fetch inputs without editing the production config.
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/configs/tested-cfgs/SM7_QV100/gpgpusim.config"
         ad_cpu_base_config)
    set(ad_cpu_config "${CMAKE_BINARY_DIR}/dtc_l1_ad_cpu.config")
    file(WRITE "${ad_cpu_config}"
         "${ad_cpu_base_config}\n# AD CPU-only one-channel address translation\n-gpgpu_memory_partition_indexing 0\n")
    add_test(NAME dtc_l1_progressive_ldst_test COMMAND dtc_l1_progressive_ldst_test
        ${ad_cpu_config})
    set_tests_properties(dtc_l1_progressive_ldst_test PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:cudart>;CUDA_VISIBLE_DEVICES=")
    # Pinned, read-only X2 header for same-build OFF differential CPU traces.
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/ad_x2_header")
    execute_process(COMMAND git show e32afdcf44be3374ff36d22568ed01a075459b6f:src/gpgpu-sim/dtc-l1-common.h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_FILE "${CMAKE_BINARY_DIR}/ad_x2_header/dtc-l1-common.h"
        RESULT_VARIABLE ad_x2_status)
    if(NOT ad_x2_status EQUAL 0)
        message(FATAL_ERROR "AD OFF comparison requires the fixed X2 header")
    endif()
    foreach(ad_variant IN ITEMS candidate x2)
        add_executable(dtc_l1_ad_off_${ad_variant} tests/dtc_l1_ad_off_trace.cc)
        add_executable(dtc_l1_ad_modes_${ad_variant} tests/dtc_l1_k0_equivalence_test.cc)
        if(ad_variant STREQUAL "x2")
            set(ad_header_dir "${CMAKE_BINARY_DIR}/ad_x2_header")
        else()
            set(ad_header_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/gpgpu-sim")
        endif()
        target_include_directories(dtc_l1_ad_off_${ad_variant} PRIVATE ${ad_header_dir})
        target_include_directories(dtc_l1_ad_modes_${ad_variant} PRIVATE ${ad_header_dir})
    endforeach()
    add_executable(dtc_l1_normal_export_probe tests/dtc_l1_normal_export_probe.cc)
    target_include_directories(dtc_l1_normal_export_probe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/libcuda
        ${CUDAToolkit_INCLUDE_DIRS})
    target_compile_options(dtc_l1_normal_export_probe PRIVATE -fno-access-control)
    target_link_libraries(dtc_l1_normal_export_probe PRIVATE cudart ${CMAKE_DL_LIBS})
endif()

# Set linker option for libcudart.so
if(GPGPUSIM_SEM1_CONDITIONAL_VALUES)
    add_executable(dtc_l1_sem1_values_ldst tests/dtc_l1_sem1_values_ldst.cc)
    target_include_directories(dtc_l1_sem1_values_ldst PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/libcuda
        ${CUDAToolkit_INCLUDE_DIRS})
    target_compile_options(dtc_l1_sem1_values_ldst PRIVATE -fno-access-control)
    target_link_libraries(dtc_l1_sem1_values_ldst PRIVATE cudart ${CMAKE_DL_LIBS})
endif()

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
target_link_libraries(cudart PUBLIC cuda ptxsim gpgpusim intersim ZLIB::ZLIB)
target_link_libraries(entrypoint PUBLIC cuda ptxsim gpgpusim intersim ZLIB::ZLIB)
if(GPGPUSIM_USE_POWER_MODEL)
target_link_libraries(cudart PUBLIC cuda ptxsim gpgpusim intersim accelwattch ZLIB::ZLIB)
target_link_libraries(entrypoint PUBLIC cuda ptxsim gpgpusim intersim accelwattch ZLIB::ZLIB)
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
