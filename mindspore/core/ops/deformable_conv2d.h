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

#ifndef MINDSPORE_CORE_OPS_DEFORMABLE_CONV2D_H_
#define MINDSPORE_CORE_OPS_DEFORMABLE_CONV2D_H_

#include <vector>
#include <string>
#include "ops/base_operator.h"
#include "mindapi/base/format.h"
#include "include/common/utils/utils.h"

namespace mindspore {
namespace ops {
constexpr auto kNameDeformableConv2d = "DeformableConv2d";
/// \brief DeformableConv2D. Refer to Python API @ref mindspore.ops.deformable_conv2d for more details.
class MIND_API DeformableConv2d : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(DeformableConv2d);
  /// \brief Constructor.
  DeformableConv2d() : BaseOperator(kNameDeformableConv2d) { InitIOName({"x", "filter", "offsets", "bias"}, {"y"}); }
  /// \brief Init. Refer to the parameters of Python API @ref mindspore.ops.deformable_conv2d for the inputs.
  void Init(const std::vector<int64_t> &strides, const std::vector<int64_t> &pads,
            const std::vector<int64_t> &dilations = {1, 1, 1, 1}, int64_t groups = 1,
            const std::string &data_format = kOpFormat_NCHW, int64_t deformable_groups = 1, bool modulated = true);

  /// \brief Set strides.
  void set_strides(const std::vector<int64_t> &strides);

  /// \brief Get strides.
  ///
  /// \return strides.
  std::vector<int64_t> get_strides() const;

  /// \brief Set pads.
  void set_pads(const std::vector<int64_t> &pads);

  /// \brief Get pads.
  ///
  /// \return pads.
  std::vector<int64_t> get_pads() const;

  /// \brief Set dilations.
  void set_dilations(const std::vector<int64_t> &dilations);

  /// \brief Get dilations.
  ///
  /// \return dilations.
  std::vector<int64_t> get_dilations() const;

  /// \brief Set format.
  void set_data_format(const std::string &data_format);

  /// \brief Get format.
  ///
  /// \return format.
  std::string get_data_format() const;

  /// \brief Set number of blocked connection from input channels to output channels.
  void set_groups(int64_t groups);

  /// \brief Get number of groups.
  ///
  /// \return groups.
  int64_t get_groups() const;

  /// \brief Set deformable_groups.
  void set_deformable_groups(int64_t deformable_groups);

  /// \brief Get deformable_groups.
  ///
  /// \return deformable_groups.
  int64_t get_deformable_groups() const;

  /// \brief Set modulated.
  void set_modulated(bool modulated);

  /// \brief Get modulated.
  ///
  /// \return modulated.
  bool get_modulated() const;
};
MIND_API abstract::AbstractBasePtr DeformableConv2dInfer(const abstract::AnalysisEnginePtr &,
                                                         const PrimitivePtr &primitive,
                                                         const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore
#endif  // MINDSPORE_CORE_OPS_DEFORMABLE_CONV2D_H_
