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

#include "ops/random_poisson.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/dtype/type.h"
#include "ir/named.h"
#include "ir/primitive.h"
#include "ir/value.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/random_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr RandomPoissonInferShape(const PrimitivePtr &primitive,
                                           const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex1]);
  auto shape_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->GetShape())[kShape];
  auto rate_shape = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->GetShape())[kShape];
  if (IsDynamic(shape_shape) || IsDynamicRank(rate_shape)) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
  }
  if (shape_shape.size() != 1) {
    MS_EXCEPTION(ValueError) << "For RandomPoisson, the argument[shape] must be a 1-D tensor, but got "
                             << shape_shape.size() << "-D";
  }

  auto shape_value = input_args[kInputIndex0]->GetValue();
  MS_EXCEPTION_IF_NULL(shape_value);
  if (!shape_value->isa<ValueAny>() && !shape_value->isa<None>()) {
    auto out_shape =
      CheckAndConvertUtils::CheckTensorIntValue("shape", shape_value, op_name, input_args[kInputIndex0]->GetType());
    (void)CheckAndConvertUtils::CheckPositiveVector("shape", out_shape, op_name);

    size_t rate_rank = rate_shape.size();
    for (size_t i = 0; i < rate_rank; i++) {
      if (rate_shape[i] > 0) {
        out_shape.push_back(rate_shape[i]);
      } else {
        MS_EXCEPTION(ValueError) << "For RandomPoisson, each dimension of 'rate' must be greater than 0.";
      }
    }

    return std::make_shared<abstract::Shape>(out_shape);
  } else {
    std::vector<int64_t> output_shape = {-2};
    return std::make_shared<abstract::Shape>(output_shape);
  }
}

TypePtr RandomPoissonInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
  MS_EXCEPTION_IF_NULL(input_args[kInputIndex1]);
  const std::set<TypePtr> valid_shape_types = {kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTypeValid("shape", input_args[kInputIndex0]->GetType(), valid_shape_types,
                                             prim_name);
  const std::set<TypePtr> valid_types = {kFloat16, kFloat32, kFloat64, kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTypeValid("rate", input_args[kInputIndex1]->GetType(), valid_types, prim_name);
  auto dtype_value = prim->GetAttr("dtype");
  MS_EXCEPTION_IF_NULL(dtype_value);
  if (!dtype_value->isa<Type>()) {
    MS_EXCEPTION(TypeError) << "For RandomPoisson, the dtype of " + prim_name + " is invalid!";
  }
  auto output_type = dtype_value->cast<TypePtr>();
  return CheckAndConvertUtils::CheckSubClass("dtype", output_type, valid_types, prim_name);
}
}  // namespace

int64_t RandomPoisson::get_seed() const {
  auto value_ptr = this->GetAttr(kSeed);
  return GetValue<int64_t>(value_ptr);
}

void RandomPoisson::set_seed(const int64_t seed) { (void)this->AddAttr(kSeed, api::MakeValue(seed)); }

int64_t RandomPoisson::get_seed2() const {
  auto value_ptr = this->GetAttr(kSeed2);
  return GetValue<int64_t>(value_ptr);
}

void RandomPoisson::set_seed2(const int64_t seed2) { (void)this->AddAttr(kSeed2, api::MakeValue(seed2)); }

AbstractBasePtr RandomPoissonInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                   const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t kInputsNum = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputsNum, primitive->name());
  auto infertype = RandomPoissonInferType(primitive, input_args);
  auto infershape = RandomPoissonInferShape(primitive, input_args);
  return abstract::MakeAbstract(infershape, infertype);
}
MIND_API_OPERATOR_IMPL(RandomPoisson, BaseOperator);

// AG means auto generated
class MIND_API AGRandomPoissonInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return RandomPoissonInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return RandomPoissonInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return RandomPoissonInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {0}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(RandomPoisson, prim::kPrimRandomPoisson, AGRandomPoissonInfer, false);
}  // namespace ops
}  // namespace mindspore
