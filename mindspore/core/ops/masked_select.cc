/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#include "ops/masked_select.h"

#include <functional>
#include <iostream>
#include <map>
#include <string>

#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "ops/array_ops.h"
#include "ops/math_ops.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/ms_context.h"

namespace mindspore {
namespace ops {
namespace {
constexpr int64_t kMaskedSelectInputNum = 2;

abstract::ShapePtr MaskedSelectFrontendInferShape(const PrimitivePtr &primitive,
                                                  const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kMaskedSelectInputNum, op_name);
  ShapeVector output_shape = {abstract::Shape::kShapeDimAny};
  return std::make_shared<abstract::TensorShape>(output_shape);
}

TypePtr MaskedSelectFrontendInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto op_name = prim->name();
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kMaskedSelectInputNum, op_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("mask", input_args[1]->GetType(), {kBool}, op_name);
  std::map<std::string, TypePtr> types;
  (void)types.emplace("input", input_args[kInputIndex0]->GetType());
  bool is_ascend = (context->get_param<std::string>(MS_CTX_DEVICE_TARGET) == kAscendDevice);
  if (is_ascend) {
    const std::set<TypePtr> ascend_valid_types = {kInt8,   kInt16,  kInt32,   kInt64, kUInt8,   kUInt16,
                                                  kUInt32, kUInt64, kFloat16, kFloat, kFloat64, kBool};
    return CheckAndConvertUtils::CheckTensorTypeSame(types, ascend_valid_types, op_name);
  }
  const std::set<TypePtr> valid_types = {kInt8,   kInt16,   kInt32, kInt64,   kUInt8, kUInt16,    kUInt32,
                                         kUInt64, kFloat16, kFloat, kFloat64, kBool,  kComplex64, kComplex128};
  return CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types, op_name);
}
}  // namespace

AbstractBasePtr MaskedSelectInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                  const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kMaskedSelectInputNum, primitive->name());
  auto infer_shape = MaskedSelectFrontendInferShape(primitive, input_args);
  auto infer_type = MaskedSelectFrontendInferType(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

MIND_API_OPERATOR_IMPL(MaskedSelect, BaseOperator);

// AG means auto generated
class MIND_API AGMaskedSelectInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto op_name = primitive->name();
    CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kMaskedSelectInputNum, op_name);

    auto x_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape());
    auto y_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape());
    auto x_shape = x_shape_map[kShape];
    auto y_shape = y_shape_map[kShape];

    auto broadcast_shape = CalBroadCastShape(x_shape, y_shape, op_name, "input", "mask");
    int64_t num = std::accumulate(broadcast_shape.begin(), broadcast_shape.end(), 1, std::multiplies<int64_t>());
    ShapeVector real_shape = {num};
    return std::make_shared<abstract::Shape>(real_shape);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return MaskedSelectFrontendInferType(primitive, input_args);
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return MaskedSelectInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(MaskedSelect, prim::kPrimMaskedSelect, AGMaskedSelectInfer, false);
}  // namespace ops
}  // namespace mindspore
