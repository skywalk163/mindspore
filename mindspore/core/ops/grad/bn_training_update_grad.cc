/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "ops/grad/bn_training_update_grad.h"

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/container.h"
#include "ir/primitive.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/nn_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
namespace {
constexpr auto kBNTrainingUpdateGradInputNum = 4;

abstract::TupleShapePtr BNTrainingUpdateGradInferShape(const PrimitivePtr &primitive,
                                                       const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, kBNTrainingUpdateGradInputNum, prim_name);
  auto batch_mean_shape_ptr = input_args[kInputIndex2]->GetShape();
  auto batch_variance_shape_ptr = input_args[kInputIndex3]->GetShape();
  return std::make_shared<abstract::TupleShape>(
    std::vector<abstract::BaseShapePtr>{batch_mean_shape_ptr, batch_variance_shape_ptr});
}

TuplePtr BNTrainingUpdateGradInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  CheckAndConvertUtils::CheckInputArgs(input_args, kGreaterEqual, kBNTrainingUpdateGradInputNum, prim_name);
  auto batch_mean_type_ptr = input_args[kInputIndex2]->GetType();
  auto batch_variance_type_ptr = input_args[kInputIndex3]->GetType();
  return std::make_shared<Tuple>(std::vector<TypePtr>{batch_mean_type_ptr, batch_variance_type_ptr});
}
}  // namespace

MIND_API_OPERATOR_IMPL(BNTrainingUpdateGrad, BaseOperator);
AbstractBasePtr BNTrainingUpdateGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                          const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  return abstract::MakeAbstract(BNTrainingUpdateGradInferShape(primitive, input_args),
                                BNTrainingUpdateGradInferType(primitive, input_args));
}

// AG means auto generated
class MIND_API AGBNTrainingUpdateGradInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return BNTrainingUpdateGradInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return BNTrainingUpdateGradInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return BNTrainingUpdateGradInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(BNTrainingUpdateGrad, prim::kPrimBNTrainingUpdateGrad, AGBNTrainingUpdateGradInfer,
                                 false);
}  // namespace ops
}  // namespace mindspore
