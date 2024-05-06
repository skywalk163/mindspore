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

#include "nnacl/fp32/cumsum_fp32.h"
#include "nnacl/op_base.h"
#include "nnacl/cumsum_fp32_simd.h"

// (a, b, c) -> (a, a+b, a+b+c)  exclusive == false
// (a, b, c) -> (0, a,   a+b)    exclusive == true
void Cumsum(const float *input, float *output, int out_dim, int axis_dim, int inner_dim, bool exclusive) {
  // when not exclusive, output axis dim[0] is the same as that of input.
  // when exclusive, output axis dim[0] is 0.0f
  if (!exclusive) {
    for (int i = 0; i < out_dim; ++i) {
      const float *layer_input = input + i * axis_dim * inner_dim;
      float *layer_output = output + i * axis_dim * inner_dim;

      int j = 0;
      SIMD_RUN_NO_SCALAR(CumsumOutputInitWithInput, j, layer_input, layer_output, inner_dim);
      for (; j < inner_dim; ++j) {
        *(layer_output + j) = *(layer_input + j);
      }
    }
  } else {
    for (int i = 0; i < out_dim; ++i) {
      float *layer_output = output + i * axis_dim * inner_dim;

      int j = 0;
      SIMD_RUN_NO_SCALAR(CumsumOutputInitWithZero, j, layer_output, inner_dim);
      for (; j < inner_dim; ++j) {
        *(layer_output + j) = 0.0f;
      }
    }
  }
  int input_offset = exclusive ? 0 : 1;
  for (int i = 0; i < out_dim; ++i) {
    const float *layer_input = input + i * axis_dim * inner_dim + inner_dim * input_offset;
    float *layer_last_output = output + i * axis_dim * inner_dim;
    float *layer_output = layer_last_output + inner_dim;

    for (int j = 1; j < axis_dim; ++j) {
      int k = 0;
      SIMD_RUN_NO_SCALAR(Cumsum, k, layer_input, layer_output, layer_last_output, inner_dim);
      for (; k < inner_dim; ++k) {
        // layer_output (i, j, k) = layer_input (i, j, k) + layer_last_output (i,j-1, k)
        *(layer_output + k) = *(layer_input + k) + *(layer_last_output + k);
      }
      layer_input += inner_dim;
      layer_last_output += inner_dim;
      layer_output += inner_dim;
    }
  }
}

// (a, b, c) -> (c+b+a, c+b, c) exclusive==false
// (a, b, c) -> (c+b, c, 0) exclusive==true
void CumsumReverse(const float *input, float *output, int out_dim, int axis_dim, int inner_dim, bool exclusive) {
  if (!exclusive) {
    for (int i = 0; i < out_dim; ++i) {
      const float *layer_input = input + i * axis_dim * inner_dim + (axis_dim - 1) * inner_dim;
      float *layer_output = output + i * axis_dim * inner_dim + (axis_dim - 1) * inner_dim;

      int j = 0;
      SIMD_RUN_NO_SCALAR(CumsumOutputInitWithInput, j, layer_input, layer_output, inner_dim);
      for (; j < inner_dim; ++j) {
        *(layer_output + j) = *(layer_input + j);
      }
    }
  } else {
    for (int i = 0; i < out_dim; ++i) {
      float *layer_output = output + i * axis_dim * inner_dim + (axis_dim - 1) * inner_dim;

      int j = 0;
      SIMD_RUN_NO_SCALAR(CumsumOutputInitWithZero, j, layer_output, inner_dim);
      for (; j < inner_dim; ++j) {
        *(layer_output + j) = 0.0f;
      }
    }
  }
  int input_offset = exclusive ? 0 : 1;
  for (int i = 0; i < out_dim; ++i) {
    const float *layer_input = input + (i + 1) * axis_dim * inner_dim - 1 - input_offset * inner_dim;
    float *layer_last_output = output + (i + 1) * axis_dim * inner_dim - 1;
    float *layer_output = layer_last_output - inner_dim;

    for (int j = 1; j < axis_dim; ++j) {
      int k = 0;
      SIMD_RUN_NO_SCALAR(CumsumReverse, k, layer_input, layer_output, layer_last_output, inner_dim);
      for (; k < inner_dim; ++k) {
        *(layer_output - k) = *(layer_input - k) + *(layer_last_output - k);
      }
      layer_input -= inner_dim;
      layer_last_output -= inner_dim;
      layer_output -= inner_dim;
    }
  }
}

// (a, b, c) -> (a, a+b, a+b+c)  exclusive == false
// (a, b, c) -> (0, a,   a+b)    exclusive == true
void CumsumInt(const int32_t *input, int32_t *output, int out_dim, int axis_dim, int inner_dim, bool exclusive) {
  // when not exclusive, output axis dim[0] is the same as that of input.
  // when exclusive, output axis dim[0] is 0
  if (!exclusive) {
    for (int i = 0; i < out_dim; ++i) {
      const int32_t *layer_input = input + i * axis_dim * inner_dim;
      int32_t *layer_output = output + i * axis_dim * inner_dim;

      int j = 0;
      SIMD_RUN_NO_SCALAR(CumsumIntOutputInitWithInput, j, layer_input, layer_output, inner_dim);
      for (; j < inner_dim; ++j) {
        *(layer_output + j) = *(layer_input + j);
      }
    }
  } else {
    for (int i = 0; i < out_dim; ++i) {
      int32_t *layer_output = output + i * axis_dim * inner_dim;

      int j = 0;
      SIMD_RUN_NO_SCALAR(CumsumIntOutputInitWithZero, j, layer_output, inner_dim);
      for (; j < inner_dim; ++j) {
        *(layer_output++) = 0;
      }
    }
  }
  int input_offset = exclusive ? 0 : 1;
  for (int i = 0; i < out_dim; ++i) {
    const int32_t *layer_input = input + i * axis_dim * inner_dim + inner_dim * input_offset;
    int32_t *layer_last_output = output + i * axis_dim * inner_dim;
    int32_t *layer_output = layer_last_output + inner_dim;

    for (int j = 1; j < axis_dim; ++j) {
      int k = 0;
      SIMD_RUN_NO_SCALAR(CumsumInt, k, layer_input, layer_output, layer_last_output, inner_dim);
      for (; k < inner_dim; ++k) {
        *(layer_output + k) = *(layer_input + k) + *(layer_last_output + k);
      }
      layer_input += inner_dim;
      layer_last_output += inner_dim;
      layer_output += inner_dim;
    }
  }
}

// (a, b, c) -> (c+b+a, c+b, c) exclusive==false
// (a, b, c) -> (c+b, c, 0) exclusive==true
void CumsumReverseInt(const int32_t *input, int32_t *output, int out_dim, int axis_dim, int inner_dim, bool exclusive) {
  if (!exclusive) {
    for (int i = 0; i < out_dim; ++i) {
      const int32_t *layer_input = input + i * axis_dim * inner_dim + (axis_dim - 1) * inner_dim;
      int32_t *layer_output = output + i * axis_dim * inner_dim + (axis_dim - 1) * inner_dim;

      int j = 0;
      SIMD_RUN_NO_SCALAR(CumsumIntOutputInitWithInput, j, layer_input, layer_output, inner_dim);
      for (; j < inner_dim; ++j) {
        *(layer_output++) = *(layer_input++);
      }
    }
  } else {
    for (int i = 0; i < out_dim; ++i) {
      int32_t *layer_output = output + i * axis_dim * inner_dim + (axis_dim - 1) * inner_dim;

      int j = 0;
      SIMD_RUN_NO_SCALAR(CumsumIntOutputInitWithZero, j, layer_output, inner_dim);
      for (; j < inner_dim; ++j) {
        *(layer_output++) = 0.0f;
      }
    }
  }
  int input_offset = exclusive ? 0 : 1;
  for (int i = 0; i < out_dim; ++i) {
    const int32_t *layer_input = input + (i + 1) * axis_dim * inner_dim - 1 - input_offset * inner_dim;
    int32_t *layer_last_output = output + (i + 1) * axis_dim * inner_dim - 1;
    int32_t *layer_output = layer_last_output - inner_dim;

    for (int j = 1; j < axis_dim; ++j) {
      int k = 0;
      SIMD_RUN_NO_SCALAR(CumsumReverseInt, k, layer_input, layer_output, layer_last_output, inner_dim);
      for (; k < inner_dim; ++k) {
        *(layer_output - k) = *(layer_input - k) + *(layer_last_output - k);
      }
      layer_input -= inner_dim;
      layer_last_output -= inner_dim;
      layer_output -= inner_dim;
    }
  }
}
