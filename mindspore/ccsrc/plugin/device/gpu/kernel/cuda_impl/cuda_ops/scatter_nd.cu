/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/scatter_nd.cuh"
#include <complex>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/complex.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/util.cuh"

template <typename T, typename S>
__global__ void ScatterNdKernel(S *indices, T *update, T *output, const size_t block_size, const size_t input_size,
                                const size_t output_size, const size_t indices_dim_0, const size_t indices_dim_1,
                                const ScatterNdInfo<S> info) {
  const S *indices_stride = info.indices_stride;
  const S *work_shape = info.shape;
  int i, j;
  for (size_t read_index = blockIdx.x * blockDim.x + threadIdx.x; read_index < input_size;
       read_index += blockDim.x * gridDim.x) {
    size_t write_index = 0;
    bool out_bound = false;

    i = read_index / block_size;
    j = read_index % block_size;

    for (size_t k = 0; k < indices_dim_1; k++) {
      S indices_i = indices[i * indices_dim_1 + k];
      CUDA_KERNEL_ASSERT(indices_i >= 0 && indices_i < work_shape[k]);
      write_index += indices_i * indices_stride[k];
    }

    write_index += j;
    CUDA_KERNEL_ASSERT(write_index < output_size);

    MsAtomicAdd(&output[write_index], update[read_index]);
  }
}

template <typename T, typename S>
cudaError_t ScatterNd(S *indices, T *update, T *output, const size_t &block_size, const size_t &input_size,
                      const size_t &output_size, const size_t &indices_dim_0, const size_t &indices_dim_1,
                      const ScatterNdInfo<S> &info, cudaStream_t stream) {
  ScatterNdKernel<<<GET_BLOCKS(output_size), GET_THREADS, 0, stream>>>(indices, update, output, block_size, input_size,
                                                                       output_size, indices_dim_0, indices_dim_1, info);
  return GetCudaStatus();
}

template CUDA_LIB_EXPORT cudaError_t ScatterNd<double, int16_t>(int16_t *indices, double *update, double *output,
                                                                const size_t &block_size, const size_t &input_size,
                                                                const size_t &output_size, const size_t &indices_dim_0,
                                                                const size_t &indices_dim_1,
                                                                const ScatterNdInfo<int16_t> &info,
                                                                cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<double, int>(int *indices, double *update, double *output,
                                                            const size_t &block_size, const size_t &input_size,
                                                            const size_t &output_size, const size_t &indices_dim_0,
                                                            const size_t &indices_dim_1, const ScatterNdInfo<int> &info,
                                                            cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<double, int64_t>(int64_t *indices, double *update, double *output,
                                                                const size_t &block_size, const size_t &input_size,
                                                                const size_t &output_size, const size_t &indices_dim_0,
                                                                const size_t &indices_dim_1,
                                                                const ScatterNdInfo<int64_t> &info,
                                                                cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<float, int16_t>(int16_t *indices, float *update, float *output,
                                                               const size_t &block_size, const size_t &input_size,
                                                               const size_t &output_size, const size_t &indices_dim_0,
                                                               const size_t &indices_dim_1,
                                                               const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<float, int>(int *indices, float *update, float *output,
                                                           const size_t &block_size, const size_t &input_size,
                                                           const size_t &output_size, const size_t &indices_dim_0,
                                                           const size_t &indices_dim_1, const ScatterNdInfo<int> &info,
                                                           cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<float, int64_t>(int64_t *indices, float *update, float *output,
                                                               const size_t &block_size, const size_t &input_size,
                                                               const size_t &output_size, const size_t &indices_dim_0,
                                                               const size_t &indices_dim_1,
                                                               const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<half, int16_t>(int16_t *indices, half *update, half *output,
                                                              const size_t &block_size, const size_t &input_size,
                                                              const size_t &output_size, const size_t &indices_dim_0,
                                                              const size_t &indices_dim_1,
                                                              const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<half, int>(int *indices, half *update, half *output,
                                                          const size_t &block_size, const size_t &input_size,
                                                          const size_t &output_size, const size_t &indices_dim_0,
                                                          const size_t &indices_dim_1, const ScatterNdInfo<int> &info,
                                                          cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<half, int64_t>(int64_t *indices, half *update, half *output,
                                                              const size_t &block_size, const size_t &input_size,
                                                              const size_t &output_size, const size_t &indices_dim_0,
                                                              const size_t &indices_dim_1,
                                                              const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int64_t, int16_t>(int16_t *indices, int64_t *update, int64_t *output,
                                                                 const size_t &block_size, const size_t &input_size,
                                                                 const size_t &output_size, const size_t &indices_dim_0,
                                                                 const size_t &indices_dim_1,
                                                                 const ScatterNdInfo<int16_t> &info,
                                                                 cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int64_t, int>(int *indices, int64_t *update, int64_t *output,
                                                             const size_t &block_size, const size_t &input_size,
                                                             const size_t &output_size, const size_t &indices_dim_0,
                                                             const size_t &indices_dim_1,
                                                             const ScatterNdInfo<int> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int64_t, int64_t>(int64_t *indices, int64_t *update, int64_t *output,
                                                                 const size_t &block_size, const size_t &input_size,
                                                                 const size_t &output_size, const size_t &indices_dim_0,
                                                                 const size_t &indices_dim_1,
                                                                 const ScatterNdInfo<int64_t> &info,
                                                                 cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int, int16_t>(int16_t *indices, int *update, int *output,
                                                             const size_t &block_size, const size_t &input_size,
                                                             const size_t &output_size, const size_t &indices_dim_0,
                                                             const size_t &indices_dim_1,
                                                             const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int, int>(int *indices, int *update, int *output,
                                                         const size_t &block_size, const size_t &input_size,
                                                         const size_t &output_size, const size_t &indices_dim_0,
                                                         const size_t &indices_dim_1, const ScatterNdInfo<int> &info,
                                                         cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int, int64_t>(int64_t *indices, int *update, int *output,
                                                             const size_t &block_size, const size_t &input_size,
                                                             const size_t &output_size, const size_t &indices_dim_0,
                                                             const size_t &indices_dim_1,
                                                             const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
// NOLINTNEXTLINE
template CUDA_LIB_EXPORT cudaError_t ScatterNd<short, int16_t>(int16_t *indices, short *update, short *output,
                                                               const size_t &block_size, const size_t &input_size,
                                                               const size_t &output_size, const size_t &indices_dim_0,
                                                               const size_t &indices_dim_1,
                                                               const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
// NOLINTNEXTLINE
template CUDA_LIB_EXPORT cudaError_t ScatterNd<short, int>(int *indices, short *update, short *output,
                                                           const size_t &block_size, const size_t &input_size,
                                                           const size_t &output_size, const size_t &indices_dim_0,
                                                           const size_t &indices_dim_1, const ScatterNdInfo<int> &info,
                                                           cudaStream_t stream);
// NOLINTNEXTLINE
template CUDA_LIB_EXPORT cudaError_t ScatterNd<short, int64_t>(int64_t *indices, short *update, short *output,
                                                               const size_t &block_size, const size_t &input_size,
                                                               const size_t &output_size, const size_t &indices_dim_0,
                                                               const size_t &indices_dim_1,
                                                               const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int8_t, int16_t>(int16_t *indices, int8_t *update, int8_t *output,
                                                                const size_t &block_size, const size_t &input_size,
                                                                const size_t &output_size, const size_t &indices_dim_0,
                                                                const size_t &indices_dim_1,
                                                                const ScatterNdInfo<int16_t> &info,
                                                                cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int8_t, int>(int *indices, int8_t *update, int8_t *output,
                                                            const size_t &block_size, const size_t &input_size,
                                                            const size_t &output_size, const size_t &indices_dim_0,
                                                            const size_t &indices_dim_1, const ScatterNdInfo<int> &info,
                                                            cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<int8_t, int64_t>(int64_t *indices, int8_t *update, int8_t *output,
                                                                const size_t &block_size, const size_t &input_size,
                                                                const size_t &output_size, const size_t &indices_dim_0,
                                                                const size_t &indices_dim_1,
                                                                const ScatterNdInfo<int64_t> &info,
                                                                cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<unsigned char, int16_t>(
  int16_t *indices, unsigned char *update, unsigned char *output, const size_t &block_size, const size_t &input_size,
  const size_t &output_size, const size_t &indices_dim_0, const size_t &indices_dim_1,
  const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<unsigned char, int>(int *indices, unsigned char *update,
                                                                   unsigned char *output, const size_t &block_size,
                                                                   const size_t &input_size, const size_t &output_size,
                                                                   const size_t &indices_dim_0,
                                                                   const size_t &indices_dim_1,
                                                                   const ScatterNdInfo<int> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<unsigned char, int64_t>(
  int64_t *indices, unsigned char *update, unsigned char *output, const size_t &block_size, const size_t &input_size,
  const size_t &output_size, const size_t &indices_dim_0, const size_t &indices_dim_1,
  const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t
ScatterNd<uint16_t, int16_t>(int16_t *indices, uint16_t *update, uint16_t *output, const size_t &block_size,
                             const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0,
                             const size_t &indices_dim_1, const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<uint16_t, int>(int *indices, uint16_t *update, uint16_t *output,
                                                              const size_t &block_size, const size_t &input_size,
                                                              const size_t &output_size, const size_t &indices_dim_0,
                                                              const size_t &indices_dim_1,
                                                              const ScatterNdInfo<int> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t
ScatterNd<uint16_t, int64_t>(int64_t *indices, uint16_t *update, uint16_t *output, const size_t &block_size,
                             const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0,
                             const size_t &indices_dim_1, const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t
ScatterNd<uint32_t, int16_t>(int16_t *indices, uint32_t *update, uint32_t *output, const size_t &block_size,
                             const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0,
                             const size_t &indices_dim_1, const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<uint32_t, int>(int *indices, uint32_t *update, uint32_t *output,
                                                              const size_t &block_size, const size_t &input_size,
                                                              const size_t &output_size, const size_t &indices_dim_0,
                                                              const size_t &indices_dim_1,
                                                              const ScatterNdInfo<int> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t
ScatterNd<uint32_t, int64_t>(int64_t *indices, uint32_t *update, uint32_t *output, const size_t &block_size,
                             const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0,
                             const size_t &indices_dim_1, const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t
ScatterNd<uint64_t, int16_t>(int16_t *indices, uint64_t *update, uint64_t *output, const size_t &block_size,
                             const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0,
                             const size_t &indices_dim_1, const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<uint64_t, int>(int *indices, uint64_t *update, uint64_t *output,
                                                              const size_t &block_size, const size_t &input_size,
                                                              const size_t &output_size, const size_t &indices_dim_0,
                                                              const size_t &indices_dim_1,
                                                              const ScatterNdInfo<int> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t
ScatterNd<uint64_t, int64_t>(int64_t *indices, uint64_t *update, uint64_t *output, const size_t &block_size,
                             const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0,
                             const size_t &indices_dim_1, const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<bool, int16_t>(int16_t *indices, bool *update, bool *output,
                                                              const size_t &block_size, const size_t &input_size,
                                                              const size_t &output_size, const size_t &indices_dim_0,
                                                              const size_t &indices_dim_1,
                                                              const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<bool, int>(int *indices, bool *update, bool *output,
                                                          const size_t &block_size, const size_t &input_size,
                                                          const size_t &output_size, const size_t &indices_dim_0,
                                                          const size_t &indices_dim_1, const ScatterNdInfo<int> &info,
                                                          cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<bool, int64_t>(int64_t *indices, bool *update, bool *output,
                                                              const size_t &block_size, const size_t &input_size,
                                                              const size_t &output_size, const size_t &indices_dim_0,
                                                              const size_t &indices_dim_1,
                                                              const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<Complex<float>, int16_t>(
  int16_t *indices, Complex<float> *update, Complex<float> *output, const size_t &block_size, const size_t &input_size,
  const size_t &output_size, const size_t &indices_dim_0, const size_t &indices_dim_1,
  const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t
ScatterNd<Complex<float>, int>(int *indices, Complex<float> *update, Complex<float> *output, const size_t &block_size,
                               const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0,
                               const size_t &indices_dim_1, const ScatterNdInfo<int> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<Complex<float>, int64_t>(
  int64_t *indices, Complex<float> *update, Complex<float> *output, const size_t &block_size, const size_t &input_size,
  const size_t &output_size, const size_t &indices_dim_0, const size_t &indices_dim_1,
  const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<Complex<double>, int16_t>(
  int16_t *indices, Complex<double> *update, Complex<double> *output, const size_t &block_size,
  const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0, const size_t &indices_dim_1,
  const ScatterNdInfo<int16_t> &info, cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<Complex<double>, int>(
  int *indices, Complex<double> *update, Complex<double> *output, const size_t &block_size, const size_t &input_size,
  const size_t &output_size, const size_t &indices_dim_0, const size_t &indices_dim_1, const ScatterNdInfo<int> &info,
  cudaStream_t stream);
template CUDA_LIB_EXPORT cudaError_t ScatterNd<Complex<double>, int64_t>(
  int64_t *indices, Complex<double> *update, Complex<double> *output, const size_t &block_size,
  const size_t &input_size, const size_t &output_size, const size_t &indices_dim_0, const size_t &indices_dim_1,
  const ScatterNdInfo<int64_t> &info, cudaStream_t stream);
