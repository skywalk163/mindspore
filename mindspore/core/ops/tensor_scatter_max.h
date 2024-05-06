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

#ifndef MINDSPORE_CORE_OPS_TENSOR_SCATTER_MAX_H_
#define MINDSPORE_CORE_OPS_TENSOR_SCATTER_MAX_H_
#include <memory>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameTensorScatterMax = "TensorScatterMax";
/// \brief By comparing the value at the position indicated by the index in input_x with the value in the update, the
/// value at the index will eventually be equal to the largest one to create a new tensor. Refer to Python API @ref
/// mindspore.ops.TensorScatterMax for more details.
class MIND_API TensorScatterMax : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(TensorScatterMax);
  /// \brief Constructor.
  TensorScatterMax() : BaseOperator(kNameTensorScatterMax) { InitIOName({"input_x", "indices", "updates"}, {"y"}); }
};

using kPrimTensorScatterMaxPtr = std::shared_ptr<TensorScatterMax>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_TENSOR_SCATTER_MAX_H_
