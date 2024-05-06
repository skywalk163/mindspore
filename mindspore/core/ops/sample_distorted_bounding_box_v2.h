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

#ifndef MINDSPORE_CORE_OPS_SAMPLE_DISTORTED_BOUNDING_BOX_V2_H_
#define MINDSPORE_CORE_OPS_SAMPLE_DISTORTED_BOUNDING_BOX_V2_H_

#include <memory>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameSampleDistortedBoundingBoxV2 = "SampleDistortedBoundingBoxV2";
/// \brief Generate a single randomly distorted bounding box for an image.
class MIND_API SampleDistortedBoundingBoxV2 : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(SampleDistortedBoundingBoxV2);
  /// \brief Constructor.
  SampleDistortedBoundingBoxV2() : BaseOperator(kNameSampleDistortedBoundingBoxV2) {
    InitIOName({"image_size", "bounding_boxes", "min_object_covered"}, {"begin", "size", "bboxes"});
  }

  void set_seed(const int64_t seed);
  int64_t get_seed() const;
  void set_seed2(const int64_t seed2);
  int64_t get_seed2() const;
  void set_aspect_ratio_range(const std::vector<float> aspect_ratio_range);
  std::vector<float> get_aspect_ratio_range() const;
  void set_area_range(const std::vector<float> area_range);
  std::vector<float> get_area_range() const;
  void set_max_attempts(const int64_t max_attempts);
  int64_t get_max_attempts() const;
  void set_use_image(const bool use_image);
  bool get_use_image() const;
};
MIND_API abstract::AbstractBasePtr SampleDistortedBoundingBoxV2Infer(
  const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
  const std::vector<abstract::AbstractBasePtr> &input_args);
using PrimSampleDistortedBoundingBoxV2Ptr = std::shared_ptr<SampleDistortedBoundingBoxV2>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_SAMPLE_DISTORTED_BOUNDING_BOX_V2_H_
