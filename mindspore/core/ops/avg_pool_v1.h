/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CORE_OPS_AVG_POOL_V1_H_
#define MINDSPORE_CORE_OPS_AVG_POOL_V1_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
constexpr auto kNameAvgPoolV1 = "AvgPoolV1";
/// \brief Average pooling operation. Refer to Python API @ref mindspore.ops.AvgPoolV1 for more details.
class MIND_API AvgPoolV1 : public BaseOperator {
 public:
  /// \brief Constructor.
  AvgPoolV1() : BaseOperator(kNameAvgPoolV1) { InitIOName({"value"}, {"output"}); }
  MIND_API_BASE_MEMBER(AvgPoolV1);
};

MIND_API abstract::AbstractBasePtr AvgPoolV1Infer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                  const std::vector<AbstractBasePtr> &input_args);
using PrimAvgPoolV1Ptr = std::shared_ptr<AvgPoolV1>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_AVG_POOL_V1_H_
