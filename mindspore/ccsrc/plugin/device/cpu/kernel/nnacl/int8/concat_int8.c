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

#include "nnacl/int8/concat_int8.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include "nnacl/concat_parameter.h"

void Int8Concat(int8_t **inputs, int8_t *output, const ConcatParameter *para, int axis, int64_t real_dst_count,
                int task_id, int input_num, int64_t count_unit, int64_t after_axis_size, int **input_shapes,
                const int32_t *output_shape) {
  float output_scale = para->quant_arg_.out_args_.scale_;
  const float output_inverse_scale = 1.f / output_scale;
  int out_copy_size = output_shape[axis] * after_axis_size;
  QuantArg *input_quant = para->quant_arg_.in_args_;
  int output_zp = para->quant_arg_.out_args_.zp_;
  int8_t max_int8 = para->quant_arg_.output_activation_max_;
  int8_t min_int8 = para->quant_arg_.output_activation_min_;
  int64_t start = task_id * count_unit;
  int64_t end = start + real_dst_count;
  output += start * out_copy_size;

  for (int k = start; k < end; k++) {
    for (int i = 0; i < input_num; i++) {
      const int32_t *input_shape = input_shapes[i];
      int64_t in_copy_size = input_shape[axis] * after_axis_size;
      const int8_t *input_ptr = inputs[i] + k * in_copy_size;
      if (fabs(input_quant[i].scale_ - output_scale) <= FLT_EPSILON && input_quant[i].zp_ == output_zp) {
        memcpy(output, input_ptr, in_copy_size);
      } else {
        float scale = input_quant[i].scale_ * output_inverse_scale;
        float bias = -input_quant[i].zp_ * scale;
        for (int j = 0; j < in_copy_size; j++) {
          int32_t output_tmp = round(input_ptr[j] * scale + bias) + output_zp;
          output_tmp = output_tmp > min_int8 ? output_tmp : min_int8;
          output_tmp = output_tmp < max_int8 ? output_tmp : max_int8;
          output[j] = (int8_t)output_tmp;
        }
      }
      output += in_copy_size;
    }
  }
}
