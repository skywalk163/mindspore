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

#include "ops/sparse_segment_sum.h"

#include <map>
#include <memory>
#include <set>

#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_name.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr SparseSegmentSumInferShape(const PrimitivePtr &prim,
                                              const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto segment_ids_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape())[kShape];
  // support dynamic rank
  if (IsDynamicRank(x_shape) || IsDynamicRank(indices_shape) || IsDynamicRank(segment_ids_shape)) {
    return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
  }
  (void)CheckAndConvertUtils::CheckInteger("indices_shape", SizeToLong(indices_shape.size()), kEqual,
                                           SizeToLong(kInputIndex1), prim->name());
  (void)CheckAndConvertUtils::CheckInteger("segment_ids_shape", SizeToLong(segment_ids_shape.size()), kEqual,
                                           SizeToLong(kInputIndex1), prim->name());
  if (x_shape.size() < kInputIndex1) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', "
                             << "x's rank must be greater than 1, but got [" << x_shape.size() << "].";
  }
  if (!(IsDynamic(indices_shape) || IsDynamic(segment_ids_shape)) &&
      indices_shape[kInputIndex0] != segment_ids_shape[kInputIndex0]) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', the rank of indices and segment_ids must be the same, "
                             << "but got indices [" << indices_shape[kInputIndex0] << "] "
                             << "and segment_ids [" << segment_ids_shape[kInputIndex0] << "].";
  }
  if ((indices_shape[kInputIndex0] == kInputIndex0) || (segment_ids_shape[kInputIndex0] == kInputIndex0)) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name << "', the rank of indices and segment_ids must greater than 0, "
                             << "but got indices [" << indices_shape[kInputIndex0] << "] "
                             << "and segment_ids [" << segment_ids_shape[kInputIndex0] << "].";
  }
  if (!input_args[kInputIndex2]->GetValue()->isa<ValueAny>() && !input_args[kInputIndex2]->GetValue()->isa<None>()) {
    auto segment_ids_value_ptr = input_args[kInputIndex2]->GetValue();
    MS_EXCEPTION_IF_NULL(segment_ids_value_ptr);
    auto segment_ids_type_ptr = input_args[kInputIndex2]->GetType();
    MS_EXCEPTION_IF_NULL(segment_ids_type_ptr);
    auto segment_ids_value_ptr_tensor = CheckAndConvertUtils::CheckTensorIntValue("segment_ids", segment_ids_value_ptr,
                                                                                  prim->name(), segment_ids_type_ptr);
    size_t dim_zero = static_cast<size_t>(segment_ids_value_ptr_tensor.back()) + kInputIndex1;
    if (dim_zero < kInputIndex1) {
      MS_EXCEPTION(ValueError) << "For '" << prim_name << "', segment_ids must be greater or equal to 0, "
                               << "but got [" << dim_zero << "].";
    } else {
      ShapeVector y_shape = x_shape;
      y_shape[kInputIndex0] = static_cast<int64_t>(dim_zero);
      return std::make_shared<abstract::Shape>(y_shape);
    }
  } else {
    ShapeVector output_shape = x_shape;
    output_shape[kInputIndex0] = -1;
    return std::make_shared<abstract::Shape>(output_shape);
  }
}

TypePtr SparseSegmentSumInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  auto x_type = input_args[kInputIndex0]->GetType();
  auto indices_type = input_args[kInputIndex1]->GetType();
  auto segment_ids_type = input_args[kInputIndex2]->GetType();
  const std::set<TypePtr> valid_types = {kInt8, kInt16, kInt32, kInt64, kUInt8, kUInt16, kFloat16, kFloat32, kFloat64};
  const std::set<TypePtr> common_valid_types = {kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", x_type, valid_types, prim_name);
  std::map<std::string, TypePtr> types;
  (void)types.emplace("indices", indices_type);
  (void)types.emplace("segment_ids", segment_ids_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, common_valid_types, prim->name());
  return input_args[kInputIndex0]->GetType();
}
}  // namespace

MIND_API_OPERATOR_IMPL(SparseSegmentSum, BaseOperator);
AbstractBasePtr SparseSegmentSumInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &prim,
                                      const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  const int64_t input_num = static_cast<int64_t>(kInputIndex3);
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, prim_name);
  auto types = SparseSegmentSumInferType(prim, input_args);
  auto shapes = SparseSegmentSumInferShape(prim, input_args);
  return abstract::MakeAbstract(shapes, types);
}

// AG means auto generated
class MIND_API AGSparseSegmentSumInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentSumInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentSumInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentSumInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {2}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(SparseSegmentSum, prim::kPrimSparseSegmentSum, AGSparseSegmentSumInfer, false);
}  // namespace ops
}  // namespace mindspore
