/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include "ops/sequence_addn.h"
#include <memory>
#include <string>
#include <vector>
#include "abstract/abstract_value.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/primitive.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sequence_ops.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
namespace {

BaseShapePtr SequenceAddNInferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  auto queue_shape = input_args[kIndex0]->GetShape()->cast<abstract::SequenceShapePtr>();
  MS_EXCEPTION_IF_NULL(queue_shape);
  return queue_shape->shape()[kIndex0]->Clone();
}

TypePtr SequenceAddNInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  if (CheckAndConvertUtils::IsTuple(input_args[kIndex0])) {
    auto queue_type = input_args[kIndex0]->GetType()->cast<TuplePtr>();
    MS_EXCEPTION_IF_NULL(queue_type);
    return queue_type->elements()[kIndex0]->Clone();
  } else {
    auto queue_type = input_args[kIndex0]->GetType()->cast<ListPtr>();
    MS_EXCEPTION_IF_NULL(queue_type);
    return queue_type->elements()[kIndex0]->Clone();
  }
}

AbstractBasePtr SequenceAddNInferInner(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  // Inputs: a tuple or list and a scalar whose value is an int32 number.
  constexpr int args_spec_size = 1;
  abstract::CheckArgsSize(op_name, input_args, args_spec_size);
  auto queue = abstract::CheckArg<abstract::AbstractSequence>(op_name, input_args, 0);

  // The value of dynamic_len_element_abs is kValueAny, do not need to Broaden.
  if (queue->dynamic_len()) {
    auto element_abs = queue->dynamic_len_element_abs();
    MS_EXCEPTION_IF_NULL(element_abs);
    return element_abs->Clone();
  }

  if (queue->elements().size() == 0) {
    MS_LOG(EXCEPTION) << "Sequence length should not be 0.";
  }
  return queue->elements()[0];
}
}  // namespace
MIND_API_OPERATOR_IMPL(SequenceAddN, BaseOperator);
class SequenceAddNInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceAddNInferShape(primitive, input_args);
  }

  TypePtr InferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceAddNInferType(prim, input_args);
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceAddNInferInner(primitive, input_args);
  }
};
REGISTER_PRIMITIVE_OP_INFER_IMPL(SequenceAddN, prim::kPrimSequenceAddN, SequenceAddNInfer, false);
}  // namespace ops
}  // namespace mindspore
