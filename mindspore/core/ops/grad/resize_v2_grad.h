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

#ifndef MINDSPORE_CORE_OPS_GRAD_RESIZE_V2_GRAD_H_
#define MINDSPORE_CORE_OPS_GRAD_RESIZE_V2_GRAD_H_

#include <memory>
#include <string>
#include <vector>
#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameResizeV2Grad = "ResizeV2Grad";
class MIND_API ResizeV2Grad : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(ResizeV2Grad);
  ResizeV2Grad() : BaseOperator(kNameResizeV2Grad) { InitIOName({"grads", "roi", "scales", "original_size"}, {"y"}); }

  void Init(const std::string coordinate_transformation_mode = "half_pixel", const std::string mode = "nearest");

  void set_coordinate_transformation_mode(const std::string coordinate_transformation_mode);
  std::string get_coordinate_transformation_mode() const;
  void set_mode(const std::string mode);
  std::string get_mode() const;
};

abstract::AbstractBasePtr ResizeV2GradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                            const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_GRAD_RESIZE_V2_GRAD_H_
