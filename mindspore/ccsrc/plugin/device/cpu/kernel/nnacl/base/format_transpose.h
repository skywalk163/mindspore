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
#ifndef NNACL_FORMAT_TRANSPOSE_H_
#define NNACL_FORMAT_TRANSPOSE_H_

#include "nnacl/op_base.h"

#ifdef __cplusplus
extern "C" {
#endif
int TransData(void *src_data, void *dst_data, const FormatC src_format, const FormatC dst_format, TypeIdC data_type,
              const int batch, const int channel, const int plane);
#ifdef __cplusplus
}
#endif

#endif  // NNACL_FILL_BASE_H_
