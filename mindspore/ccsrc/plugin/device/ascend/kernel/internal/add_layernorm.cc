/**
 * Copyright 2023-2024 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/kernel/internal/add_layernorm.h"

#include <memory>

#include "plugin/device/ascend/kernel/internal/internal_kernel_utils.h"

namespace mindspore {
namespace kernel {
internal::OpParamPtr InternalAddLayerNorm::CreateOpParam(const std::vector<KernelTensor *> &inputs,
                                                         const std::vector<KernelTensor *> &outputs) {
  internal::OpParamPtr param_ptr = std::make_shared<internal::OpParam>();
  // setup param from inputs
  internal::AddLayerNormParam op_param;

  auto beginNormAxis = inputs[kIndex4]->GetValueWithCheck<int64_t>();
  auto beginParamsAxis = inputs[kIndex5]->GetValueWithCheck<int64_t>();
  if (beginNormAxis != -1 || beginParamsAxis != -1) {
    MS_LOG(EXCEPTION) << "beginNormAxis and beginParamsAxis must both be -1, but get beginNormAxis: '" << beginNormAxis
                      << " and beginParamsAxis: " << beginParamsAxis;
  }
  op_param.eps = inputs[kIndex6]->GetValueWithCheck<float>();

  param_ptr->specificParam = op_param;
  param_ptr->opId = internal::OpId::AddLayerNorm;
  return param_ptr;
}
void InternalAddLayerNorm::SetInOutIdx() {
  inputsIdxMap_[kIndex0] = kIndex0;
  inputsIdxMap_[kIndex1] = kIndex1;
  inputsIdxMap_[kIndex2] = kIndex2;
  inputsIdxMap_[kIndex3] = kIndex3;
  outputsIdxMap_[kIndex0] = kIndex0;
  outputsIdxMap_[kIndex1] = kIndex1;
  outputsIdxMap_[kIndex2] = kIndex2;
  outputsIdxMap_[kIndex3] = kIndex3;
}

MS_INTERNAL_KERNEL_FACTORY_REG(AddLayerNorm, InternalAddLayerNorm);
}  // namespace kernel
}  // namespace mindspore
