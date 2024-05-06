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

#include <algorithm>
#include <memory>
#include <string>
#include "mindapi/src/helper.h"
#include "ops/list_equal.h"
#include "ops/op_utils.h"
#include "ops/tuple_equal.h"
#include "utils/check_convert_utils.h"
#include "ir/tensor.h"

namespace mindspore {
namespace ops {
AbstractBasePtr SequenceEqualInferInner(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }
  constexpr size_t x_index = 0;
  constexpr size_t y_index = 1;
  auto x_abs = input_args[x_index];
  auto y_abs = input_args[y_index];
  if (!CheckAndConvertUtils::IsSequence(x_abs) || !CheckAndConvertUtils::IsSequence(y_abs)) {
    MS_EXCEPTION(TypeError) << "For primitive '" << prim_name << "', the input must be a list or tuple, "
                            << "but got: " << x_abs->ToString() << " and " << y_abs->ToString();
  }
  if (CheckAndConvertUtils::IsDynamicSequence(x_abs) || CheckAndConvertUtils::IsDynamicSequence(y_abs) ||
      x_abs->GetValue()->ContainsValueAny() || y_abs->GetValue()->ContainsValueAny()) {
    return std::make_shared<abstract::AbstractScalar>(kValueAny, kBool);
  }
  auto x_ptr = x_abs->GetValue();
  auto y_ptr = y_abs->GetValue();
  if (x_ptr->isa<ValueSequence>() && y_ptr->isa<ValueSequence>()) {
    auto x_sequence = x_ptr->cast<ValueSequencePtr>()->value();
    auto y_sequence = y_ptr->cast<ValueSequencePtr>()->value();
    if (x_sequence.size() != y_sequence.size()) {
      return std::make_shared<abstract::AbstractScalar>(false);
    }
    for (size_t i = 0; i < x_sequence.size(); i++) {
      MS_EXCEPTION_IF_NULL(x_sequence[i]);
      MS_EXCEPTION_IF_NULL(y_sequence[i]);
      if (x_sequence[i]->isa<tensor::Tensor>() && y_sequence[i]->isa<tensor::Tensor>()) {
        if (!x_sequence[i]->cast<tensor::TensorPtr>()->ValueEqual(*y_sequence[i]->cast<tensor::TensorPtr>())) {
          return std::make_shared<abstract::AbstractScalar>(false);
        }
      } else {
        if (!(*x_sequence[i] == *y_sequence[i])) {
          return std::make_shared<abstract::AbstractScalar>(false);
        }
      }
    }
    return std::make_shared<abstract::AbstractScalar>(true);
  }
  return std::make_shared<abstract::AbstractScalar>(false);
}

class SequenceEqualInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceEqualInferInner(primitive, input_args)->GetShape();
  }

  TypePtr InferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceEqualInferInner(prim, input_args)->GetType();
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceEqualInferInner(primitive, input_args);
  }
};
MIND_API_OPERATOR_IMPL(tuple_equal, BaseOperator);
MIND_API_OPERATOR_IMPL(list_equal, BaseOperator);
REGISTER_PRIMITIVE_OP_INFER_IMPL(tuple_equal, prim::kPrimTupleEqual, SequenceEqualInfer, false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(list_equal, prim::kPrimListEqual, SequenceEqualInfer, false);
}  // namespace ops
}  // namespace mindspore
