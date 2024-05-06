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

#ifndef MINDSPORE_CORE_OPS_INSTANCE_NORM_V2_H_
#define MINDSPORE_CORE_OPS_INSTANCE_NORM_V2_H_
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameInstanceNormV2 = "InstanceNormV2";
/// \brief InstanceNormV2 defined the InstanceNormV2 operator prototype.
class MIND_API InstanceNormV2 : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(InstanceNormV2);
  /// \brief Constructor.
  InstanceNormV2() : BaseOperator(kNameInstanceNormV2) {
    InitIOName({"x", "gamma", "beta", "mean", "variance"}, {"y", "batch_mean", "batch_variance"});
  }
};

MIND_API abstract::AbstractBasePtr InstanceNormV2Infer(const abstract::AnalysisEnginePtr &,
                                                       const PrimitivePtr &primitive,
                                                       const std::vector<abstract::AbstractBasePtr> &input_args);
using PrimInstanceNormV2Ptr = std::shared_ptr<InstanceNormV2>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_INSTANCE_NORM_V2_H_
