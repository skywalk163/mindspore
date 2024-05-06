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
#include "ops/random_standard_laplace.h"

#include <memory>
#include <set>
#include <string>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/dtype/tensor_type.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/random_ops.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
void StandardLaplace::Init(const int64_t seed, const int64_t seed2) {
  this->set_seed(seed);
  this->set_seed2(seed2);
}

void StandardLaplace::set_seed(const int64_t seed) { (void)this->AddAttr(kSeed, api::MakeValue(seed)); }

void StandardLaplace::set_seed2(const int64_t seed2) { (void)this->AddAttr(kSeed2, api::MakeValue(seed2)); }

int64_t StandardLaplace::get_seed() const {
  auto value_ptr = GetAttr(kSeed);
  return GetValue<int64_t>(value_ptr);
}

int64_t StandardLaplace::get_seed2() const {
  auto value_ptr = GetAttr(kSeed2);
  return GetValue<int64_t>(value_ptr);
}

namespace {
abstract::ShapePtr StandardLaplaceInferShape(const PrimitivePtr &primitive,
                                             const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
  auto shape_value = input_args[kInputIndex0]->GetValue();
  MS_EXCEPTION_IF_NULL(shape_value);
  if (CheckAndConvertUtils::IsTuple(input_args[kInputIndex0])) {
    std::vector<int64_t> out_shape = CheckAndConvertUtils::CheckIntOrTupleInt("input[shape]", shape_value, prim_name);
    if (IsValueKnown(shape_value)) {
      (void)CheckAndConvertUtils::CheckPositiveVector("shape", out_shape, prim_name);
      return std::make_shared<abstract::Shape>(out_shape);
    } else {
      constexpr int dynamic_rank_value = -2;
      ShapeVector shape = {dynamic_rank_value};
      return std::make_shared<abstract::Shape>(shape);
    }
  } else if (CheckAndConvertUtils::IsTensor(input_args[kInputIndex0])) {
    if (IsValueKnown(shape_value)) {
      auto x_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
      if (x_shape.size() != 1) {
        MS_EXCEPTION(ValueError) << "For '" << prim_name
                                 << "', rank of the input Tensor shall be 1, but got: " << x_shape.size() << ".";
      }
      ShapeVector input_shape = CheckAndConvertUtils::CheckTensorIntValue("input[shape]", shape_value, prim_name,
                                                                          input_args[kInputIndex0]->GetType());
      return std::make_shared<abstract::Shape>(input_shape);
    } else {
      constexpr int dynamic_rank_value = -2;
      ShapeVector shape = {dynamic_rank_value};
      return std::make_shared<abstract::Shape>(shape);
    }
  } else {
    MS_EXCEPTION(TypeError) << "For '" << prim_name
                            << "', input must be a tuple, or a Tensor with all Int elements, but got: "
                            << input_args[kInputIndex0]->ToString() << ".";
  }
}

TypePtr StandardLaplaceInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
  if (CheckAndConvertUtils::IsTuple(input_args[kInputIndex0])) {
    auto elements_type = input_args[kInputIndex0]->GetType()->cast<TuplePtr>();
    MS_EXCEPTION_IF_NULL(elements_type);
    const std::set<TypePtr> valid_shape_types = {kInt32, kInt64};
    for (const auto &input_dtype : elements_type->elements()) {
      (void)CheckAndConvertUtils::CheckTypeValid("shape", input_dtype, valid_shape_types, prim_name);
    }
  } else if (CheckAndConvertUtils::IsTensor(input_args[kInputIndex0])) {
    const std::set<TypePtr> valid_shape_types = {kInt32, kInt64};
    auto input_dtype = input_args[kInputIndex0]->GetType();
    (void)CheckAndConvertUtils::CheckTensorTypeValid("shape", input_dtype, valid_shape_types, prim_name);
  } else {
    MS_EXCEPTION(TypeError) << "For '" << prim_name
                            << "', input must be a tuple, or a Tensor with all Int elements, but got: "
                            << input_args[kInputIndex0]->ToString() << ".";
  }
  return std::make_shared<TensorType>(kFloat32);
}
}  // namespace

AbstractBasePtr StandardLaplaceInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                     const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  const int64_t kMinInputNum = 1;
  const int64_t kMaxInputNum = 3;
  (void)CheckAndConvertUtils::CheckInteger("input numbers", SizeToLong(input_args.size()), kGreaterEqual, kMinInputNum,
                                           prim_name);
  (void)CheckAndConvertUtils::CheckInteger("input numbers", SizeToLong(input_args.size()), kLessEqual, kMaxInputNum,
                                           prim_name);
  auto type = StandardLaplaceInferType(primitive, input_args);
  auto shape = StandardLaplaceInferShape(primitive, input_args);
  return abstract::MakeAbstract(shape, type);
}

MIND_API_OPERATOR_IMPL(StandardLaplace, BaseOperator);

// AG means auto generated
class MIND_API AGStandardLaplaceInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return StandardLaplaceInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return StandardLaplaceInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return StandardLaplaceInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {0}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(StandardLaplace, prim::kPrimStandardLaplace, AGStandardLaplaceInfer, false);
}  // namespace ops
}  // namespace mindspore
