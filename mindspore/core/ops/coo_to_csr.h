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

#ifndef MINDSPORE_CORE_OPS_COO_TO_CSR
#define MINDSPORE_CORE_OPS_COO_TO_CSR
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameCOO2CSR = "COO2CSR";
/// \brief Converts the row indices of a COOTensor to the indptr of a CSRTensor.
class MIND_API COO2CSR : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(COO2CSR);
  /// \brief Constructor.
  COO2CSR() : BaseOperator(kNameCOO2CSR) { InitIOName({"row_indices", "height"}, {"output"}); }
};
MIND_API abstract::AbstractBasePtr COO2CSRInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_COO_TO_CSR
