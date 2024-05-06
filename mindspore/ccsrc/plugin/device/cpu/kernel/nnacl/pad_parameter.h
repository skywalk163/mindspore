/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#ifndef NNACL_PAD_PARAMETER_H_
#define NNACL_PAD_PARAMETER_H_

#include "nnacl/op_base.h"

#define MAX_PAD_SIZE 12
#define DEFAULT_PAD_NDIMS 6

typedef struct PadQuantArg {
  QuantArg *in_quant_args_;
  QuantArg *out_quanr_args_;
  int8_t *constant_value_;
} PadQuantArg;

typedef struct PadParameter {
  OpParameter op_parameter_;
  int paddings_[MAX_PAD_SIZE];
  int pad_mode_;
  float constant_value_;
  int padding_length;
} PadParameter;

#endif  // NNACL_PAD_PARAMETER_H_
