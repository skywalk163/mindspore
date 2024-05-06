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

#ifndef MINDSPORE_CORE_OPS_SPARSE_SOFTMAX_CROSS_ENTROPY_WITH_LOGITS_V2_H_
#define MINDSPORE_CORE_OPS_SPARSE_SOFTMAX_CROSS_ENTROPY_WITH_LOGITS_V2_H_
#include <memory>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameSparseSoftmaxCrossEntropyWithLogitsV2 = "SparseSoftmaxCrossEntropyWithLogitsV2";
/// \brief Computes the softmax cross-entropy value between logits and sparse
/// encoding labels. Refer to Python API @ref
/// mindspore.ops.SparseSoftmaxCrossEntropyWithLogitsV2 for more details.
class MIND_API SparseSoftmaxCrossEntropyWithLogitsV2 : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(SparseSoftmaxCrossEntropyWithLogitsV2);
  /// \brief Constructor.
  SparseSoftmaxCrossEntropyWithLogitsV2() : BaseOperator(kNameSparseSoftmaxCrossEntropyWithLogitsV2) {
    InitIOName({"features", "labels"}, {"loss", "backprop"});
  }
};
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_SPARSE_SOFTMAX_CROSS_ENTROPY_WITH_LOGITS_V2_H_
