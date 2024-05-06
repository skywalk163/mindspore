/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include <memory>
#include "backend/common/graph_kernel/expanders/op_desc_registry.h"

namespace mindspore::graphkernel::expanders {
class TanhGrad : public OpDesc {
 public:
  TanhGrad() { validators_.push_back(std::make_unique<CheckAllFormatsSame>()); }
  ~TanhGrad() = default;

 protected:
  NodePtrList Expand(const NodePtrList &inputs) override {
    const auto &input_y = inputs[0];
    const auto &input_dy = inputs[1];

    auto const_one = gb.Tensor(1, input_y->type);
    auto double_y = gb.Mul(input_y, input_y);
    auto one_sub_double_y = gb.Sub(const_one, double_y);
    auto result = gb.Mul(input_dy, one_sub_double_y);
    return {result};
  }
};
EXPANDER_OP_DESC_REGISTER("TanhGrad", TanhGrad);
}  // namespace mindspore::graphkernel::expanders
