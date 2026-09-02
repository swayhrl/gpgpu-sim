#include <cuda_runtime.h>

#include <cstdio>

namespace {

__global__ void repeated_line_load(const int *input, int *output) {
  volatile const int *volatile_input = input;
  int value = 0;
#pragma unroll 1
  for (int iteration = 0; iteration < 64; ++iteration) {
    value += volatile_input[0];
  }
  if (threadIdx.x == 0) output[0] = value;
}

bool check(cudaError_t status, const char *operation) {
  if (status == cudaSuccess) return true;
  std::fprintf(stderr, "%s: %s\n", operation, cudaGetErrorString(status));
  return false;
}

}  // namespace

int main() {
  int input = 3;
  int output = 0;
  int *device_input = nullptr;
  int *device_output = nullptr;
  if (!check(cudaMalloc(&device_input, sizeof(input)), "cudaMalloc(input)") ||
      !check(cudaMalloc(&device_output, sizeof(output)), "cudaMalloc(output)") ||
      !check(cudaMemcpy(device_input, &input, sizeof(input), cudaMemcpyHostToDevice),
             "cudaMemcpy(input)")) {
    return 1;
  }
  repeated_line_load<<<1, 32>>>(device_input, device_output);
  if (!check(cudaGetLastError(), "kernel launch") ||
      !check(cudaMemcpy(&output, device_output, sizeof(output),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy(output)")) {
    return 1;
  }
  std::printf("dtc_l1_legacy_hit result: %s\n",
              output == 64 * input ? "PASS" : "FAIL");
  cudaFree(device_input);
  cudaFree(device_output);
  return output == 64 * input ? 0 : 1;
}
