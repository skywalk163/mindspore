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

#ifndef MINDSPORE_CORE_OPS_RAGGED_TENSOR_TO_SPARSE_H_
#define MINDSPORE_CORE_OPS_RAGGED_TENSOR_TO_SPARSE_H_
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "abstract/abstract_value.h"
#include "ops/base_operator.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
constexpr auto kNameRaggedTensorToSparse = "RaggedTensorToSparse";

class MIND_API RaggedTensorToSparse : public BaseOperator {
 public:
  RaggedTensorToSparse() : BaseOperator(kNameRaggedTensorToSparse) {
    InitIOName({"rt_nested_splits, rt_dense_values"}, {"sparse_indices", "sparse_values", "sparse_dense_shape"});
  }
  MIND_API_BASE_MEMBER(RaggedTensorToSparse);
};

abstract::AbstractBasePtr RaggedTensorToSparseInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                    const std::vector<AbstractBasePtr> &input_args);
using PrimRaggedTensorToSparsePtr = std::shared_ptr<RaggedTensorToSparse>;
}  // namespace ops
}  // namespace mindspore

#endif  // MINDSPORE_CORE_OPS_RAGGED_TENSOR_TO_SPARSE_H_
