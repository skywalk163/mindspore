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

#ifndef MINDSPORE_CORE_OPS_GRAD_SCALE_AND_TRANSLATE_GRAD_H_
#define MINDSPORE_CORE_OPS_GRAD_SCALE_AND_TRANSLATE_GRAD_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameScaleAndTranslateGrad = "ScaleAndTranslateGrad";
/// \brief Computes gradients of ScaleAndTranslate.
class MIND_API ScaleAndTranslateGrad : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(ScaleAndTranslateGrad);
  /// \brief Constructor.
  ScaleAndTranslateGrad() : BaseOperator(kNameScaleAndTranslateGrad) {
    InitIOName({"grads", "original_image", "scale", "translation"}, {"y"});
  }
  /// \brief Init. Refer to the parameters of Python API @ref mindspore.ops.ScaleAndTranslateGrad for the inputs.
  void Init(const std::string kernel_type = "lanczos3", const bool antialias = true);
  /// \brief Set kernel_type.
  void set_kernel_type(const std::string kernel_type);
  /// \brief Set antialias.
  void set_antialias(const bool antialias);
  /// \brief Get kernel_type.
  ///
  /// \return kernel_type.
  std::string get_kernel_type() const;
  /// \brief Get antialias.
  ///
  /// \return antialias.
  bool get_antialias() const;
};

MIND_API abstract::AbstractBasePtr ScaleAndTranslateGradInfer(const abstract::AnalysisEnginePtr &,
                                                              const PrimitivePtr &primitive,
                                                              const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore
#endif  // MINDSPORE_CORE_OPS_GRAD_SCALE_AND_TRANSLATE_GRAD_H_
