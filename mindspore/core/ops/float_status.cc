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

#include <memory>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/dshape.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/dtype/tensor_type.h"
#include "ir/primitive.h"
#include "mindapi/base/shape_vector.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/math_ops.h"
#include "ops/float_status.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(FloatStatus, BaseOperator);
class FloatStatusInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, 1L, primitive->name());
    MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
    ShapeVector shape = {1};
    return std::make_shared<abstract::Shape>(shape);
  }

  TypePtr InferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) const override {
    CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, 1L, prim->name());
    MS_EXCEPTION_IF_NULL(input_args[kInputIndex0]);
    (void)CheckAndConvertUtils::CheckTensorTypeValid("x", input_args[0]->GetType(), {kFloat16, kFloat32, kFloat64},
                                                     prim->name());
    return std::make_shared<TensorType>(kFloat32);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(FloatStatus, prim::kPrimFloatStatus, FloatStatusInfer, false);
}  // namespace ops
}  // namespace mindspore
