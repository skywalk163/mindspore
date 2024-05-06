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

#include "backend/common/graph_kernel/expanders/utils.h"
#include "backend/common/graph_kernel/expanders/op_desc_registry.h"
#include "backend/common/graph_kernel/model/graph_builder.h"
#include "ir/anf.h"
#include "mindapi/base/shape_vector.h"
#include "utils/convert_utils_base.h"

namespace mindspore::graphkernel::expanders {
constexpr size_t kTwoDims = 2;
constexpr size_t kThreeDims = 3;
constexpr int OFFSET1 = 1;
constexpr int OFFSET2 = 4;

class BiasAddGrad : public OpDesc {
 public:
  BiasAddGrad() {
    auto support_format = std::make_unique<SupportFormat>();
    support_format->AddFormat({kOpFormat_DEFAULT});
    support_format->AddFormat({kOpFormat_NCHW});
    support_format->AddFormat({kOpFormat_NHWC});
    support_format->AddFormat({kOpFormat_FRAC_NZ});
    (void)validators_.emplace_back(std::move(support_format));
  }
  ~BiasAddGrad() = default;

 protected:
  bool CheckInputs() override {
    auto x_format = inputs_info_[0].format;
    auto x_shape = inputs_info_[0].shape;
    if (x_format == kOpFormat_FRAC_NZ && IsDynamic(x_shape)) {
      MS_LOG(DEBUG) << "Skip dynamic shape case";
      return false;
    }
    return true;
  }

  NodePtrList Expand(const NodePtrList &inputs) override {
    const auto &input_x = inputs[0];
    ShapeVector reduce_axis;
    if (input_x->format == kOpFormat_NHWC) {
      reduce_axis = {0, 1, 2};
    } else if (input_x->format == kOpFormat_NCHW) {
      reduce_axis = {0, 2, 3};
    } else if (input_x->format == kOpFormat_FRAC_NZ) {
      reduce_axis = {-2, -3};
    } else {  // kOpFormat_Default
      if (input_x->shape.size() == kTwoDims) {
        reduce_axis = {0};
      } else if (input_x->shape.size() == kThreeDims) {
        reduce_axis = {0, 1};
      } else {
        reduce_axis = {0, 2, 3};
      }
    }
    auto result = gb.ReduceSum(input_x, reduce_axis, false);
    if (input_x->format == kOpFormat_FRAC_NZ) {
      ShapeVector out_shape{input_x->shape.begin(), input_x->shape.end() - OFFSET2};
      // calculate the last dim of output shape.
      out_shape.push_back(*(input_x->shape.end() - OFFSET1) * *(input_x->shape.end() - OFFSET2));
      result = gb.Reshape(result, out_shape);
    }
    return {result};
  }
};
EXPANDER_OP_DESC_REGISTER("BiasAddGrad", BiasAddGrad);
}  // namespace mindspore::graphkernel::expanders
