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

#include "ops/combined_non_max_suppression.h"

#include <algorithm>
#include <set>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/container.h"
#include "ir/dtype/number.h"
#include "ir/dtype/tensor_type.h"
#include "ir/named.h"
#include "ir/primitive.h"
#include "ir/tensor.h"
#include "ir/value.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/image_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
const int64_t kInputDimension0 = 4;
const int64_t kInputDimension1 = 3;
const int64_t kDimsize = 4;
const int64_t kInputs = 6;
const size_t ksecond = 2;
void CombinedNonMaxSuppressionCheckShapeSize(const ShapeVector &input0_shape, const ShapeVector &input1_shape,
                                             const ShapeVector &input2_shape, const ShapeVector &input3_shape,
                                             const ShapeVector &input4_shape, const ShapeVector &input5_shape,
                                             const bool &is_dynamic_rank, const std::string &prim_name) {
  if (is_dynamic_rank) {
    return;
  }
  (void)CheckAndConvertUtils::CheckInteger("boxes dim", SizeToLong(input0_shape.size()), kEqual, kInputDimension0,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("scores dim", SizeToLong(input1_shape.size()), kEqual, kInputDimension1,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("max_output_size_per_class dim", SizeToLong(input2_shape.size()), kEqual, 0,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("max_total_size dim", SizeToLong(input3_shape.size()), kEqual, 0, prim_name);
  (void)CheckAndConvertUtils::CheckInteger("iou_threshold", SizeToLong(input4_shape.size()), kEqual, 0, prim_name);
  (void)CheckAndConvertUtils::CheckInteger("score_threshold", SizeToLong(input5_shape.size()), kEqual, 0, prim_name);
}

void CombinedNonMaxSuppressionCheckShapeValue(const ShapeVector &input0_shape, const ShapeVector &input1_shape,
                                              const bool &is_dynamic, const std::string &prim_name) {
  if (is_dynamic) {
    return;
  }
  if (input0_shape[0] != input1_shape[0]) {
    MS_EXCEPTION(ValueError) << "For " << prim_name << ", the boxes's 1st dim must be same with the scores's"
                             << " 1st dim, but got" << input0_shape[0] << " and " << input1_shape[0] << ".";
  }
  if (input0_shape[1] != input1_shape[1]) {
    MS_EXCEPTION(ValueError) << "For " << prim_name << ", the boxes's 2nd dim must be same with the scores's"
                             << " 2nd dim, but got" << input0_shape[1] << " and " << input1_shape[1] << ".";
  }
  if (input0_shape[kInputIndex2] != input1_shape[kInputIndex2] && input0_shape[kInputIndex2] != 1) {
    MS_EXCEPTION(ValueError) << "For " << prim_name
                             << ", the boxes's 3rd dim is must be same with the scores's 3rd dim or 1, but got "
                             << input0_shape[kInputIndex2] << ".";
  }
  if (input0_shape[kInputIndex3] != kDimsize) {
    MS_EXCEPTION(ValueError) << "For " << prim_name << ", the boxes's 4th dim must be equal to 4, but got"
                             << input0_shape[kInputIndex3] << ".";
  }
}

abstract::TupleShapePtr CombinedNonMaxSuppressionGetOutputShape(const PrimitivePtr &primitive,
                                                                const std::vector<AbstractBasePtr> &input_args,
                                                                const bool &is_dynamic) {
  auto input0_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto input1_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto pad_per_class_ptr = primitive->GetAttr("pad_per_class");
  MS_EXCEPTION_IF_NULL(pad_per_class_ptr);
  bool pad_per_class = GetValue<bool>(pad_per_class_ptr);

  auto max_output_size_per_class_opt = GetScalarValue<int>(input_args[kInputIndex2]->GetValue());
  auto max_total_size_opt = GetScalarValue<int>(input_args[kInputIndex3]->GetValue());
  if (!is_dynamic && max_output_size_per_class_opt.has_value() && max_total_size_opt.has_value()) {
    auto max_output_size_per_class = max_output_size_per_class_opt.value();
    auto max_total_size = max_total_size_opt.value();

    const int32_t kNumZero = 0;
    (void)CheckAndConvertUtils::CheckInteger("max_total_size", max_total_size, kGreaterThan, kNumZero,
                                             primitive->name());
    (void)CheckAndConvertUtils::CheckInteger("max_output_size_per_clas", max_output_size_per_class, kGreaterThan,
                                             kNumZero, primitive->name());

    auto num_detection = max_total_size;
    if (pad_per_class) {
      num_detection = std::min(max_total_size, max_output_size_per_class * static_cast<int32_t>(input1_shape[ksecond]));
    }
    int64_t bs = input0_shape[0];
    ShapeVector shape1 = {bs, num_detection, 4};
    ShapeVector shape2 = {bs, num_detection};
    ShapeVector shape3 = {bs, num_detection};
    ShapeVector shape4 = {bs};
    auto out1 = std::make_shared<abstract::Shape>(shape1);
    auto out2 = std::make_shared<abstract::Shape>(shape2);
    auto out3 = std::make_shared<abstract::Shape>(shape3);
    auto out4 = std::make_shared<abstract::Shape>(shape4);
    return std::make_shared<abstract::TupleShape>(std::vector<abstract::BaseShapePtr>{out1, out2, out3, out4});
  } else {
    auto shape1 = std::make_shared<abstract::Shape>(ShapeVector{-1, -1, 4});
    auto shape2 = std::make_shared<abstract::Shape>(ShapeVector{-1, -1});
    auto shape3 = std::make_shared<abstract::Shape>(ShapeVector{-1, -1});
    auto shape4 = std::make_shared<abstract::Shape>(ShapeVector{-1});
    return std::make_shared<abstract::TupleShape>(std::vector<abstract::BaseShapePtr>{shape1, shape2, shape3, shape4});
  }
}

void CheckCombinedNonMaxSuppressionThreshold(const AbstractBasePtr &iou_arg, const AbstractBasePtr &score_arg,
                                             const std::string &prim_name, const ShapeVector &input0_shape,
                                             const ShapeVector &input1_shape, bool is_dynamic) {
  auto iou_opt = GetScalarValue<float>(iou_arg->GetValue());
  if (iou_opt.has_value()) {
    auto iou_threshold = iou_opt.value();
    if (iou_threshold < 0 || iou_threshold > 1) {
      MS_EXCEPTION(ValueError) << "For " << prim_name << ", iou_threshold must be in [0,1], but got " << iou_threshold
                               << ".";
    }
  }
  auto score_opt = GetScalarValue<float>(score_arg->GetValue());
  if (score_opt.has_value()) {
    auto score_threshold = score_opt.value();
    if (score_threshold < 0 && !is_dynamic && input0_shape[kInputIndex2] == input1_shape[kInputIndex2]) {
      MS_EXCEPTION(ValueError) << "For " << prim_name << ", it is temporarily unsupported when boxes's 2'nd dim "
                               << "is not 1 and score_threshold is less than 1.";
    }
  }
}

abstract::TupleShapePtr CombinedNonMaxSuppressionInferShape(const PrimitivePtr &primitive,
                                                            const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();
  auto input0_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto input1_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  auto input2_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape())[kShape];
  auto input3_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex3]->GetShape())[kShape];
  auto input4_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex4]->GetShape())[kShape];
  auto input5_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex5]->GetShape())[kShape];

  std::vector<ShapeVector> all_shapes = {input0_shape, input1_shape, input2_shape,
                                         input3_shape, input4_shape, input5_shape};
  auto is_dynamic = (IsDynamic(input0_shape) || IsDynamic(input1_shape));
  auto is_dynamic_rank = std::any_of(all_shapes.begin(), all_shapes.end(), IsDynamicRank);
  CombinedNonMaxSuppressionCheckShapeSize(input0_shape, input1_shape, input2_shape, input3_shape, input4_shape,
                                          input5_shape, is_dynamic_rank, prim_name);

  CombinedNonMaxSuppressionCheckShapeValue(input0_shape, input1_shape, is_dynamic, prim_name);

  for (int64_t i = 0; i < kInputs; i++) {
    if (!CheckAndConvertUtils::IsTensor(input_args[LongToSize(i)])) {
      MS_EXCEPTION(TypeError) << "For " << prim_name << " input" << i << " only support tensor!";
    }
  }
  CheckCombinedNonMaxSuppressionThreshold(input_args[kInputIndex4], input_args[kInputIndex5], prim_name, input0_shape,
                                          input1_shape, is_dynamic);

  return CombinedNonMaxSuppressionGetOutputShape(primitive, input_args, is_dynamic);
}

TuplePtr CombinedNonMaxSuppressionInferType(const PrimitivePtr &primitive,
                                            const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();
  auto input0_type = input_args[kInputIndex0]->GetType();
  auto input1_type = input_args[kInputIndex1]->GetType();
  auto input2_type = input_args[kInputIndex2]->GetType();
  auto input3_type = input_args[kInputIndex3]->GetType();
  auto input4_type = input_args[kInputIndex4]->GetType();
  auto input5_type = input_args[kInputIndex5]->GetType();
  const std::set valid_type_float32 = {kFloat32};
  const std::set valid_type_int = {kInt32};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("boxes", input0_type, valid_type_float32, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("scores", input1_type, valid_type_float32, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("max_output_size_per_class", input2_type, valid_type_int, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("max_total_size", input3_type, valid_type_int, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("iou_threshold", input4_type, valid_type_float32, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("score_threshold", input5_type, valid_type_float32, prim_name);
  return std::make_shared<Tuple>(
    std::vector<TypePtr>{std::make_shared<TensorType>(kFloat32), std::make_shared<TensorType>(kFloat32),
                         std::make_shared<TensorType>(kFloat32), std::make_shared<TensorType>(kInt32)});
}
}  // namespace
MIND_API_OPERATOR_IMPL(CombinedNonMaxSuppression, BaseOperator);
AbstractBasePtr CombinedNonMaxSuppressionInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                               const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();
  const int64_t kInputNum = 6;
  CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, kInputNum, prim_name);
  auto infer_shape = CombinedNonMaxSuppressionInferShape(primitive, input_args);
  auto infer_type = CombinedNonMaxSuppressionInferType(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

bool CombinedNonMaxSuppression::get_pad_per_class() const {
  auto value_ptr = this->GetAttr("pad_per_class");
  return GetValue<bool>(value_ptr);
}

bool CombinedNonMaxSuppression::get_clip_boxes() const {
  auto value_ptr = this->GetAttr("clip_boxes");
  return GetValue<bool>(value_ptr);
}

// AG means auto generated
class MIND_API AGCombinedNonMaxSuppressionInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return CombinedNonMaxSuppressionInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return CombinedNonMaxSuppressionInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return CombinedNonMaxSuppressionInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {2, 3, 4, 5}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(CombinedNonMaxSuppression, prim::kPrimCombinedNonMaxSuppression,
                                 AGCombinedNonMaxSuppressionInfer, false);
}  // namespace ops
}  // namespace mindspore
