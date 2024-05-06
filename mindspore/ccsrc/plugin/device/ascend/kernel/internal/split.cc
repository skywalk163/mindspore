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

#include "plugin/device/ascend/kernel/internal/split.h"

#include <memory>

#include "plugin/device/ascend/kernel/internal/internal_kernel_utils.h"

namespace mindspore {
namespace kernel {
internal::OpParamPtr InternalSplit::CreateOpParam(const std::vector<KernelTensor *> &inputs,
                                                  const std::vector<KernelTensor *> &outputs) {
  internal::OpParamPtr param_ptr = std::make_shared<internal::OpParam>();
  internal::SplitParam split_param;
  param_ptr->opId = internal::OpId::Split;

  split_param.splitDim = inputs[kIndex1]->GetValueWithCheck<int64_t>();
  split_param.splitNum = inputs[kIndex2]->GetValueWithCheck<int64_t>();

  param_ptr->specificParam = split_param;
  return param_ptr;
}
void InternalSplit::SetInOutIdx() {
  auto value_str = primitive_->GetAttr("size_splits");
  MS_EXCEPTION_IF_NULL(value_str);
  auto size_splits = GetValue<std::vector<int64_t>>(value_str);
  size_t split_num = size_splits.size();
  inputsIdxMap_[kIndex0] = kIndex0;
  for (size_t i = 0; i < split_num; i++) {
    outputsIdxMap_[i] = i;
  }
}

MS_INTERNAL_KERNEL_FACTORY_REG(Split, InternalSplit);
}  // namespace kernel
}  // namespace mindspore
