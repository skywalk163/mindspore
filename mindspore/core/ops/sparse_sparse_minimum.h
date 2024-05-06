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

#ifndef MINDSPORE_CORE_OPS_SPARSE_SPARSE_MINIMUM_H_
#define MINDSPORE_CORE_OPS_SPARSE_SPARSE_MINIMUM_H_
#include <memory>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameSparseSparseMinimum = "SparseSparseMinimum";
class MIND_API SparseSparseMinimum : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(SparseSparseMinimum);
  SparseSparseMinimum() : BaseOperator(kNameSparseSparseMinimum) {
    InitIOName({"x1_indices", "x1_values", "x1_shape", "x2_indices", "x2_values", "x2_shape"},
               {"y_indices", "y_values"});
  }
};
using PrimSparseSparseMinimumPtr = std::shared_ptr<SparseSparseMinimum>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_SPARSE_SPARSE_MINIMUM_H_
