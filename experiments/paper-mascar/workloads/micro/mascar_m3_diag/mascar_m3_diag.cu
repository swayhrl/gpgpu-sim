#include <cuda_runtime.h>
#include <stdio.h>

__global__ void mascar_m3_diag_kernel(const float *hot, const float *cold,
                                      float *out, int hot_mask,
                                      int cold_n, int outer_iters,
                                      int hot_reuse, int stride) {
  int tid = blockIdx.x * blockDim.x + threadIdx.x;
  int lane = threadIdx.x & 31;
  float acc = 0.0f;
  for (int it = 0; it < outer_iters; ++it) {
    int cold_idx = (tid * stride + it * 131 + lane * 17) % cold_n;
    acc += cold[cold_idx];
    #pragma unroll 4
    for (int r = 0; r < hot_reuse; ++r) {
      int hot_idx = (lane + r + (it & 7)) & hot_mask;
      acc += hot[hot_idx] * 1.000001f;
    }
  }
  out[tid] = acc;
}

static void fill(float *p, int n, float base) {
  for (int i = 0; i < n; ++i) p[i] = base + (float)(i & 255) * 0.001f;
}

int main(int argc, char **argv) {
  int blocks = argc > 1 ? atoi(argv[1]) : 256;
  int threads = argc > 2 ? atoi(argv[2]) : 256;
  int cold_n = argc > 3 ? atoi(argv[3]) : (1 << 22);
  int hot_n = argc > 4 ? atoi(argv[4]) : 1024;
  int outer_iters = argc > 5 ? atoi(argv[5]) : 64;
  int hot_reuse = argc > 6 ? atoi(argv[6]) : 8;
  int stride = argc > 7 ? atoi(argv[7]) : 97;
  int total_threads = blocks * threads;
  if ((hot_n & (hot_n - 1)) != 0) {
    fprintf(stderr, "hot_n must be power of two\n");
    return 2;
  }
  float *h_hot = (float *)malloc(sizeof(float) * hot_n);
  float *h_cold = (float *)malloc(sizeof(float) * cold_n);
  float *h_out = (float *)malloc(sizeof(float) * total_threads);
  fill(h_hot, hot_n, 1.0f);
  fill(h_cold, cold_n, 2.0f);
  float *d_hot = 0, *d_cold = 0, *d_out = 0;
  cudaMalloc(&d_hot, sizeof(float) * hot_n);
  cudaMalloc(&d_cold, sizeof(float) * cold_n);
  cudaMalloc(&d_out, sizeof(float) * total_threads);
  cudaMemcpy(d_hot, h_hot, sizeof(float) * hot_n, cudaMemcpyHostToDevice);
  cudaMemcpy(d_cold, h_cold, sizeof(float) * cold_n, cudaMemcpyHostToDevice);
  mascar_m3_diag_kernel<<<blocks, threads>>>(d_hot, d_cold, d_out, hot_n - 1,
                                             cold_n, outer_iters, hot_reuse,
                                             stride);
  cudaError_t err = cudaDeviceSynchronize();
  if (err != cudaSuccess) {
    fprintf(stderr, "cuda error: %s\n", cudaGetErrorString(err));
    return 1;
  }
  cudaMemcpy(h_out, d_out, sizeof(float) * total_threads, cudaMemcpyDeviceToHost);
  double sum = 0.0;
  for (int i = 0; i < total_threads; i += total_threads / 16 + 1) sum += h_out[i];
  printf("mascar_m3_diag_sum=%0.6f\n", sum);
  cudaFree(d_hot); cudaFree(d_cold); cudaFree(d_out);
  free(h_hot); free(h_cold); free(h_out);
  return 0;
}
