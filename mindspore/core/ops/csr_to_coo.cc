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

#include "ops/csr_to_coo.h"

#include <memory>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/param_validator.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/primitive.h"
#include "ir/scalar.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
using abstract::AbstractScalar;
using abstract::AbstractTensor;
using abstract::AbstractTuple;
AbstractBasePtr CSR2COOInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                             const std::vector<AbstractBasePtr> &input_args) {
  // Inputs: the indptr of a sparse csr tensor, and the number of non-zero elements.
  constexpr auto kCSRArgsSize = 2;
  const std::string op_name = primitive->name();
  CheckArgsSize(op_name, input_args, kCSRArgsSize);
  auto indptr = abstract::CheckArg<AbstractTensor>(op_name, input_args, 0);
  MS_EXCEPTION_IF_NULL(indptr);
  CheckSparseIndicesDtypeInt32(indptr->element()->GetType(), "Indptr");

  auto nnz = abstract::CheckArg<AbstractScalar>(op_name, input_args, 1);
  MS_EXCEPTION_IF_NULL(nnz);

  MS_EXCEPTION_IF_NULL(nnz->GetValue());
  ShapeVector out_shape;
  if (nnz->GetValue()->isa<Int32Imm>() || nnz->GetValue()->isa<Int64Imm>()) {
    int64_t nnz_value = GetValue<int64_t>(nnz->GetValue());
    out_shape.push_back(nnz_value);
  } else {
    MS_EXCEPTION(ValueError) << "Currently, only support Integer nnz.";
  }

  MS_EXCEPTION_IF_NULL(indptr->shape());
  auto num_rows = indptr->shape()->shape()[0] - 1;
  int csr_avg_rows = GetValue<int64_t>(nnz->GetValue()) / num_rows;
  primitive->set_attr(kCSRAvgRows, MakeValue(csr_avg_rows));
  primitive->set_attr(kIsCSR, MakeValue(true));

  MS_EXCEPTION_IF_NULL(indptr->element());
  auto ret = std::make_shared<AbstractTensor>(indptr->element()->GetType(), out_shape);
  return ret;
}
MIND_API_OPERATOR_IMPL(CSR2COO, BaseOperator);
REGISTER_PRIMITIVE_EVAL_IMPL(CSR2COO, prim::kPrimCSR2COO, CSR2COOInfer, nullptr, true);
}  // namespace ops
}  // namespace mindspore
