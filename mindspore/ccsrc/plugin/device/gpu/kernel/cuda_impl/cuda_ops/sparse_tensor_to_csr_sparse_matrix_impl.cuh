/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_SPARSE_TENSOR_TO_CSR_SPARSE_MATRIX_IMPL_CUH_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_SPARSE_TENSOR_TO_CSR_SPARSE_MATRIX_IMPL_CUH_
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_device_info.h"

template <typename IndiceType>
CUDA_LIB_EXPORT cudaError_t SparseTensorToCSRSparseMatrix(const IndiceType *x_indices_ptr,
                                                          IndiceType *out_row_indices_ptr,
                                                          IndiceType *out_col_indices_ptr,
                                                          IndiceType *out_batch_pointers_ptr, int total_num, int rank,
                                                          cudaStream_t cuda_stream, const uint32_t device_id);

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_SPARSE_TENSOR_TO_CSR_SPARSE_MATRIX_IMPL_CUH_
