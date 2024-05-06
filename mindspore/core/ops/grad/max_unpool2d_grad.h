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

#ifndef MINDSPORE_CORE_OPS_MAXUNPOOL2DGRAD_H_
#define MINDSPORE_CORE_OPS_MAXUNPOOL2DGRAD_H_
#include <memory>
#include <string>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameMaxUnpool2DGrad = "MaxUnpool2DGrad";
class MIND_API MaxUnpool2DGrad : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(MaxUnpool2DGrad);
  MaxUnpool2DGrad() : BaseOperator(kNameMaxUnpool2DGrad) { InitIOName({"x", "grads", "argmax"}, {"y"}); }
  std::string get_format() const;
};

MIND_API abstract::AbstractBasePtr MaxUnpool2DGradInfer(const abstract::AnalysisEnginePtr &,
                                                        const PrimitivePtr &primitive,
                                                        const std::vector<abstract::AbstractBasePtr> &input_args);
using PrimMaxUnpool2DGradPtr = std::shared_ptr<MaxUnpool2DGrad>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_MAXUNPOOL2DGRAD_H_
