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

#include "ops/equal_count.h"

#include <memory>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/param_validator.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/primitive.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/comparison_ops.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr EqualCountInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  auto input0 = CheckAndConvertUtils::CheckArgsType(op_name, input_args, 0, kObjectTypeTensorType);
  auto input1 = CheckAndConvertUtils::CheckArgsType(op_name, input_args, 1, kObjectTypeTensorType);
  abstract::CheckShapeSame(op_name, input0, input1);

  std::vector<int64_t> out_shape = {1};
  return std::make_shared<abstract::Shape>(out_shape);
}

TypePtr EqualCountInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  auto x = CheckAndConvertUtils::CheckArgsType(prim->name(), input_args, 0, kObjectTypeTensorType);
  auto y = CheckAndConvertUtils::CheckArgsType(prim->name(), input_args, 1, kObjectTypeTensorType);
  (void)abstract::CheckDtypeSame(prim->name(), x, y);

  return input_args[0]->GetType();
}
}  // namespace

MIND_API_OPERATOR_IMPL(EqualCount, BaseOperator);
AbstractBasePtr EqualCountInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                const std::vector<AbstractBasePtr> &input_args) {
  auto type = EqualCountInferType(primitive, input_args);
  auto shape = EqualCountInferShape(primitive, input_args);
  return abstract::MakeAbstract(shape, type);
}

// AG means auto generated
class MIND_API AGEqualCountInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return EqualCountInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return EqualCountInferType(primitive, input_args);
  }
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return EqualCountInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(EqualCount, prim::kPrimEqualCount, AGEqualCountInfer, false);
}  // namespace ops
}  // namespace mindspore
