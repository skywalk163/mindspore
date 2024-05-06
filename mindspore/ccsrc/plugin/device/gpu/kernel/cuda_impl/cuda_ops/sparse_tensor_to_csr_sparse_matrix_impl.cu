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

#include "sparse_tensor_to_csr_sparse_matrix_impl.cuh"
#include <algorithm>
#include <iostream>
#include "plugin/device/cpu/kernel/nnacl/op_base.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/util.cuh"

template <typename IndiceType>
__global__ void SparseTensorToCSRSparseMatrixKernel(const IndiceType *x_indices_ptr, IndiceType *out_row_indices_ptr,
                                                    IndiceType *out_col_indices_ptr, IndiceType *out_batch_pointers_ptr,
                                                    int total_num, int rank) {
  for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < total_num; i += blockDim.x * gridDim.x) {
    out_row_indices_ptr[i] = x_indices_ptr[i * rank + rank - 2];
    out_col_indices_ptr[i] = x_indices_ptr[i * rank + rank - 1];
    if (rank == 3) {
      IndiceType batch = x_indices_ptr[i * rank];
      MsAtomicMax(out_batch_pointers_ptr + batch + 1, i + 1);
    } else {
      MsAtomicMax(out_batch_pointers_ptr + 1, i + 1);
    }
  }
}

template <typename IndiceType>
CUDA_LIB_EXPORT cudaError_t SparseTensorToCSRSparseMatrix(const IndiceType *x_indices_ptr,
                                                          IndiceType *out_row_indices_ptr,
                                                          IndiceType *out_col_indices_ptr,
                                                          IndiceType *out_batch_pointers_ptr, int total_num, int rank,
                                                          cudaStream_t cuda_stream, const uint32_t device_id) {
  SparseTensorToCSRSparseMatrixKernel<<<CUDA_BLOCKS(device_id, total_num), CUDA_THREADS(device_id), 0, cuda_stream>>>(
    x_indices_ptr, out_row_indices_ptr, out_col_indices_ptr, out_batch_pointers_ptr, total_num, rank);
  return GetCudaStatus();
}

template CUDA_LIB_EXPORT cudaError_t SparseTensorToCSRSparseMatrix<int32_t>(
  const int32_t *x_indices_ptr, int32_t *out_row_indices_ptr, int32_t *out_col_indices_ptr,
  int32_t *out_batch_pointers_ptr, int total_num, int rank, cudaStream_t cuda_stream, const uint32_t device_id);
