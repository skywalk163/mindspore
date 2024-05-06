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

#ifndef MINDSPORE_CORE_OPS_SPARSE_MATRIX_ADD_H_
#define MINDSPORE_CORE_OPS_SPARSE_MATRIX_ADD_H_
#include <memory>
#include <vector>

#include "mindapi/base/types.h"
#include "ops/base_operator.h"

namespace mindspore {
namespace ops {
constexpr auto kNameSparseMatrixAdd = "SparseMatrixAdd";
/// \brief Computes the softmax cross-entropy value between logits and sparse encoding labels.
/// Refer to Python API @ref mindspore.ops.SparseMatrixAdd for more details.
class MIND_API SparseMatrixAdd : public BaseOperator {
 public:
  MIND_API_BASE_MEMBER(SparseMatrixAdd);
  /// \brief Constructor.
  SparseMatrixAdd() : BaseOperator(kNameSparseMatrixAdd) {
    InitIOName({"a_indptr", "a_indices", "a_values", "b_indptr", "b_indices", "b_values", "alpha", "beta"},
               {"c_indptr", "c_indices", "c_values"});
  }
  /// \brief Init.
  /// Refer to the parameters of python API @ref mindspore.ops._csr_ops.SparseMatrixAdd for the inputs.
  void Init(const std::vector<int64_t> &csr_a, const std::vector<int64_t> &csr_b);
  /// \brief Set dense shape.
  void set_dense_shape(const std::vector<int64_t> &shape);
  /// \brief Get dense shape.
  ///
  /// \return dense shape.
  std::vector<int64_t> get_dense_shape() const;
};
MIND_API abstract::AbstractBasePtr SparseMatrixAddInfer(const abstract::AnalysisEnginePtr &,
                                                        const PrimitivePtr &primitive,
                                                        const std::vector<abstract::AbstractBasePtr> &input_args);
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_SPARSE_MATRIX_ADD_H_
