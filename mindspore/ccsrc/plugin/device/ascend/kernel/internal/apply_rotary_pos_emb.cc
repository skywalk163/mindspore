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

#include "plugin/device/ascend/kernel/internal/apply_rotary_pos_emb.h"

#include <memory>

#include "plugin/device/ascend/kernel/internal/internal_kernel_utils.h"

namespace mindspore {
namespace kernel {
internal::OpParamPtr ApplyRotaryPosEmb::CreateOpParam(const std::vector<KernelTensor *> &inputs,
                                                      const std::vector<KernelTensor *> &outputs) {
  internal::OpParamPtr param_ptr = std::make_shared<internal::OpParam>();
  param_ptr->opId = internal::OpId::ApplyRotaryPosEmb;

  internal::ApplyRotaryPosEmbParam ropeParam;
  auto last_input = inputs.at(kIndex5);
  if (last_input->dtype_id() == TypeId::kNumberTypeInt64) {
    ropeParam.cosFormat = static_cast<int32_t>(last_input->GetValue<int64_t>().value());
  } else {
    MS_LOG(EXCEPTION) << "ApplyRotaryPosEmb input[5] dtype is not kNumberTypeInt64";
  }

  // setup rope param from inputs
  param_ptr->specificParam = ropeParam;
  return param_ptr;
}
void ApplyRotaryPosEmb::SetInOutIdx() {
  inputsIdxMap_[kIndex0] = kIndex0;
  inputsIdxMap_[kIndex1] = kIndex1;
  inputsIdxMap_[kIndex2] = kIndex2;
  inputsIdxMap_[kIndex3] = kIndex3;
  inputsIdxMap_[kIndex4] = kIndex4;
  outputsIdxMap_[kIndex0] = kIndex0;
  outputsIdxMap_[kIndex1] = kIndex1;
}

MS_INTERNAL_KERNEL_FACTORY_REG(ApplyRotaryPosEmb, ApplyRotaryPosEmb);
}  // namespace kernel
}  // namespace mindspore
