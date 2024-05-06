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
#include "ops/random_categorical.h"

#include <map>
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
#include "ir/dtype.h"
#include "ir/dtype/number.h"
#include "ir/dtype/type.h"
#include "ir/primitive.h"
#include "ir/tensor.h"
#include "ir/value.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/base/type_id.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/random_ops.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace ops {
namespace {
int64_t GetNumSample(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  const auto num_sample_value = GetScalarValue<int64_t>(input_args[kInputIndex1]->GetValue());
  if (!num_sample_value.has_value()) {
    MS_EXCEPTION(ValueError) << "For '" << prim->name() << "', failed to get value 'num_sample'";
  }
  auto num_sample = num_sample_value.value();
  if (num_sample <= 0) {
    MS_EXCEPTION(ValueError) << "For '" << prim->name() << "', num_sample can't has negative value, but got "
                             << num_sample;
  }
  return num_sample;
}

abstract::BaseShapePtr RandomCategoricalInferShape(const PrimitivePtr &primitive,
                                                   const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto logits_shape_ptr = input_args[kInputIndex0]->GetShape();
  if (IsDynamicRank(CheckAndConvertUtils::ConvertShapePtrToShapeMap(logits_shape_ptr)[kShape])) {
    return std::make_shared<abstract::Shape>(std::vector<int64_t>{-2});
  }
  auto logits_shape_map = CheckAndConvertUtils::ConvertShapePtrToShapeMap(logits_shape_ptr);
  auto logits_shape = logits_shape_map[kShape];
  if (logits_shape_ptr->IsDynamic()) {
    return logits_shape_ptr->Clone();
  }
  if (logits_shape.size() != kDim2) {
    MS_EXCEPTION(ValueError) << "logits shape size only support 2D";
  }
  std::vector<int64_t> output_shape;
  for (size_t i = 0; i < logits_shape.size() - 1; ++i) {
    output_shape.push_back(logits_shape.at(i));
  }
  int64_t num_sample = GetNumSample(primitive, input_args);
  if (num_sample == -1) {
    return logits_shape_ptr->Clone();
  }
  output_shape.push_back(num_sample);
  MS_LOG(INFO) << output_shape;
  return std::make_shared<abstract::Shape>(output_shape);
}

TypePtr RandomCategoricalInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(prim);
  auto prim_name = prim->name();
  const std::set<TypePtr> valid_logits_types = {kFloat16, kFloat32, kFloat64};
  (void)CheckAndConvertUtils::CheckTypeValid("logits", input_args[kInputIndex0]->GetType(), valid_logits_types,
                                             prim_name);
  const std::set<TypePtr> valid_num_sample_types = {kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTypeValid("num_sample", input_args[kInputIndex1]->GetType(), valid_num_sample_types,
                                             prim_name);
  const std::set<TypePtr> valid_seed_types = {kInt32, kInt64};
  (void)CheckAndConvertUtils::CheckTypeValid("seed", input_args[kInputIndex2]->GetType(), valid_seed_types, prim_name);
  auto dtype_value = prim->GetAttr("dtype");
  MS_EXCEPTION_IF_NULL(dtype_value);
  if (!dtype_value->isa<Type>()) {
    MS_EXCEPTION(TypeError) << "For RandomPoisson, the dtype of " + prim_name + " is invalid!";
  }
  auto output_type = dtype_value->cast<TypePtr>();
  const std::set<TypePtr> valid_data_types = {kInt16, kInt32, kInt64};
  return CheckAndConvertUtils::CheckSubClass("dtype", output_type, valid_data_types, prim_name);
}
}  // namespace

void RandomCategorical::Init(const int64_t num_sample, const int64_t seed) {
  this->set_num_sample(num_sample);
  this->set_seed(seed);
}

void RandomCategorical::set_num_sample(int64_t num_sample) {
  (void)this->AddAttr(kNumSample, api::MakeValue(num_sample));
}

int64_t RandomCategorical::get_num_sample() const {
  auto value_ptr = GetAttr(kNumSample);
  return GetValue<int64_t>(value_ptr);
}

void RandomCategorical::set_seed(int64_t seed) { (void)this->AddAttr(kSeed, api::MakeValue(seed)); }

int64_t RandomCategorical::get_seed() const {
  auto value_ptr = GetAttr(kSeed);
  return GetValue<int64_t>(value_ptr);
}

AbstractBasePtr RandomCategoricalInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                       const std::vector<AbstractBasePtr> &input_args) {
  auto infertype = RandomCategoricalInferType(primitive, input_args);
  auto infershape = RandomCategoricalInferShape(primitive, input_args);
  return abstract::MakeAbstract(infershape, infertype);
}
MIND_API_OPERATOR_IMPL(RandomCategorical, BaseOperator);

// AG means auto generated
class MIND_API AGRandomCategoricalInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return RandomCategoricalInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return RandomCategoricalInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return RandomCategoricalInfer(engine, primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {1, 2}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(RandomCategorical, prim::kPrimRandomCategorical, AGRandomCategoricalInfer, false);
}  // namespace ops
}  // namespace mindspore
