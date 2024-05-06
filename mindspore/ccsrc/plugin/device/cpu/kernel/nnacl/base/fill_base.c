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

#include "nnacl/base/fill_base.h"
#include "nnacl/fill_base_simd.h"

int FillFp32(float *output, int size, float data) {
  if (output == NULL) {
    return NNACL_NULL_PTR;
  }

  int index = 0;

  SIMD_RUN_NO_SCALAR(FillFp32, index, output, size, data);

  for (; index < size; ++index) {
    output[index] = data;
  }
  return NNACL_OK;
}

int FillInt32(int *output, int size, int data) {
  if (output == NULL) {
    return NNACL_NULL_PTR;
  }

  int index = 0;

  SIMD_RUN_NO_SCALAR(FillInt32, index, output, size, data);

  for (; index < size; ++index) {
    output[index] = data;
  }
  return NNACL_OK;
}

int FillBool(bool *output, int size, bool data) {
  if (output == NULL) {
    return NNACL_NULL_PTR;
  }

  for (int index = 0; index < size; ++index) {
    output[index] = data;
  }
  return NNACL_OK;
}
