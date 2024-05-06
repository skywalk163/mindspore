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

#ifndef MINDSPORE_CORE_OPS_BINCOUNT_H
#define MINDSPORE_CORE_OPS_BINCOUNT_H

#include <memory>
#include <vector>
#include "abstract/abstract_value.h"
#include "ops/base_operator.h"
#include "ops/primitive_c.h"

namespace mindspore {
namespace ops {
constexpr auto kNameBincount = "Bincount";

class MIND_API Bincount : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(Bincount);
  Bincount() : BaseOperator(kNameBincount) { InitIOName({"array", "size", "weights"}, {"bins"}); }
};

AbstractBasePtr BincountInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                              const std::vector<AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore
#endif  // MINDSPORE_CORE_OPS_BINCOUNT_H
