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

#include "ops/grad/sparse_segment_mean_grad.h"

#include <map>
#include <set>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/named.h"
#include "ir/primitive.h"
#include "ir/value.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr SparseSegmentMeanGradInferShape(const PrimitivePtr &prim,
                                                   const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  constexpr size_t kRankNum0 = 0;
  constexpr size_t kRankNum1 = 1;
  constexpr size_t kShapeNum0 = 0;
  constexpr int kDimNum0 = 0;
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto segment_ids_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape())[kShape];
  auto output_dim0_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex3]->GetShape())[kShape];

  if (x_shape.size() < kRankNum1) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', tensor x's rank cannot be less than 1.";
  }
  if (!IsDynamic(output_dim0_shape) && output_dim0_shape.size() != kRankNum0) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', tensor outputdim0 should be a scalar.";
  }
  if (!IsDynamic(indices_shape) && !IsDynamic(segment_ids_shape) &&
      indices_shape[kShapeNum0] != segment_ids_shape[kShapeNum0]) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', tensor indices & segment_ids's ranks mismatch.";
  }

  if (IsDynamicRank(x_shape)) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
  }

  ShapeVector y_shape = x_shape;
  if (!input_args[kInputIndex3]->GetValue()->isa<ValueAny>() && !input_args[kInputIndex3]->GetValue()->isa<None>()) {
    auto output_dim0_value_ptr = input_args[kInputIndex3]->GetValue();
    MS_EXCEPTION_IF_NULL(output_dim0_value_ptr);
    auto output_dim0_type_ptr = input_args[kInputIndex3]->GetType();
    MS_EXCEPTION_IF_NULL(output_dim0_type_ptr);
    auto output_dim0_value_ptr_tensor =
      CheckAndConvertUtils::CheckTensorIntValue("output_dim0", output_dim0_value_ptr, prim_name, output_dim0_type_ptr);
    int dim_zero = static_cast<int32_t>(output_dim0_value_ptr_tensor[kShapeNum0]);
    if (dim_zero < kDimNum0) {
      MS_EXCEPTION(ValueError) << "Input output_dim0 must >= 0!";
    } else {
      y_shape[kShapeNum0] = dim_zero;
    }
  } else {
    y_shape[kShapeNum0] = abstract::Shape::kShapeDimAny;
  }

  return std::make_shared<abstract::Shape>(y_shape);
}

TypePtr SparseSegmentMeanGradInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto x_type = input_args[kInputIndex0]->GetType();
  auto indices_type = input_args[kInputIndex1]->GetType();
  auto segment_ids_type = input_args[kInputIndex2]->GetType();
  auto output_dim0_type = input_args[kInputIndex3]->GetType();
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", x_type, {kFloat16, kFloat32, kFloat64}, prim->name());
  std::map<std::string, TypePtr> types;
  (void)types.emplace("indices", indices_type);
  (void)types.emplace("segment_ids", segment_ids_type);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("output_dim0", output_dim0_type, {kInt32}, prim->name());
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, {kInt32, kInt64}, prim->name());
  return input_args[kInputIndex0]->GetType();
}
}  // namespace

AbstractBasePtr SparseSegmentMeanGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &prim,
                                           const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  const int64_t kInputNum = 4;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputNum, prim_name);
  auto types = SparseSegmentMeanGradInferType(prim, input_args);
  auto shapes = SparseSegmentMeanGradInferShape(prim, input_args);
  return abstract::MakeAbstract(shapes, types);
}

MIND_API_OPERATOR_IMPL(SparseSegmentMeanGrad, BaseOperator);
// AG means auto generated
class MIND_API AGSparseSegmentMeanGradInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentMeanGradInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentMeanGradInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentMeanGradInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {3}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(SparseSegmentMeanGrad, prim::kPrimSparseSegmentMeanGrad, AGSparseSegmentMeanGradInfer,
                                 false);
}  // namespace ops
}  // namespace mindspore
