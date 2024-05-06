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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_SPARSE_SPARSE_MAXIMUM_IMPL_CUH_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_SPARSE_SPARSE_MAXIMUM_IMPL_CUH_
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_device_info.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_common.h"

template <typename T, typename S>
CUDA_LIB_EXPORT cudaError_t SparseSparseMaximum(const T *a_indices, const S *a_values, const T *b_indices,
                                                const S *b_values, T *sum_indices, S *sum_values,
                                                int64_t *ab_status_ptr, int64_t *sum_ptr, const int64_t a_indices_num,
                                                const int64_t b_indices_num, const int64_t rank_1,
                                                cudaStream_t cuda_stream, const uint32_t &device_id,
                                                int64_t *ab_status_ptr1, int64_t *ab_status_ptr2);
#endif
