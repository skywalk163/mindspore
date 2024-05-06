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
#ifndef MINDSPORE_CORE_OPS_RELU_V3_H_
#define MINDSPORE_CORE_OPS_RELU_V3_H_
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameReLUV3 = "ReLUV3";
/// \brief Computes ReLUV3 (Rectified Linear Unit activation function) of input tensors element-wise.
/// Refer to Python API @ref mindspore.ops.ReLUV3 for more details.
class MIND_API ReLUV3 : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(ReLUV3);
  /// \brief Constructor.
  ReLUV3() : BaseOperator(kNameReLUV3) { InitIOName({"x"}, {"output"}); }
  /// \brief Init.
  void Init() const {}
};
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_RELU_V3_H_
