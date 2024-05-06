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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_INPLACE_OPS_IMPL_CUH_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_INPLACE_OPS_IMPL_CUH_
#include <vector>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_device_info.h"

enum BroadcastOpType {
  INPLACE_OP_TYPE_UPDATE = 0,
  INPLACE_OP_TYPE_ADD = 1,
  INPLACE_OP_TYPE_SUB = 2,
};

template <typename T, typename S>
CUDA_LIB_EXPORT cudaError_t CalInplaceOp(const size_t size_v, const T *input_v, T *output, S *indices,
                                         S *indices_key_ptr, const int64_t first_dimension, const int64_t band_size,
                                         const uint32_t &device_id, int op_type, cudaStream_t cuda_stream);

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_INPLACE_UPDATE_IMPL_CUH_
