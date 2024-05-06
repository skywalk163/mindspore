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
#ifndef NNACL_GROUP_NORM_PARAMETER_H_
#define NNACL_GROUP_NORM_PARAMETER_H_

#include "nnacl/op_base.h"
#include "nnacl/int8/quantize.h"
typedef struct GroupNormParameter {
  // Primitive parameter
  OpParameter op_parameter_;
  float epsilon_;
  int num_groups_;
  int channel_;
  int unit_;
  int batch_;
  bool affine_;
  void *mean_;
  void *variance_;
} GroupNormParameter;

typedef struct GroupNormQuantArg {
  int32_t in_zp_;
  int32_t out_zp_;
  double in_scale_;
  double out_scale_;
} GroupNormQuantArg;

#endif  // NNACL_GROUP_NORM_PARAMETER_H_
