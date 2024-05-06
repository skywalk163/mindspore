/**
 * Copyright 2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <math.h>
#include "blackman_window_impl.cuh"

template <typename S>
__global__ void BlackmanWindowOne(const size_t size, const double N, const double PI, S *output) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < size; pos += blockDim.x * gridDim.x) {
    output[pos] = static_cast<S>(1);
  }
  return;
}

template <typename S>
__global__ void BlackmanWindow(const size_t size, const double N, const double PI, S *output) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < size; pos += blockDim.x * gridDim.x) {
    double out = 0.42 - 0.5 * cos((2 * PI * pos) / (N - 1)) + 0.08 * cos((4 * PI * pos) / (N - 1));
    output[pos] = static_cast<S>(out);
  }
  return;
}

template <typename T, typename S>
cudaError_t CalBlackmanWindow(const size_t size, const T *input, const bool periodic, S *output,
                              const uint32_t &device_id, cudaStream_t cuda_stream) {
  const double PI = acos(-1);
  T N = 0;
  cudaMemcpy(&N, &input[0], sizeof(T), cudaMemcpyDeviceToHost);
  if (N == 1) {
    BlackmanWindowOne<<<CUDA_BLOCKS(device_id, size), CUDA_THREADS(device_id), 0, cuda_stream>>>(size, N, PI, output);
  } else {
    N = periodic ? static_cast<double>(N + 1) : static_cast<double>(N);
    BlackmanWindow<<<CUDA_BLOCKS(device_id, size), CUDA_THREADS(device_id), 0, cuda_stream>>>(size, N, PI, output);
  }
  return GetCudaStatus();
}

template CUDA_LIB_EXPORT cudaError_t CalBlackmanWindow<int, half>(const size_t size, const int *input,
                                                                  const bool periodic, half *output,
                                                                  const uint32_t &device_id, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalBlackmanWindow<int64_t, half>(const size_t size, const int64_t *input,
                                                                      const bool periodic, half *output,
                                                                      const uint32_t &device_id,
                                                                      cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalBlackmanWindow<int, float>(const size_t size, const int *input,
                                                                   const bool periodic, float *output,
                                                                   const uint32_t &device_id, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalBlackmanWindow<int64_t, float>(const size_t size, const int64_t *input,
                                                                       const bool periodic, float *output,
                                                                       const uint32_t &device_id,
                                                                       cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalBlackmanWindow<int, double>(const size_t size, const int *input,
                                                                    const bool periodic, double *output,
                                                                    const uint32_t &device_id,
                                                                    cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalBlackmanWindow<int64_t, double>(const size_t size, const int64_t *input,
                                                                        const bool periodic, double *output,
                                                                        const uint32_t &device_id,
                                                                        cudaStream_t cuda_stream);
