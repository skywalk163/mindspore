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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_SSPADDMM_IMPL_CUH_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_SSPADDMM_IMPL_CUH_
#include <vector>
#include <unordered_map>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_device_info.h"

template <typename T, typename S>
CUDA_LIB_EXPORT cudaError_t CalSparseAddSparse(const S *input_indices, const T *input_values,
                                               const int64_t input_values_num, int64_t *y_indices, T *y_values,
                                               const int64_t y_values_num, const T *beta, const uint32_t &device_id,
                                               cudaStream_t cuda_stream);

template <typename T, typename S>
CUDA_LIB_EXPORT cudaError_t CalSparseMulDense(const S *mat1_indices, const T *mat1_values,
                                              const int64_t mat1_values_num, const T *mat2, int64_t *y_indices,
                                              T *y_values, const int64_t y_values_num, const int64_t mat2_col,
                                              const int64_t input_values_num, const T *alpha, int64_t *index,
                                              const uint32_t &device_id, cudaStream_t cuda_stream);

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_SSPADDMM_IMPL_CUH_
