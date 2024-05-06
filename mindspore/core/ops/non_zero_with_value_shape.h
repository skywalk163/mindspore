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

#ifndef MINDSPORE_CORE_OPS_NON_ZERO_WITH_VALUE_SHAPE_H_
#define MINDSPORE_CORE_OPS_NON_ZERO_WITH_VALUE_SHAPE_H_
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameNonZeroWithValueShape = "NonZeroWithValueShape";
/// \brief Returns the value and index of elements that are non-zero.
class MIND_API NonZeroWithValueShape : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(NonZeroWithValueShape);
  /// \brief Constructor.
  NonZeroWithValueShape() : BaseOperator(kNameNonZeroWithValueShape) {
    InitIOName({"value", "index", "count"}, {"out_value", "out_index"});
  }
  /// \brief Init.
  void Init() const {}
};
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_NON_ZERO_WITH_VALUE_SHAPE_H_
