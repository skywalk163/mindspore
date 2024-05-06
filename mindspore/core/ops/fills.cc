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

#include "ops/fills.h"

#include <map>
#include <memory>
#include <set>

#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "ir/dtype/number.h"
#include "ir/primitive.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr FillsInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  if (CheckAndConvertUtils::IsTensor(input_args[kInputIndex1])) {
    auto value_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
    auto value_rank = SizeToLong(value_shape.size());
    (void)CheckAndConvertUtils::CheckInteger("rank of 'value'", value_rank, kEqual, 0, primitive->name());
  }
  auto x_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape());
  auto x_shape = x_shape_map[kShape];
  return std::make_shared<abstract::Shape>(x_shape);
}

TypePtr FillsInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  auto op_name = prim->name();
  auto value_type = input_args[kInputIndex1]->GetType();
  (void)CheckAndConvertUtils::CheckTypeValid("value", value_type, {kFloat32}, op_name);
  auto x_type = input_args[kInputIndex0]->GetType();
  const std::set<TypePtr> x_valid_types = {kInt8, kInt16, kInt32, kFloat16, kFloat32};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", x_type, x_valid_types, op_name);
  return x_type;
}
}  // namespace

MIND_API_OPERATOR_IMPL(Fills, BaseOperator);
AbstractBasePtr FillsInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                           const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t kInputNum = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputNum, primitive->name());
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  auto infer_type = FillsInferType(primitive, input_args);
  auto infer_shape = FillsInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

// AG means auto generated
class MIND_API AGFillsInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return FillsInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return FillsInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return FillsInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(Fills, prim::kPrimFills, AGFillsInfer, false);
}  // namespace ops
}  // namespace mindspore
