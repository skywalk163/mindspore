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

#include <set>

#include "mindapi/src/helper.h"
#include "mindspore/core/ops/image_ops.h"
#include "ops/non_max_suppression_with_overlaps.h"

namespace mindspore {
namespace ops {
namespace {
const int64_t InputIndex2 = 2;
constexpr size_t kNonMaxSuppressionWithOverlapsInputsNum = 5;
constexpr size_t kOverlapsRank = 2;
abstract::ShapePtr NonMaxSuppressionWithOverlapsInferShape(const PrimitivePtr &primitive,
                                                           const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  (void)CheckAndConvertUtils::CheckInteger("input number", input_args.size(), kEqual,
                                           kNonMaxSuppressionWithOverlapsInputsNum, prim_name);
  for (const auto &i : input_args) {
    MS_EXCEPTION_IF_NULL(i);
  }
  (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 0, kObjectTypeTensorType);
  (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 1, kObjectTypeTensorType);

  auto overlaps_shape = input_args[kInputIndex0]->GetShape()->GetShapeVector();
  auto scores_shape = input_args[kInputIndex1]->GetShape()->GetShapeVector();
  auto max_output_size_shape = input_args[kInputIndex2]->GetShape()->GetShapeVector();
  auto overlap_threshold_shape = input_args[kInputIndex3]->GetShape()->GetShapeVector();
  auto score_threshold_shape = input_args[kInputIndex4]->GetShape()->GetShapeVector();

  (void)CheckAndConvertUtils::CheckInteger("rank of scores", SizeToLong(scores_shape.size()), kEqual, 1, prim_name);
  auto scores_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape());
  (void)CheckAndConvertUtils::CheckInteger("rank of overlaps", overlaps_shape.size(), kEqual, kOverlapsRank, prim_name);
  (void)CheckAndConvertUtils::CheckInteger("size of the second dimension of overlaps", overlaps_shape[1], kEqual,
                                           overlaps_shape[0], prim_name);
  (void)CheckAndConvertUtils::CheckInteger("length of scores", scores_shape[0], kEqual, overlaps_shape[0], prim_name);
  (void)CheckAndConvertUtils::CheckInteger("rank of max_output_size", max_output_size_shape.size(), kEqual, 0,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("rank of overlap_threshold", overlap_threshold_shape.size(), kEqual, 0,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("rank of score_threshold", score_threshold_shape.size(), kEqual, 0,
                                           prim_name);

  // calculate output shape
  ShapeVector selected_indices_max_shape = scores_shape_map[kShape];
  return std::make_shared<abstract::Shape>(selected_indices_max_shape);
}

abstract::ShapePtr NonMaxSuppressionWithOverlapsFrontendInferShape(const PrimitivePtr &primitive,
                                                                   const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  (void)CheckAndConvertUtils::CheckInteger("input number", input_args.size(), kEqual,
                                           kNonMaxSuppressionWithOverlapsInputsNum, prim_name);
  for (const auto &i : input_args) {
    MS_EXCEPTION_IF_NULL(i);
  }
  (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 0, kObjectTypeTensorType);
  (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 1, kObjectTypeTensorType);

  auto overlaps_shape = input_args[kInputIndex0]->GetShape()->GetShapeVector();
  auto scores_shape = input_args[kInputIndex1]->GetShape()->GetShapeVector();
  auto max_output_size_shape = input_args[kInputIndex2]->GetShape()->GetShapeVector();
  auto overlap_threshold_shape = input_args[kInputIndex3]->GetShape()->GetShapeVector();
  auto score_threshold_shape = input_args[kInputIndex4]->GetShape()->GetShapeVector();
  // support dynamic rank
  if (IsDynamicRank(overlaps_shape) || IsDynamicRank(scores_shape) || IsDynamicRank(max_output_size_shape) ||
      IsDynamicRank(overlap_threshold_shape) || IsDynamicRank(score_threshold_shape)) {
    return std::make_shared<abstract::Shape>(ShapeVector({abstract::Shape::kShapeRankAny}));
  }

  (void)CheckAndConvertUtils::CheckInteger("rank of scores", SizeToLong(scores_shape.size()), kEqual, 1, prim_name);
  if (scores_shape[0] != -1) {
    (void)CheckAndConvertUtils::CheckInteger("rank of overlaps", overlaps_shape.size(), kEqual, kOverlapsRank,
                                             prim_name);
    (void)CheckAndConvertUtils::CheckInteger("size of the second dimension of overlaps", overlaps_shape[1], kEqual,
                                             overlaps_shape[0], prim_name);
    (void)CheckAndConvertUtils::CheckInteger("length of scores", scores_shape[0], kEqual, overlaps_shape[0], prim_name);
    (void)CheckAndConvertUtils::CheckInteger("rank of max_output_size", max_output_size_shape.size(), kEqual, 0,
                                             prim_name);
    (void)CheckAndConvertUtils::CheckInteger("rank of overlap_threshold", overlap_threshold_shape.size(), kEqual, 0,
                                             prim_name);
    (void)CheckAndConvertUtils::CheckInteger("rank of score_threshold", score_threshold_shape.size(), kEqual, 0,
                                             prim_name);
  }

  // calculate output shape
  ShapeVector selected_indices_shape = {abstract::Shape::kShapeDimAny};
  return std::make_shared<abstract::Shape>(selected_indices_shape);
}

TypePtr NonMaxSuppressionWithOverlapsInferType(const PrimitivePtr &prim,
                                               const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = prim->name();
  MS_EXCEPTION_IF_NULL(prim);
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual,
                                           kNonMaxSuppressionWithOverlapsInputsNum, prim_name);
  for (const auto &i : input_args) {
    MS_EXCEPTION_IF_NULL(i);
  }

  auto overlaps_type = input_args[kInputIndex0]->GetType();
  auto scores_type = input_args[kInputIndex1]->GetType();
  auto max_output_size_type = input_args[kInputIndex2]->GetType();
  auto overlap_threshold_type = input_args[kInputIndex3]->GetType();
  auto score_threshold_type = input_args[kInputIndex4]->GetType();

  const std::set<TypePtr> valid_types = {kFloat16, kFloat32, kFloat64};
  std::map<std::string, TypePtr> args;
  (void)args.emplace("overlaps", overlaps_type);
  (void)args.emplace("scores", scores_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(args, valid_types, prim_name);
  // overlap_threshold,score_threshold must be scalar
  std::map<std::string, TypePtr> args2;
  (void)args2.emplace("overlap_threshold", overlap_threshold_type);
  (void)args2.emplace("score_threshold", score_threshold_type);
  (void)CheckAndConvertUtils::CheckScalarOrTensorTypesSame(args2, valid_types, prim_name);
  // max_output_size must be scalar
  const std::set<TypePtr> valid_types2 = {kInt32};
  std::map<std::string, TypePtr> args3;
  (void)args3.emplace("max_output_size", max_output_size_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(args3, valid_types2, prim_name);
  return max_output_size_type;
}
}  // namespace

MIND_API_OPERATOR_IMPL(NonMaxSuppressionWithOverlaps, BaseOperator);
AbstractBasePtr NonMaxSuppressionWithOverlapsInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                                   const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t input_num = 5;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infer_type = NonMaxSuppressionWithOverlapsInferType(primitive, input_args);
  auto infer_shape = NonMaxSuppressionWithOverlapsFrontendInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

// AG means auto generated
class MIND_API AGNonMaxSuppressionWithOverlapsInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return NonMaxSuppressionWithOverlapsInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return NonMaxSuppressionWithOverlapsInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return NonMaxSuppressionWithOverlapsInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(NonMaxSuppressionWithOverlaps, prim::kPrimNonMaxSuppressionWithOverlaps,
                                 AGNonMaxSuppressionWithOverlapsInfer, false);
}  // namespace ops
}  // namespace mindspore
