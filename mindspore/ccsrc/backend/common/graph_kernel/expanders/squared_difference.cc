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
#include <vector>
#include "backend/common/graph_kernel/expanders/op_desc_registry.h"

namespace mindspore::graphkernel::expanders {
class SquaredDifference : public OpDesc {
 public:
  SquaredDifference() { (void)validators_.emplace_back(std::make_unique<CheckAllFormatsSame>()); }
  ~SquaredDifference() = default;

 protected:
  bool CheckInputs() override {
    auto dtype_x = inputs_info_[0].type;
    auto dtype_y = inputs_info_[1].type;
    if (dtype_x == TypeId::kNumberTypeFloat64 || dtype_y == TypeId::kNumberTypeFloat64) {
      MS_LOG(INFO) << "For 'SquaredDifference', the inputs data type must not be float64";
      return false;
    }
    if (dtype_x != dtype_y) {
      MS_LOG(INFO) << "For 'SquaredDifference', the inputs data type should be same, but got " << dtype_x << " and "
                   << dtype_y;
      return false;
    }
    return true;
  }

  NodePtrList Expand(const NodePtrList &inputs) override {
    const auto &x = inputs[0];
    const auto &y = inputs[1];
    auto sub_val = gb.Sub(x, y);
    auto result = gb.Mul(sub_val, sub_val);
    return {result};
  }
};
EXPANDER_OP_DESC_REGISTER("SquaredDifference", SquaredDifference);
}  // namespace mindspore::graphkernel::expanders
