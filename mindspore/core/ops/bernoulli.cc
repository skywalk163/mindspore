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

#include "ops/bernoulli.h"
#include <algorithm>
#include <memory>
#include <vector>
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr BernoulliInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[0]->GetShape())[kShape];
  auto p_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  if (!IsDynamic(x_shape) && !IsDynamic(p_shape)) {
    if (SizeOf(p_shape) != 1 && p_shape != x_shape) {
      MS_EXCEPTION(ValueError) << "For '" << primitive->name() << "', "
                               << "'x' and 'p' should have same shape or 'p' have a size of 1.";
    }
  }
  return std::make_shared<abstract::Shape>(x_shape);
}

TypePtr BernoulliInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  auto x_type = input_args[0]->GetType();
  const std::set valid_types = {kInt8, kUInt8, kInt16, kInt32, kInt64, kBool, kFloat32, kFloat64};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", x_type, valid_types, prim_name);
  auto p_type = input_args[1]->GetType();
  const std::set p_valid_types = {kFloat32, kFloat64};
  (void)CheckAndConvertUtils::CheckTypeValid("p", p_type, p_valid_types, prim_name);
  return x_type;
}
}  // namespace

MIND_API_OPERATOR_IMPL(Bernoulli, BaseOperator);

void Bernoulli::Init(int64_t seed) { this->set_seed(seed); }

void Bernoulli::set_seed(int64_t seed) { (void)this->AddAttr(kSeed, api::MakeValue(seed)); }

int64_t Bernoulli::get_seed() const {
  auto value_ptr = this->GetAttr(kSeed);
  return GetValue<int64_t>(value_ptr);
}

AbstractBasePtr BernoulliInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t kInputsNum = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputsNum, primitive->name());
  auto infer_type = BernoulliInferType(primitive, input_args);
  auto infer_shape = BernoulliInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}

// AG means auto generated
class MIND_API AGBernoulliInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return BernoulliInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return BernoulliInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return BernoulliInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(Bernoulli, prim::kPrimBernoulli, AGBernoulliInfer, false);
}  // namespace ops
}  // namespace mindspore
