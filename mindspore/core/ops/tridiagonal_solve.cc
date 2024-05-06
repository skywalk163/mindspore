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

#include "ops/tridiagonal_solve.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
constexpr auto kTridiagonalSolveInputNums = 2;
namespace {
abstract::ShapePtr TridiagonalSolveInferShape(const PrimitivePtr &primitive,
                                              const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kTridiagonalSolveInputNums, primitive->name());
  auto diagonals_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape());
  auto rhs_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape());
  auto diagonals_shp = diagonals_shape_map[kShape];
  auto rhs_shp = rhs_shape_map[kShape];

  std::vector<ShapeVector> all_shapes = {diagonals_shp, rhs_shp};
  auto is_dynamic_rank = std::any_of(all_shapes.begin(), all_shapes.end(), IsDynamicRank);
  auto is_dynamic = std::any_of(all_shapes.begin(), all_shapes.end(), IsDynamic);

  const int64_t kNumOne = 1;
  const int64_t numberofdiagonals = 3;
  const size_t numberforthelastsecend = 2;
  const size_t rank_diagonals = diagonals_shp.size();
  const size_t rank_rhs = rhs_shp.size();

  if (!is_dynamic_rank) {
    (void)CheckAndConvertUtils::CheckInteger("the rank of the input diagonals", SizeToLong(rank_diagonals),
                                             kGreaterThan, kNumOne, prim_name);
    (void)CheckAndConvertUtils::CheckInteger("the rank of the input diagonals and rhs", SizeToLong(rank_diagonals),
                                             kEqual, SizeToLong(rank_rhs), prim_name);
  }

  if (!is_dynamic) {
    (void)CheckAndConvertUtils::CheckInteger("the last second dimension of the input diagonals",
                                             diagonals_shp[rank_diagonals - numberforthelastsecend], kEqual,
                                             numberofdiagonals, prim_name);
    (void)CheckAndConvertUtils::CheckInteger(
      "the last dimension of the input diagonals and the last second dimension of the input rhs",
      diagonals_shp[rank_diagonals - 1], kEqual, rhs_shp[rank_rhs - numberforthelastsecend], prim_name);
  }

  return std::make_shared<abstract::Shape>(rhs_shp);
}

TypePtr TridiagonalSolveInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kTridiagonalSolveInputNums, prim->name());
  const std::set<TypePtr> valid_types = {kFloat32, kFloat64, kComplex64, kComplex128};
  std::map<std::string, TypePtr> types;
  (void)types.insert({"diagonals", input_args[0]->GetType()});
  (void)types.insert({"rhs", input_args[1]->GetType()});
  (void)CheckAndConvertUtils::CheckScalarOrTensorTypesSame(types, valid_types, prim->name());

  return input_args[1]->GetType();
}
}  // namespace

bool TridiagonalSolve::get_partial_pivoting() const {
  auto value_ptr = GetAttr("partial_pivoting");
  MS_EXCEPTION_IF_NULL(value_ptr);
  return GetValue<bool>(value_ptr);
}

AbstractBasePtr TridiagonalSolveInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                      const std::vector<AbstractBasePtr> &input_args) {
  auto infer_type = TridiagonalSolveInferType(primitive, input_args);
  auto infer_shape = TridiagonalSolveInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

MIND_API_OPERATOR_IMPL(TridiagonalSolve, BaseOperator);
// AG means auto generated
class MIND_API AGTridiagonalSolveInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return TridiagonalSolveInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return TridiagonalSolveInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return TridiagonalSolveInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(TridiagonalSolve, prim::kPrimTridiagonalSolve, AGTridiagonalSolveInfer, false);
}  // namespace ops
}  // namespace mindspore
