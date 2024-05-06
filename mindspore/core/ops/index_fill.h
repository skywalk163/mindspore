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

#ifndef MINDSPORE_CORE_OPS_INDEX_FILL_H_
#define MINDSPORE_CORE_OPS_INDEX_FILL_H_
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameIndexFill = "IndexFill";
/// \brief Fill self tensor with input values.
/// Refer to Python API @ref mindspore.ops.IndexFill for more details.
class MIND_API IndexFill : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(IndexFill);
  IndexFill() : BaseOperator(kNameIndexFill) { InitIOName({"x", "dim", "index", "value"}, {"y"}); }
};

MIND_API abstract::AbstractBasePtr IndexFillInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                  const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_INDEX_FILL_H_
