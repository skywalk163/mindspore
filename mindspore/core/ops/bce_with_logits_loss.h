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

#ifndef MINDSPORE_CORE_OPS_BCE_WITH_LOGITS_LOSS_H_
#define MINDSPORE_CORE_OPS_BCE_WITH_LOGITS_LOSS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameBCEWithLogitsLoss = "BCEWithLogitsLoss";
/// \brief Uses the given logits to compute binary cross entropy between the logits and the label. Refer to Python API
/// @ref mindspore.ops.BCEWithLogitsLoss for more details.
class MIND_API BCEWithLogitsLoss : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(BCEWithLogitsLoss);
  /// \brief Constructor.
  BCEWithLogitsLoss() : BaseOperator(kNameBCEWithLogitsLoss) {
    InitIOName({"logits", "label", "weight", "pos_weight"}, {"y"});
  }
  /// \brief Init. Refer to the parameters of Python API @ref mindspore.ops.BCEWithLogitsLoss for the inputs.
  void Init(const std::string &reduction = "mean");
  /// \brief Set reduction.
  void set_reduction(const std::string &norm_region);
  /// \brief Get reduction.
  ///
  /// \return reduction.
  std::string get_reduction() const;
};
MIND_API abstract::AbstractBasePtr BCEWithLogitsLossInfer(const abstract::AnalysisEnginePtr &,
                                                          const PrimitivePtr &primitive,
                                                          const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore
#endif  // MINDSPORE_CORE_OPS_BCE_WITH_LOGITS_LOSS_H_
