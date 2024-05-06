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

#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/heaviside_impl.cuh"

__constant__ size_t start_cal[5];
__constant__ size_t end_cal[5];
__constant__ size_t output_cal[5];

template <typename T>
struct HeavisideFunc {
  __device__ __host__ __forceinline__ T operator()(const T &x1, const T &x2) {
    if (x1 > T(0)) {
      return T(1);
    } else if (x1 == T(0)) {
      return x2;
    } else {
      return T(0);
    }
  }
};

template <typename T, typename Func>
__global__ void CalHeavisideKernel(size_t size, const T *x1, const T *x2, T *y) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < size; pos += blockDim.x * gridDim.x) {
    y[pos] = Func()(x1[pos], x2[pos]);
  }
}

__device__ __forceinline__ size_t Index(const size_t &index, const size_t &dim) { return dim == 1 ? 0 : index; }

template <typename T, typename Func>
__global__ void BroadcastHeavisideKernel(const size_t l0, const size_t l1, const size_t l2, const size_t l3,
                                         const size_t l4, const size_t l5, const size_t l6, const size_t r0,
                                         const size_t r1, const size_t r2, const size_t r3, const size_t r4,
                                         const size_t r5, const size_t r6, const size_t d0, const size_t d1,
                                         const size_t d2, const size_t d3, const size_t d4, const size_t d5,
                                         const size_t d6, const T *x1, const T *x2, T *y) {
  for (size_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < d0 * d1 * d2 * d3 * d4 * d5 * d6;
       pos += blockDim.x * gridDim.x) {
    size_t i = pos / output_cal[0] % d0;
    size_t j = pos / output_cal[1] % d1;
    size_t k = pos / output_cal[2] % d2;
    size_t l = pos / output_cal[3] % d3;
    size_t m = pos / output_cal[4] % d4;
    size_t n = pos / d6 % d5;
    size_t o = pos % d6;

    size_t l_index = Index(i, l0) * start_cal[0];
    l_index += Index(j, l1) * start_cal[1];
    l_index += Index(k, l2) * start_cal[2];
    l_index += Index(l, l3) * start_cal[3];
    l_index += Index(m, l4) * start_cal[4];
    l_index += Index(n, l5) * l6;
    l_index += Index(o, l6);
    size_t r_index = Index(i, r0) * end_cal[0];
    r_index += Index(j, r1) * end_cal[1];
    r_index += Index(k, r2) * end_cal[2];
    r_index += Index(l, r3) * end_cal[3];
    r_index += Index(m, r4) * end_cal[4];
    r_index += Index(n, r5) * r6;
    r_index += Index(o, r6);
    y[pos] = Func()(x1[l_index], x2[r_index]);
  }
}

template <typename T>
cudaError_t CalHeaviside(size_t size, const T *x1, const T *x2, T *y, const uint32_t &device_id,
                         cudaStream_t cuda_stream) {
  CalHeavisideKernel<T, HeavisideFunc<T>>
    <<<CUDA_BLOCKS(device_id, size), CUDA_THREADS(device_id), 0, cuda_stream>>>(size, x1, x2, y);
  return GetCudaStatus();
}

cudaError_t CalData(const std::vector<size_t> &start_shape, size_t *output) {
  output[4] = start_shape[5] * start_shape[6];
  output[3] = output[4] * start_shape[4];
  output[2] = output[3] * start_shape[3];
  output[1] = output[2] * start_shape[2];
  output[0] = output[1] * start_shape[1];
  return GetCudaStatus();
}

template <typename T>
cudaError_t BroadcastHeaviside(const std::vector<size_t> &x1_shape, const std::vector<size_t> &x2_shape,
                               const std::vector<size_t> &y_shape, const T *x1, const T *x2, T *y,
                               const uint32_t &device_id, cudaStream_t cuda_stream) {
  size_t size = 1;
  for (auto d : y_shape) {
    size *= d;
  }
  size_t start_dim[5];
  size_t end_dim[5];
  size_t output_dim[5];
  CalData(x1_shape, start_dim);
  CalData(x2_shape, end_dim);
  CalData(y_shape, output_dim);
  cudaMemcpyToSymbol(start_cal, start_dim, sizeof(size_t) * 5);
  cudaMemcpyToSymbol(end_cal, end_dim, sizeof(size_t) * 5);
  cudaMemcpyToSymbol(output_cal, output_dim, sizeof(size_t) * 5);
  BroadcastHeavisideKernel<T, HeavisideFunc<T>>
    <<<CUDA_BLOCKS(device_id, size), CUDA_THREADS(device_id), 0, cuda_stream>>>(
      x1_shape[0], x1_shape[1], x1_shape[2], x1_shape[3], x1_shape[4], x1_shape[5], x1_shape[6], x2_shape[0],
      x2_shape[1], x2_shape[2], x2_shape[3], x2_shape[4], x2_shape[5], x2_shape[6], y_shape[0], y_shape[1], y_shape[2],
      y_shape[3], y_shape[4], y_shape[5], y_shape[6], x1, x2, y);
  return GetCudaStatus();
}
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<uint8_t>(size_t, const uint8_t *, const uint8_t *, uint8_t *,
                                                           const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<uint16_t>(size_t, const uint16_t *, const uint16_t *, uint16_t *,
                                                            const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<uint32_t>(size_t, const uint32_t *, const uint32_t *, uint32_t *,
                                                            const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<uint64_t>(size_t, const uint64_t *, const uint64_t *, uint64_t *,
                                                            const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<int8_t>(size_t, const int8_t *, const int8_t *, int8_t *,
                                                          const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<int16_t>(size_t, const int16_t *, const int16_t *, int16_t *,
                                                           const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<int32_t>(size_t, const int32_t *, const int32_t *, int32_t *,
                                                           const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<int64_t>(size_t, const int64_t *, const int64_t *, int64_t *,
                                                           const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<half>(size_t, const half *, const half *, half *, const uint32_t &,
                                                        cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<float>(size_t, const float *, const float *, float *,
                                                         const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalHeaviside<double>(size_t, const double *, const double *, double *,
                                                          const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<uint8_t>(const std::vector<size_t> &,
                                                                 const std::vector<size_t> &,
                                                                 const std::vector<size_t> &, const uint8_t *,
                                                                 const uint8_t *, uint8_t *, const uint32_t &,
                                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<uint16_t>(const std::vector<size_t> &,
                                                                  const std::vector<size_t> &,
                                                                  const std::vector<size_t> &, const uint16_t *,
                                                                  const uint16_t *, uint16_t *, const uint32_t &,
                                                                  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<uint32_t>(const std::vector<size_t> &,
                                                                  const std::vector<size_t> &,
                                                                  const std::vector<size_t> &, const uint32_t *,
                                                                  const uint32_t *, uint32_t *, const uint32_t &,
                                                                  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<uint64_t>(const std::vector<size_t> &,
                                                                  const std::vector<size_t> &,
                                                                  const std::vector<size_t> &, const uint64_t *,
                                                                  const uint64_t *, uint64_t *, const uint32_t &,
                                                                  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<int8_t>(const std::vector<size_t> &,
                                                                const std::vector<size_t> &,
                                                                const std::vector<size_t> &, const int8_t *,
                                                                const int8_t *, int8_t *, const uint32_t &,
                                                                cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<int16_t>(const std::vector<size_t> &,
                                                                 const std::vector<size_t> &,
                                                                 const std::vector<size_t> &, const int16_t *,
                                                                 const int16_t *, int16_t *, const uint32_t &,
                                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<int32_t>(const std::vector<size_t> &,
                                                                 const std::vector<size_t> &,
                                                                 const std::vector<size_t> &, const int32_t *,
                                                                 const int32_t *, int32_t *, const uint32_t &,
                                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<int64_t>(const std::vector<size_t> &,
                                                                 const std::vector<size_t> &,
                                                                 const std::vector<size_t> &, const int64_t *,
                                                                 const int64_t *, int64_t *, const uint32_t &,
                                                                 cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<half>(const std::vector<size_t> &, const std::vector<size_t> &,
                                                              const std::vector<size_t> &, const half *, const half *,
                                                              half *, const uint32_t &, cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<float>(const std::vector<size_t> &, const std::vector<size_t> &,
                                                               const std::vector<size_t> &, const float *,
                                                               const float *, float *, const uint32_t &,
                                                               cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t BroadcastHeaviside<double>(const std::vector<size_t> &,
                                                                const std::vector<size_t> &,
                                                                const std::vector<size_t> &, const double *,
                                                                const double *, double *, const uint32_t &,
                                                                cudaStream_t cuda_stream);
