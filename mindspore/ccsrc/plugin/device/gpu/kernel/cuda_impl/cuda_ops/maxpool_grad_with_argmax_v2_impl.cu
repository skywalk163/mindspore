/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include <algorithm>
#include "maxpool_grad_with_argmax_v2_impl.cuh"
#include "include/cuda_fp16.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/util.cuh"

template <typename T, typename S>
__global__ void MaxPoolGradWithArgmaxV2(const T *dy, const S *index, const int64_t x_hw, const int64_t x_chw,
                                        const int64_t dy_hw, const int64_t dy_chw, const int64_t size, T *dx) {
  for (int64_t pos = blockIdx.x * blockDim.x + threadIdx.x; pos < size; pos += blockDim.x * gridDim.x) {
    const S idx = index[pos];
    const int64_t pos_n = pos / dy_chw;
    const int64_t pos_c = pos / dy_hw % (dy_chw / dy_hw);
    MsAtomicAdd(dx + pos_n * x_chw + pos_c * x_hw + idx, dy[pos]);
  }
  return;
}

template <typename T>
__global__ void InitOutput(const int size, T *output) {
  T zero = 0;
  for (size_t id = blockIdx.x * blockDim.x + threadIdx.x; id < size; id += blockDim.x * gridDim.x) {
    output[id] = zero;
  }
  return;
}

template <typename T, typename S>
cudaError_t CalMaxPoolGradWithArgmaxV2(const T *dy, const S *index, const int64_t x_hw, const int64_t x_chw,
                                       const int64_t x_nchw, const int64_t dy_hw, const int64_t dy_chw,
                                       const int64_t dy_nchw, T *dx, const uint32_t device_id,
                                       cudaStream_t cuda_stream) {
  InitOutput<<<GET_BLOCKS(x_nchw), GET_THREADS, 0, cuda_stream>>>(x_nchw, dx);
  MaxPoolGradWithArgmaxV2<<<GET_BLOCKS(dy_nchw), GET_THREADS, 0, cuda_stream>>>(dy, index, x_hw, x_chw, dy_hw, dy_chw,
                                                                                dy_nchw, dx);
  return GetCudaStatus();
}

template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<half, int32_t>(
  const half *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, half *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<float, int32_t>(
  const float *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, float *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<double, int32_t>(
  const double *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, double *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<int8_t, int32_t>(
  const int8_t *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, int8_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<int16_t, int32_t>(
  const int16_t *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, int16_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<int32_t, int32_t>(
  const int32_t *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, int32_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<int64_t, int32_t>(
  const int64_t *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, int64_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<uint8_t, int32_t>(
  const uint8_t *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, uint8_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<uint16_t, int32_t>(
  const uint16_t *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, uint16_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<uint32_t, int32_t>(
  const uint32_t *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, uint32_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<uint64_t, int32_t>(
  const uint64_t *dy, const int32_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, uint64_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);

template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<half, int64_t>(
  const half *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, half *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<float, int64_t>(
  const float *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, float *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<double, int64_t>(
  const double *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, double *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<int8_t, int64_t>(
  const int8_t *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, int8_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<int16_t, int64_t>(
  const int16_t *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, int16_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<int32_t, int64_t>(
  const int32_t *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, int32_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<int64_t, int64_t>(
  const int64_t *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, int64_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<uint8_t, int64_t>(
  const uint8_t *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, uint8_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<uint16_t, int64_t>(
  const uint16_t *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, uint16_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<uint32_t, int64_t>(
  const uint32_t *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, uint32_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
template CUDA_LIB_EXPORT cudaError_t CalMaxPoolGradWithArgmaxV2<uint64_t, int64_t>(
  const uint64_t *dy, const int64_t *index, const int64_t x_hw, const int64_t x_chw, const int64_t x_nchw,
  const int64_t dy_hw, const int64_t dy_chw, const int64_t dy_nchw, uint64_t *dx, const uint32_t device_id,
  cudaStream_t cuda_stream);
