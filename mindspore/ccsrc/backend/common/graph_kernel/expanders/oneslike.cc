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

#include "backend/common/graph_kernel/expanders/op_desc_registry.h"

namespace mindspore::graphkernel::expanders {
class OnesLike : public OpDesc {
 public:
  OnesLike() {}
  ~OnesLike() = default;

 protected:
  bool CheckInputs() override {
    auto x_shape = inputs_info_[0].shape;
    if (IsDynamic(x_shape)) {
      MS_LOG(DEBUG) << "Skip dynamic shape case";
      return false;
    }
    return true;
  }

  NodePtrList Expand(const NodePtrList &inputs) override {
    const auto &input_x = inputs[0];
    auto const_one = gb.Tensor(1, input_x->type);
    auto result = gb.BroadcastTo(const_one, input_x->shape);
    return {result};
  }
};
EXPANDER_OP_DESC_REGISTER("OnesLike", OnesLike);
}  // namespace mindspore::graphkernel::expanders
