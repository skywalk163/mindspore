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
#include <set>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/image_ops.h"
#include "ops/non_max_suppression_v3.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr NonMaxSuppressionV3InferShape(const PrimitivePtr &primitive,
                                                 const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  const int input_num = 5;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, prim_name);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 0, kObjectTypeTensorType);
  (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 1, kObjectTypeTensorType);
  auto boxes_shape = input_args[0]->GetShape()->GetShapeVector();
  auto scores_shape = input_args[1]->GetShape()->GetShapeVector();
  auto max_output_size_shape = input_args[2]->GetShape()->GetShapeVector();
  auto iou_threshold_shape = input_args[3]->GetShape()->GetShapeVector();
  auto score_threshold_shape = input_args[4]->GetShape()->GetShapeVector();
  // boxes must be rank 2
  (void)CheckAndConvertUtils::CheckInteger("boxes rank", SizeToLong(boxes_shape.size()), kEqual, 2, prim_name);
  int x_shape = static_cast<ShapeValueDType>(boxes_shape[1]);
  if (x_shape > 0) {
    // boxes second dimension must euqal 4
    (void)CheckAndConvertUtils::CheckInteger("boxes second dimension", boxes_shape[1], kEqual, 4, prim_name);
  }
  // score must be rank 1
  (void)CheckAndConvertUtils::CheckInteger("scores rank", SizeToLong(scores_shape.size()), kEqual, 1, prim_name);
  // score length must be equal with boxes first dimension
  (void)CheckAndConvertUtils::CheckInteger("scores length", scores_shape[0], kEqual, boxes_shape[0], prim_name);
  // max_output_size,iou_threshold,score_threshold must be scalar
  (void)CheckAndConvertUtils::CheckInteger("max_output_size size", SizeToLong(max_output_size_shape.size()), kEqual, 0,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("iou_threshold size", SizeToLong(iou_threshold_shape.size()), kEqual, 0,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("score_threshold size", SizeToLong(score_threshold_shape.size()), kEqual, 0,
                                           prim_name);
  auto scores_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[1]->GetShape());
  // calculate output shape
  auto selected_indices_max_shape = scores_shape_map[kShape];
  return std::make_shared<abstract::Shape>(selected_indices_max_shape);
}

abstract::ShapePtr NonMaxSuppressionV3FrontendInferShape(const PrimitivePtr &primitive,
                                                         const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  const int input_num = 5;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, prim_name);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 0, kObjectTypeTensorType);
  (void)CheckAndConvertUtils::CheckArgsType(prim_name, input_args, 1, kObjectTypeTensorType);
  auto boxes_shape = input_args[0]->GetShape()->GetShapeVector();
  auto scores_shape = input_args[1]->GetShape()->GetShapeVector();
  auto max_output_size_shape = input_args[2]->GetShape()->GetShapeVector();
  auto iou_threshold_shape = input_args[3]->GetShape()->GetShapeVector();
  auto score_threshold_shape = input_args[4]->GetShape()->GetShapeVector();
  if (IsDynamicRank(boxes_shape) || IsDynamicRank(scores_shape) || IsDynamicRank(max_output_size_shape) ||
      IsDynamicRank(iou_threshold_shape) || IsDynamicRank(score_threshold_shape)) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{abstract::TensorShape::kShapeRankAny});
  }
  // boxes must be rank 2
  (void)CheckAndConvertUtils::CheckInteger("boxes rank", SizeToLong(boxes_shape.size()), kEqual, 2, prim_name);
  int x_shape = static_cast<int32_t>(boxes_shape[1]);
  if (x_shape > 0) {
    // boxes second dimension must euqal 4
    (void)CheckAndConvertUtils::CheckInteger("boxes second dimension", boxes_shape[1], kEqual, 4, prim_name);
  }
  // score must be rank 1
  (void)CheckAndConvertUtils::CheckInteger("scores rank", SizeToLong(scores_shape.size()), kEqual, 1, prim_name);
  // score length must be equal with boxes first dimension
  (void)CheckAndConvertUtils::CheckInteger("scores length", scores_shape[0], kEqual, boxes_shape[0], prim_name);
  // max_output_size,iou_threshold,score_threshold must be scalar
  (void)CheckAndConvertUtils::CheckInteger("max_output_size size", SizeToLong(max_output_size_shape.size()), kEqual, 0,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("iou_threshold size", SizeToLong(iou_threshold_shape.size()), kEqual, 0,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("score_threshold size", SizeToLong(score_threshold_shape.size()), kEqual, 0,
                                           prim_name);
  // calculate output shape
  ShapeVector selected_indices_shape = {abstract::TensorShape::kShapeDimAny};
  return std::make_shared<abstract::Shape>(selected_indices_shape);
}

TypePtr NonMaxSuppressionV3InferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = prim->name();
  MS_EXCEPTION_IF_NULL(prim);
  const int input_num = 5;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, prim_name);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  auto boxes_type = input_args[0]->GetType();
  auto scores_type = input_args[1]->GetType();
  auto max_output_size_type = input_args[2]->GetType();
  auto iou_threshold_type = input_args[3]->GetType();
  auto score_threshold_type = input_args[4]->GetType();
  // boxes and scores must have same type
  const std::set<TypePtr> valid_types = {kFloat16, kFloat32};
  std::map<std::string, TypePtr> args;
  (void)args.insert({"boxes_type", boxes_type});
  (void)args.insert({"scores_type", scores_type});
  (void)CheckAndConvertUtils::CheckTensorTypeSame(args, valid_types, prim_name);
  // iou_threshold,score_threshold must be scalar
  std::map<std::string, TypePtr> args2;
  (void)args2.insert({"iou_threshold_type", iou_threshold_type});
  (void)args2.insert({"score_threshold_type", score_threshold_type});
  (void)CheckAndConvertUtils::CheckScalarOrTensorTypesSame(args2, valid_types, prim_name);
  // max_output_size must be scalar
  const std::set<TypePtr> valid_types1 = {kInt32, kInt64};
  std::map<std::string, TypePtr> args3;
  (void)args3.insert({"max_output_size_type", max_output_size_type});
  (void)CheckAndConvertUtils::CheckScalarOrTensorTypesSame(args3, valid_types1, prim_name);
  return max_output_size_type;
}
}  // namespace

MIND_API_OPERATOR_IMPL(NonMaxSuppressionV3, BaseOperator);
AbstractBasePtr NonMaxSuppressionV3Infer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                         const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  return abstract::MakeAbstract(NonMaxSuppressionV3FrontendInferShape(primitive, input_args),
                                NonMaxSuppressionV3InferType(primitive, input_args));
}

// AG means auto generated
class MIND_API AGNonMaxSuppressionV3Infer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return NonMaxSuppressionV3InferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return NonMaxSuppressionV3InferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return NonMaxSuppressionV3Infer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(NonMaxSuppressionV3, prim::kPrimNonMaxSuppressionV3, AGNonMaxSuppressionV3Infer,
                                 false);
}  // namespace ops
}  // namespace mindspore
