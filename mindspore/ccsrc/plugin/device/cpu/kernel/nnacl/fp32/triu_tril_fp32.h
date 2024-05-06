/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_NNACL_FP32_TRIU_TRIL_H_
#define MINDSPORE_NNACL_FP32_TRIU_TRIL_H_

#include "nnacl/op_base.h"
#include "nnacl/kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

int TriuTrilGetCalculateNum(KernelBase *self, int64_t *mul, int64_t *height, int64_t *width);
int TriuTrilGetKValue(KernelBase *self, int64_t *k);

void TriuByte8(const void *src, void *dst, int64_t k, int64_t height, int64_t width, int64_t out_elems);
void TriuByte4(const void *src, void *dst, int64_t k, int64_t height, int64_t width, int64_t out_elems);
void TriuByte2(const void *src, void *dst, int64_t k, int64_t height, int64_t width, int64_t out_elems);
void TriuByte1(const void *src, void *dst, int64_t k, int64_t height, int64_t width, int64_t out_elems);

void TrilByte8(const void *src, void *dst, int64_t k, int64_t height, int64_t width, int64_t out_elems);
void TrilByte4(const void *src, void *dst, int64_t k, int64_t height, int64_t width, int64_t out_elems);
void TrilByte2(const void *src, void *dst, int64_t k, int64_t height, int64_t width, int64_t out_elems);
void TrilByte1(const void *src, void *dst, int64_t k, int64_t height, int64_t width, int64_t out_elems);

#ifdef __cplusplus
}
#endif
#endif  // MINDSPORE_NNACL_FP32_TRIU_TRIL_H_
