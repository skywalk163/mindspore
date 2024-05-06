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

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "abstract/dshape.h"
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sparse_ops.h"
#include "ops/op_name.h"
#include "ops/sparse_segment_sqrt_n_with_num_segments.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr SparseSegmentSqrtNWithNumSegmentsInferShape(const PrimitivePtr &prim,
                                                               const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto indices_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto segment_ids_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape())[kShape];
  auto num_segments_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex3]->GetShape())[kShape];
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
  if (num_segments_shape.size() > kInputIndex1) {
    MS_EXCEPTION(ValueError) << "For " << prim_name << ", num_segments should be at most 1-D, but got ["
                             << num_segments_shape.size() << "].";
  }
  if (IsDynamicRank(x_shape)) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
  }
  if (!input_args[kInputIndex3]->GetValue()->isa<ValueAny>() && !input_args[kInputIndex3]->GetValue()->isa<None>()) {
    if (!IsDynamic(num_segments_shape) && num_segments_shape.size() == kInputIndex1) {
      if (LongToSize(num_segments_shape[kInputIndex0]) != kInputIndex1) {
        MS_EXCEPTION(ValueError) << "For " << prim_name << ", the num element of num_segments should be 1, but got ["
                                 << num_segments_shape[kInputIndex0] << "].";
      }
    }
    auto num_segments_value_ptr = input_args[kInputIndex3]->GetValue();
    MS_EXCEPTION_IF_NULL(num_segments_value_ptr);
    auto num_segments_type_ptr = input_args[kInputIndex3]->GetType();
    MS_EXCEPTION_IF_NULL(num_segments_type_ptr);
    auto num_segments_value_ptr_tensor = CheckAndConvertUtils::CheckTensorIntValue(
      "num_segments", num_segments_value_ptr, prim->name(), num_segments_type_ptr);
    size_t dim_zero = static_cast<size_t>(num_segments_value_ptr_tensor.back());
    if (dim_zero < kInputIndex1) {
      MS_EXCEPTION(ValueError) << "For " << prim_name
                               << ", num_segments must bigger than the last number of segment_ids, "
                               << "but got " << dim_zero << ".";
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

TypePtr SparseSegmentSqrtNWithNumSegmentsInferType(const PrimitivePtr &prim,
                                                   const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto x_type = input_args[kInputIndex0]->GetType();
  auto indices_type = input_args[kInputIndex1]->GetType();
  auto segment_ids_type = input_args[kInputIndex2]->GetType();
  auto num_segments_type = input_args[kInputIndex3]->GetType();
  const std::set<TypePtr> valid_types = {kFloat16, kFloat32, kFloat64};
  const std::set<TypePtr> common_valid_types = {kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", x_type, valid_types, prim->name());
  std::map<std::string, TypePtr> types;
  (void)types.emplace("indices", indices_type);
  (void)types.emplace("segment_ids", segment_ids_type);
  (void)types.emplace("num_segments", num_segments_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, common_valid_types, prim->name());
  return input_args[kInputIndex0]->GetType();
}
}  // namespace

MIND_API_OPERATOR_IMPL(SparseSegmentSqrtNWithNumSegments, BaseOperator);
AbstractBasePtr SparseSegmentSqrtNWithNumSegmentsInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &prim,
                                                       const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  const int64_t input_num = static_cast<int64_t>(kInputIndex4);
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, prim_name);
  auto types = SparseSegmentSqrtNWithNumSegmentsInferType(prim, input_args);
  auto shapes = SparseSegmentSqrtNWithNumSegmentsInferShape(prim, input_args);
  return abstract::MakeAbstract(shapes, types);
}

// AG means auto generated
class MIND_API AGSparseSegmentSqrtNWithNumSegmentsInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentSqrtNWithNumSegmentsInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentSqrtNWithNumSegmentsInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SparseSegmentSqrtNWithNumSegmentsInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {3}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(SparseSegmentSqrtNWithNumSegments, prim::kPrimSparseSegmentSqrtNWithNumSegments,
                                 AGSparseSegmentSqrtNWithNumSegmentsInfer, false);
}  // namespace ops
}  // namespace mindspore
