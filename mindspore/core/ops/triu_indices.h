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

#ifndef MINDSPORE_CORE_OPS_TRIU_INDICES_H_
#define MINDSPORE_CORE_OPS_TRIU_INDICES_H_

#include <memory>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameTriuIndices = "TriuIndices";
/// \brief Returns the indices of the lower triangular part of a row-by- col matrix.
/// Refer to Python API @ref mindspore.ops.TriuIndices for more details.
class MIND_API TriuIndices : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(TriuIndices);
  /// \brief Construct.
  TriuIndices() : BaseOperator(kNameTriuIndices) { InitIOName({}, {"y"}); }
  /// \brief Init.
  void Init(const int64_t row, const int64_t col, const int64_t offset = 0);
  /// \brief Set row.
  void set_row(const int64_t row);
  /// \brief Set col.
  void set_col(const int64_t col);
  /// \brief Set offset.
  void set_offset(const int64_t offset);

  /// \brief Get row.
  ///
  /// \return row.
  int64_t get_row() const;
  /// \brief Get col.
  ///
  /// \return col.
  int64_t get_col() const;
  /// \brief Get offset.
  ///
  /// \return offset.
  int64_t get_offset() const;
};

MIND_API abstract::AbstractBasePtr TriuIndicesInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                    const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_TRIU_INDICES_H_
