/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_KERNEL_GPU_CUDA_IMP_RL_BUFFER_IMPL_H_
#define MINDSPORE_CCSRC_KERNEL_GPU_CUDA_IMP_RL_BUFFER_IMPL_H_
#include <curand_kernel.h>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_common.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cuda_device_info.h"
CUDA_LIB_EXPORT cudaError_t BufferAppend(const int64_t capacity, const size_t size, const int *index,
                                         const int exp_batch, unsigned char *buffer, const unsigned char *exp,
                                         cudaStream_t cuda_stream);
CUDA_LIB_EXPORT cudaError_t IncreaseCount(const int64_t capacity, const int exp_batch, int *count, int *head,
                                          int *index, cudaStream_t cuda_stream);
CUDA_LIB_EXPORT cudaError_t ReMappingIndex(const int *count, const int *head, const int *origin_index, int *index,
                                           cudaStream_t cuda_stream);
CUDA_LIB_EXPORT cudaError_t BufferGetItem(const size_t size, const int *index, const size_t one_exp_len,
                                          const unsigned char *buffer, unsigned char *out, cudaStream_t cuda_stream);
CUDA_LIB_EXPORT cudaError_t CheckBatchSize(const int *count, const int *head, const size_t batch_size,
                                           const int64_t capacity, cudaStream_t cuda_stream);
CUDA_LIB_EXPORT cudaError_t BufferSample(const size_t size, const size_t one_element, const unsigned int *index,
                                         const unsigned char *buffer, unsigned char *out, cudaStream_t cuda_stream);
CUDA_LIB_EXPORT cudaError_t RandomGen(const int size, curandState *globalState, unsigned int *value, unsigned int *key,
                                      cudaStream_t stream);
CUDA_LIB_EXPORT cudaError_t RandInit(const int size, const int seed, curandState *state, cudaStream_t stream);

template <typename T>
CUDA_LIB_EXPORT cudaError_t RandomGenUniform(const int size, curandState *globalState, const int up_bound, T *indexes,
                                             cudaStream_t cuda_stream);
#endif  // MINDSPORE_CCSRC_KERNEL_GPU_CUDA_IMP_ADAM_IMPL_H_
