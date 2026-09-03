#include <cuda_runtime.h>

#include <cstdio>

namespace {

// The paper-facing conventional L1 has 32 sets and 128-byte lines.  These
// addresses are exactly one cache-set stride apart, so they map to one set
// regardless of cudaMalloc's base alignment.
constexpr int kLineWords = 128 / static_cast<int>(sizeof(int));
constexpr int kSetStrideWords = 32 * kLineWords;
constexpr int kWays = 4;
constexpr int kFifthLine = kWays * kSetStrideWords;

__global__ void dirty_set_ratio_zero(volatile int *lines, int *result) {
  if (blockIdx.x != 0 || threadIdx.x != 0) return;

  // Each store takes the configured write-through path and leaves a local
  // MODIFIED line.  The fifth same-set load must replace one of those lines
  // under ratio zero rather than remain a permanent RESERVATION_FAIL retry.
  for (int way = 0; way < kWays; ++way) {
    lines[way * kSetStrideWords] = 101 + way;
  }
  const int fifth_value = lines[kFifthLine];
  result[0] = fifth_value;
}

bool check(cudaError_t status, const char *operation) {
  if (status == cudaSuccess) return true;
  std::fprintf(stderr, "%s: %s\n", operation, cudaGetErrorString(status));
  return false;
}

}  // namespace

int main() {
  constexpr int kWords = kFifthLine + 1;
  int host_lines[kWords] = {};
  host_lines[kFifthLine] = 17;
  int host_result = 0;
  int *device_lines = nullptr;
  int *device_result = nullptr;

  if (!check(cudaMalloc(&device_lines, sizeof(host_lines)), "cudaMalloc(lines)") ||
      !check(cudaMalloc(&device_result, sizeof(host_result)),
             "cudaMalloc(result)") ||
      !check(cudaMemcpy(device_lines, host_lines, sizeof(host_lines),
                        cudaMemcpyHostToDevice),
             "cudaMemcpy(lines H2D)") ||
      !check(cudaFuncSetCacheConfig(dirty_set_ratio_zero, cudaFuncCachePreferL1),
             "cudaFuncSetCacheConfig")) {
    return 1;
  }

  dirty_set_ratio_zero<<<1, 32>>>(device_lines, device_result);
  if (!check(cudaGetLastError(), "kernel launch") ||
      !check(cudaMemcpy(&host_result, device_result, sizeof(host_result),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy(result D2H)") ||
      !check(cudaMemcpy(host_lines, device_lines, sizeof(host_lines),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy(lines D2H)")) {
    return 1;
  }

  bool pass = host_result == 17;
  for (int way = 0; way < kWays; ++way) {
    pass = pass && host_lines[way * kSetStrideWords] == 101 + way;
  }
  pass = pass && host_lines[kFifthLine] == 17;
  std::printf("dtc_l1_dirty_victim_ratio_zero result: %s\n",
              pass ? "PASS" : "FAIL");
  cudaFree(device_lines);
  cudaFree(device_result);
  return pass ? 0 : 1;
}
