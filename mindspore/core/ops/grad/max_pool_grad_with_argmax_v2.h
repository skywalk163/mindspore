/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CORE_OPS_MAX_POOL_GRAD_WITH_ARGMAX_V2_H_
#define MINDSPORE_CORE_OPS_MAX_POOL_GRAD_WITH_ARGMAX_V2_H_

#include <memory>
#include <set>
#include <string>
#include <vector>
#include "mindapi/base/format.h"
#include "mindapi/base/type_id.h"
#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameMaxPoolGradWithArgmaxV2 = "MaxPoolGradWithArgmaxV2";
/// \brief Max pooling operation. Refer to Python API @ref mindspore.ops.MaxPoolGradWithArgmaxV2 for more details
class MIND_API MaxPoolGradWithArgmaxV2 : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(MaxPoolGradWithArgmaxV2);
  /// \brief Constructor
  MaxPoolGradWithArgmaxV2() : BaseOperator(kNameMaxPoolGradWithArgmaxV2) { InitIOName({"x", "grad", "argmax"}, {"y"}); }

  void Init(const std::vector<int64_t> &kernel_size, const std::vector<int64_t> &strides = {1},
            const std::vector<int64_t> &pads = {0}, const std::vector<int64_t> &dilation = {1}, bool ceil_mode = false,
            const TypeId &argmax_type = kNumberTypeInt64);
  /// \brief Set kernel_szie.
  void set_kernel_size(const std::vector<int64_t> &kernel_size);
  /// \brief Set strides.
  void set_strides(const std::vector<int64_t> &strides);
  /// \brief Set pads.
  void set_pads(const std::vector<int64_t> &pads);
  /// \brief Set dilation.
  void set_dilation(const std::vector<int64_t> &dilation);
  /// \brief Set ceil_mode.
  void set_ceil_mode(bool ceil_mode);
  /// \brief Set argmax_type.
  void set_argmax_type(const TypeId &argmax_type);

  /// \return kernel_size.
  std::vector<int64_t> get_kernel_size() const;
  /// \return strides.
  std::vector<int64_t> get_strides() const;
  /// \return pads.
  std::vector<int64_t> get_pads() const;
  /// \return dilation.
  std::vector<int64_t> get_dilation() const;
  /// \return ceil_mode.
  bool get_ceil_mode() const;
  /// \return argmax_type.
  TypeId get_argmax_type() const;
};

abstract::AbstractBasePtr MaxPoolGradWithArgmaxV2Infer(const abstract::AnalysisEnginePtr &,
                                                       const PrimitivePtr &primitive,
                                                       const std::vector<abstract::AbstractBasePtr> &input_args);
using PrimMaxPoolGradWithArgmaxV2 = std::shared_ptr<MaxPoolGradWithArgmaxV2>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_MAX_POOL_GRAD_WITH_ARGMAX_V2_H_
