/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "nnacl/fp32/arithmetic_self_fp32.h"
#include "nnacl/arithmetic_self_fp32_simd.h"

int ElementAbs(const float *input, float *output, const int element_size) {
  int i = 0;

  // only avx512 support abs fp32 instruction
  SIMD_RUN_AVX512(ElementAbs, i, input, output, element_size);
  for (; i < element_size; i++) {
    output[i] = fabsf(input[i]);
  }
  return NNACL_OK;
}

int ElementAbsInt(const int32_t *input, int32_t *output, const int element_size) {
  int i = 0;

  // only avx512 support abs fp32 instruction
  SIMD_RUN_AVX512(ElementAbsInt, i, input, output, element_size);
  for (; i < element_size; i++) {
    output[i] = abs(input[i]);
  }
  return NNACL_OK;
}

// cos
int ElementCos(const float *input, float *output, const int element_size) {
  int i = 0;
  SIMD_RUN_X86_NO_SCALAR(ElementCos, i, input, output, element_size);
  for (; i < element_size; i++) {
    output[i] = cosf(input[i]);
  }
  return NNACL_OK;
}

// log:
int ElementLog(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_X86_NO_SCALAR(ElementLog, i, input, output, element_size);
  for (; i < element_size; i++) {
    if (MS_UNLIKELY(input[i] < 0)) {
      return NNACL_ERRCODE_LOG_NEGATIVE_OR_ZERO;
    }
    output[i] = logf(input[i]);
  }
  return NNACL_OK;
}

// log1p:
int ElementLog1p(const float *input, float *output, const int element_size) {
  for (int i = 0; i < element_size; i++) {
    if (MS_UNLIKELY(input[i] < -1.0f)) {
      return NNACL_ERRCODE_LOG_NEGATIVE_OR_ZERO;
    }
    output[i] = log1p(input[i]);
  }
  return NNACL_OK;
}

int ElementSquare(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_NO_SCALAR(ElementSquare, i, input, output, element_size);
  for (; i < element_size; i++) {
    output[i] = input[i] * input[i];
  }
  return NNACL_OK;
}

int ElementSqrt(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_NO_SCALAR(ElementSqrt, i, input, output, element_size);
  for (; i < element_size; i++) {
    if (MS_UNLIKELY(input[i] < 0)) {
      return NNACL_ERRCODE_SQRT_NEGATIVE;
    }
    output[i] = sqrtf(input[i]);
  }
  return NNACL_OK;
}

int ElementRsqrt(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_NO_SCALAR(ElementRsqrt, i, input, output, element_size);
  for (; i < element_size; i++) {
    if (MS_UNLIKELY(input[i] < 0)) {
      return NNACL_ERRCODE_RSQRT_NEGATIVE;
    }
    output[i] = 1.f / sqrtf(input[i]);
  }
  return NNACL_OK;
}

int ElementSin(const float *input, float *output, const int element_size) {
  for (int i = 0; i < element_size; i++) {
    output[i] = sinf(input[i]);
  }
  return NNACL_OK;
}

// logical_not:
int ElementLogicalNot(const float *input, float *output, const int element_size) {
  for (int i = 0; i < element_size; i++) {
    output[i] = (float)(!((bool)(input[i])));
  }
  return NNACL_OK;
}

// logical_not:
int ElementLogicalNotBool(const bool *input, bool *output, const int element_size) {
  for (int i = 0; i < element_size; i++) {
    output[i] = !input[i];
  }
  return NNACL_OK;
}

int ElementRound(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_AVX(ElementRound, i, input, output, element_size);
  SIMD_RUN_SSE(ElementRound, i, input, output, element_size);
  for (; i < element_size; i++) {
    output[i] = roundf(input[i]);
  }
  return NNACL_OK;
}

int ElementFloor(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_X86_NO_SCALAR(ElementFloor, i, input, output, element_size);
  for (; i < element_size; i++) {
    output[i] = floorf(input[i]);
  }
  return NNACL_OK;
}

int ElementCeil(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_X86_NO_SCALAR(ElementCeil, i, input, output, element_size);
  for (; i < element_size; ++i) {
    output[i] = ceilf(input[i]);
  }
  return NNACL_OK;
}

int ElementNegative(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_NO_SCALAR(ElementNegative, i, input, output, element_size);
  for (; i < element_size; ++i) {
    output[i] = -input[i];
  }
  return NNACL_OK;
}

int ElementNegativeInt(const int32_t *input, int32_t *output, const int element_size) {
  int i = 0;

  SIMD_RUN_NO_SCALAR(ElementNegativeInt, i, input, output, element_size);
  for (; i < element_size; ++i) {
    output[i] = -input[i];
  }
  return NNACL_OK;
}

int ElementReciprocal(const float *input, float *output, const int element_size) {
  int i = 0;

  SIMD_RUN_NO_SCALAR(ElementReciprocal, i, input, output, element_size);
  for (; i < element_size; ++i) {
    if (input[i] == 0.0f) {
      return NNACL_ERR;
    }
    output[i] = 1.f / input[i];
  }
  return NNACL_OK;
}

// Erf
int ElementErf(const float *input, float *output, const int element_size) {
  for (int i = 0; i < element_size; i++) {
    output[i] = erff(input[i]);
  }
  return NNACL_OK;
}

int ElementIsFinite(const float *input, bool *output, const int element_size) {
  for (int i = 0; i < element_size; i++) {
    output[i] = true;
    if (isnan(input[i]) || isinf(input[i])) {
      output[i] = false;
    }
  }
  return NNACL_OK;
}

int ElementMish(const float *input, float *output, const int element_size) {
  int i = 0;
  SIMD_RUN_NO_SCALAR(ElementMish, i, input, output, element_size);

  for (; i < element_size; ++i) {
    simd_exp32(input[i], output + i);
    float exp_pow = (output[i] + 1) * (output[i] + 1);
    output[i] = input[i] * (exp_pow - 1) / (exp_pow + 1);
  }
  return NNACL_OK;
}
