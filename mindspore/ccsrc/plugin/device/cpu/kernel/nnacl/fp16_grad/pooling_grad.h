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

#ifndef NNACL_FP16_GRAD_POOLING_GRAD_H_
#define NNACL_FP16_GRAD_POOLING_GRAD_H_

#include "nnacl/fp16/pooling_fp16.h"
#include "nnacl/kernel/pooling.h"

#ifdef __cplusplus
extern "C" {
#endif
void AvgPoolingFp16Grad(const float16_t *input_ptr, float16_t *output_ptr, int count, PoolingParameter *pooling_param,
                        const PoolingComputeParam *pooling_args);
void MaxPoolingFp16Grad(const float16_t *input_ptr, const float16_t *dy_ptr, float16_t *output_ptr, int output_batch,
                        PoolingParameter *pooling_param, const PoolingComputeParam *pooling_args);
#ifdef __cplusplus
}
#endif

#endif  // NNACL_FP16_GRAD_POOLING_GRAD_H_
