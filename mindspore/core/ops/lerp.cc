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

#include "ops/lerp.h"
#include <algorithm>
#include <map>
#include <string>
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr LerpInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  const int64_t input_num = 3;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, op_name);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  auto start_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape());
  auto start_shape = start_shape_map[kShape];
  auto end_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape());
  auto end_shape = end_shape_map[kShape];
  auto weight_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->GetShape());
  auto weight_shape = weight_shape_map[kShape];
  auto broadcast_shape = CalBroadCastShape(start_shape, end_shape, op_name, "start", "end");
  if (input_args[kInputIndex2]->GetType()->object_type() == kObjectTypeTensorType) {
    (void)CalBroadCastShape(start_shape, weight_shape, op_name, "start", "weight");
    (void)CalBroadCastShape(end_shape, weight_shape, op_name, "end", "weight");
    broadcast_shape = CalBroadCastShape(broadcast_shape, weight_shape, op_name);
  }
  if (IsDynamicRank(weight_shape) || IsDynamicRank(start_shape) || IsDynamicRank(end_shape)) {
    return std::make_shared<abstract::Shape>(broadcast_shape);
  }
  // Do additional check for the rank of weight for static rank case only.
  if (weight_shape.size() > start_shape.size() && weight_shape.size() > end_shape.size()) {
    MS_EXCEPTION(RuntimeError) << "weight should be of dimension max(self.dim(), end.dim()) or lesser.";
  }
  return std::make_shared<abstract::Shape>(broadcast_shape);
}

TypePtr LerpInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  auto op_name = prim->name();
  const int64_t input_num = 3;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_num, op_name);
  std::map<std::string, TypePtr> types;
  (void)types.emplace("start", input_args[0]->GetType());
  (void)types.emplace("end", input_args[1]->GetType());
  if (input_args[kInputIndex2]->GetType()->object_type() == kObjectTypeTensorType) {
    (void)types.emplace("weight", input_args[kInputIndex2]->GetType());
  } else {
    (void)CheckAndConvertUtils::CheckSubClass("weight", input_args[kInputIndex2]->GetType(), {kFloat}, op_name);
  }
  return CheckAndConvertUtils::CheckTensorTypeSame(types, {kFloat16, kFloat32, kFloat64}, op_name);
}
}  // namespace

MIND_API_OPERATOR_IMPL(Lerp, BaseOperator);
AbstractBasePtr LerpInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) {
  auto lerp_output_data_type = LerpInferType(primitive, input_args);
  auto lerp_output_shape = LerpInferShape(primitive, input_args)->shape();
  return std::make_shared<abstract::AbstractTensor>(lerp_output_data_type, lerp_output_shape);
}

// AG means auto generated
class MIND_API AGLerpInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return LerpInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return LerpInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return LerpInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(Lerp, prim::kPrimLerp, AGLerpInfer, false);
}  // namespace ops
}  // namespace mindspore
