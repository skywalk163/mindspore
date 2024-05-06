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

#ifndef NNACL_INT8_DYNAMIC_GATHER_INT8_H_
#define NNACL_INT8_DYNAMIC_GATHER_INT8_H_

#include "nnacl/op_base.h"
#include "nnacl/int8/quantize.h"

#ifdef __cplusplus
extern "C" {
#endif
void DynamicGather(const int8_t *input, int outer_size, int inner_size, int limit, const int32_t *indices,
                   int indices_element_size, float *output, const float *scale_in, const int32_t *zp_in);
void DynamicGatherArm64(const int8_t *src, float *output, int count_16, int zp, float scale);

#ifdef ENABLE_FP16
void DynamicGatherForFp16(const int8_t *input, int outer_size, int inner_size, int limit, const int32_t *indices,
                          int indices_element_size, float16_t *output, const float *scale_in, const int32_t *zp_in);
void DynamicGatherArm64ForFp16(const int8_t *src, float16_t *output, int count_16, int zp, float scale);
#endif

#ifdef __cplusplus
}
#endif

#endif  // NNACL_INT8_DYNAMIC_GATHER_INT8_H_
