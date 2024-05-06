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

#ifndef MINDSPORE_CORE_OPS_CROP_AND_RESIZE_GRAD_BOXES_H_
#define MINDSPORE_CORE_OPS_CROP_AND_RESIZE_GRAD_BOXES_H_

#include <memory>
#include <vector>

#include "abstract/abstract_value.h"
#include "mindapi/base/types.h"
#include "ops/base_operator.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
constexpr auto kNameCropAndResizeGradBoxes = "CropAndResizeGradBoxes";
/// \brief Computes the gradient of the crop_and_resize op wrt the input boxes tensor .
/// Refer to Python API @ref mindspore.ops.CropAndResizeGradBoxes for more details.
class MIND_API CropAndResizeGradBoxes : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(CropAndResizeGradBoxes);
  /// \brief Constructor.

  CropAndResizeGradBoxes() : BaseOperator(kNameCropAndResizeGradBoxes) {
    InitIOName({"grads", "images", "boxes", "box_index"}, {"y"});
  }

  /// \brief Init. Refer to the parameters of Python API @ref mindspore.ops.CropAndResizeGradBoxes for the inputs.
  void Init(ResizeMethod method);

  /// \brief Set method.
  void set_method(ResizeMethod method);

  /// \brief Get method.
  ResizeMethod get_method() const;
};
AbstractBasePtr CropAndResizeGradBoxesInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                            const std::vector<AbstractBasePtr> &input_args);
using PrimCropAndResizeGradBoxesPtr = std::shared_ptr<CropAndResizeGradBoxes>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_CROP_AND_RESIZE_GRAD_BOXES_H_
