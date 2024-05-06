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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_COALESCE_IMPL_CUH_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_CUDA_IMPL_CUDA_OPS_COALESCE_IMPL_CUH_
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_device_info.h"
template <typename T>
CUDA_LIB_EXPORT cudaError_t Coalesce(int64_t *origin_indices, int64_t *unique_indices, const size_t shape_elements,
                                     const size_t indices_num, const size_t values_num, int *ret_flag_host,
                                     int64_t *flatten_input_indices, const int64_t *input_indices,
                                     const T *input_values, const int64_t *input_shape, int64_t *output_indices,
                                     T *output_value, int64_t *output_shape, const uint32_t &device_id,
                                     cudaStream_t cuda_stream, int *output_size);
#endif
