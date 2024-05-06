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

#ifndef MINDSPORE_CORE_OPS_TENSOR_SCATTER_ADD_H_
#define MINDSPORE_CORE_OPS_TENSOR_SCATTER_ADD_H_
#include <memory>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameTensorScatterAdd = "TensorScatterAdd";
/// \brief Creates a new tensor by adding the values from the positions in input_x indicated by indices, with values
/// from updates. Refer to Python API @ref mindspore.ops.TensorScatterAdd for more details.
class MIND_API TensorScatterAdd : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(TensorScatterAdd);
  /// \brief Constructor.
  TensorScatterAdd() : BaseOperator(kNameTensorScatterAdd) { InitIOName({"input_x", "indices", "updates"}, {"y"}); }
};

using kPrimTensorScatterAddPtr = std::shared_ptr<TensorScatterAdd>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_TENSOR_SCATTER_ADD_H_
