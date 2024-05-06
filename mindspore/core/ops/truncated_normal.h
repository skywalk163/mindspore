/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CORE_OPS_TRUNCATEDNORMAL_H_
#define MINDSPORE_CORE_OPS_TRUNCATEDNORMAL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kTruncatedNormal = "TruncatedNormal";
class MIND_API TruncatedNormal : public BaseOperator {
  /// \brief Generates random numbers according to the truncatednormal random number distribution.
  /// Refer to Python API @ref mindspore.ops.Truncatednormal for more details.
 public:
  MIND_API_BASE_MEMBER(TruncatedNormal);
  /// \brief Constructor.
  TruncatedNormal() : BaseOperator(kTruncatedNormal) { InitIOName({"shape"}, {"output"}); }
  /// \brief Init. Refer to the parameters of Python API @ref mindspore.ops.Truncatednormal for the inputs.
  void Init(const int64_t seed = 0, const int64_t seed2 = 0);

  /// \brief Set seed. Defaults to 0.
  void set_seed(const int64_t seed);
  /// \brief Get seed.
  ///
  /// \return seed.
  int64_t get_seed() const;

  /// \brief Set seed2. Defaults to 0.
  void set_seed2(const int64_t seed2);
  /// \brief Get seed2.
  ///
  /// \return seed2.
  int64_t get_seed2() const;
};

MIND_API abstract::AbstractBasePtr TruncatedNormalInfer(const abstract::AnalysisEnginePtr &,
                                                        const PrimitivePtr &primitive,
                                                        const std::vector<abstract::AbstractBasePtr> &input_args);
using PrimTruncatedNormalPtr = std::shared_ptr<TruncatedNormal>;
}  // namespace ops
}  // namespace mindspore
#endif
