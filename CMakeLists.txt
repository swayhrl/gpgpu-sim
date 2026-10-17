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

# Opt-in source-bound structural diagnostics, not application experiments.
foreach(potential_test potential_s64 potential_a1)
  add_executable(ep_l2_${potential_test} EXCLUDE_FROM_ALL tests/ep_l2/test_${potential_test}.cc)
  target_include_directories(ep_l2_${potential_test} PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
  target_link_libraries(ep_l2_${potential_test} PRIVATE -Wl,--start-group
      entrypoint cuda ptxsim gpgpusim intersim accelwattch -Wl,--end-group
      ZLIB::ZLIB OpenGL::GL ${CMAKE_DL_LIBS} pthread)
endforeach()
