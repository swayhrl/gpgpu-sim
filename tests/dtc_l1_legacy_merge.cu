#include <cuda_runtime.h>

#include <cstdio>

namespace {

// One CTA provides 32 warps that all request one 128B line before the first
// miss can return.  This exercises the existing baseline MSHR merge path.
__global__ void same_line_many_warps(const int *input, int *output) {
  volatile const int *volatile_input = input;
  const int value = volatile_input[0];
  if (threadIdx.x == 0) output[blockIdx.x] = value;
}

bool check(cudaError_t status, const char *operation) {
  if (status == cudaSuccess) return true;
  std::fprintf(stderr, "%s: %s\n", operation, cudaGetErrorString(status));
  return false;
}

}  // namespace

int main() {
  int input = 7;
  int output = 0;
  int *device_input = nullptr;
  int *device_output = nullptr;
  if (!check(cudaMalloc(&device_input, sizeof(input)), "cudaMalloc(input)") ||
      !check(cudaMalloc(&device_output, sizeof(output)), "cudaMalloc(output)") ||
      !check(cudaMemcpy(device_input, &input, sizeof(input), cudaMemcpyHostToDevice),
             "cudaMemcpy(input)")) {
    return 1;
  }
  same_line_many_warps<<<1, 1024>>>(device_input, device_output);
  if (!check(cudaGetLastError(), "kernel launch") ||
      !check(cudaMemcpy(&output, device_output, sizeof(output),
                        cudaMemcpyDeviceToHost),
             "cudaMemcpy(output)")) {
    return 1;
  }
  std::printf("dtc_l1_legacy_merge result: %s\n",
              output == input ? "PASS" : "FAIL");
  cudaFree(device_input);
  cudaFree(device_output);
  return output == input ? 0 : 1;
}
