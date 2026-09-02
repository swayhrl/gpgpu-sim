#include <cuda_runtime.h>

#include <cstdio>

namespace {

__device__ int load_global_bypass(const int *address) {
  int value;
  asm volatile("ld.global.cg.s32 %0, [%1];" : "=r"(value) : "l"(address));
  return value;
}

__global__ void bypass_load(const int *input, int *output) {
  if (threadIdx.x == 0) output[0] = load_global_bypass(input);
}

bool check(cudaError_t status, const char *operation) {
  if (status == cudaSuccess) return true;
  std::fprintf(stderr, "%s: %s\n", operation, cudaGetErrorString(status));
  return false;
}

}  // namespace

int main() {
  int input = 11;
  int output = 0;
  int *device_input = nullptr;
  int *device_output = nullptr;
  if (!check(cudaMalloc(&device_input, sizeof(input)), "cudaMalloc(input)") ||
      !check(cudaMalloc(&device_output, sizeof(output)), "cudaMalloc(output)") ||
      !check(cudaMemcpy(device_input, &input, sizeof(input), cudaMemcpyHostToDevice),
             "cudaMemcpy(input)")) {
    return 1;
  }
  bypass_load<<<1, 32>>>(device_input, device_output);
  if (!check(cudaGetLastError(), "kernel launch") ||
      !check(cudaMemcpy(&output, device_output, sizeof(output),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy(output)")) {
    return 1;
  }
  std::printf("dtc_l1_legacy_bypass result: %s\n",
              output == input ? "PASS" : "FAIL");
  cudaFree(device_input);
  cudaFree(device_output);
  return output == input ? 0 : 1;
}
