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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_UTILS_TRAIN_UTILS_H_
#define MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_UTILS_TRAIN_UTILS_H_

#include "coder/opcoders/op_coder.h"

namespace mindspore::lite::micro {
bool IsLossCoder(const OperatorCoder *code);
bool IsGradCoder(const OperatorCoder *coder);
bool IsOptimizer(const OperatorCoder *coder);
bool IsMaskOutput(const OperatorCoder *coder);
}  // namespace mindspore::lite::micro
#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_MICRO_CODER_UTILS_TRAIN_UTILS_H_
