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

#ifndef MINDSPORE_CORE_OPS_GRAD_UPSAMPLE_TRILINEAR_3D_GRAD_H
#define MINDSPORE_CORE_OPS_GRAD_UPSAMPLE_TRILINEAR_3D_GRAD_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameUpsampleTrilinear3DGrad = "UpsampleTrilinear3DGrad";
class MIND_API UpsampleTrilinear3DGrad : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(UpsampleTrilinear3DGrad);
  UpsampleTrilinear3DGrad() : BaseOperator(kNameUpsampleTrilinear3DGrad) {
    InitIOName({"dy", "input_size", "output_size", "scales"}, {"dx"});
  }
  bool get_align_corners() const;
};
using PrimUpsampleTrilinear3DGrad = std::shared_ptr<UpsampleTrilinear3DGrad>;
}  // namespace ops
}  // namespace mindspore
#endif  // MINDSPORE_CORE_OPS_GRAD_UPSAMPLE_TRILINEAR_3D_GRAD_H
