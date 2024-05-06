/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CORE_OPS_BATCH_TO_SPACE_ND_V2_H_
#define MINDSPORE_CORE_OPS_BATCH_TO_SPACE_ND_V2_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameBatchToSpaceNDV2 = "BatchToSpaceNDV2";
/// \brief Divides batch dimension with blocks and interleaves these blocks back into spatial dimensions.
/// Refer to Python API @ref mindspore.ops.BatchToSpaceNDV2 for more details.
class MIND_API BatchToSpaceNDV2 : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(BatchToSpaceNDV2);
  /// \brief Constructor.
  BatchToSpaceNDV2() : BaseOperator(kNameBatchToSpaceNDV2) { InitIOName({"input_x", "block_shape", "crops"}, {"y"}); }
  /// \brief Init. Refer to the parameters of Python API @ref mindspore.ops.BatchToSpaceNDV2 for the inputs.
  void Init() const {}
};
MIND_API abstract::AbstractBasePtr BatchToSpaceNDV2Infer(const abstract::AnalysisEnginePtr &,
                                                         const PrimitivePtr &primitive,
                                                         const std::vector<abstract::AbstractBasePtr> &input_args);
using kPrimBatchToSpaceNDV2Ptr = std::shared_ptr<BatchToSpaceNDV2>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_BATCH_TO_SPACE_ND_V2_H_
