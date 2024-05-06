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
#include <algorithm>
#include <memory>
#include <string>
#include "mindapi/src/helper.h"
#include "ops/list_le.h"
#include "ops/list_lt.h"
#include "ops/op_utils.h"
#include "ops/tuple_le.h"
#include "ops/tuple_lt.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
AbstractBasePtr LessImpl(const AbstractBasePtrList &seqx_elements, const AbstractBasePtrList &seqy_elements,
                         const std::string &prim_name, const bool is_less_equal = true) {
  size_t x_size = seqx_elements.size();
  size_t y_size = seqy_elements.size();
  size_t max_size = std::max(x_size, y_size);
  if (x_size == 0 && y_size > 0) {
    return std::make_shared<abstract::AbstractScalar>(true);
  }

  for (size_t i = 0; i < max_size; ++i) {
    if (i >= x_size) {
      return std::make_shared<abstract::AbstractScalar>(true);
    }
    if (i >= y_size) {
      return std::make_shared<abstract::AbstractScalar>(false);
    }

    auto x_element = seqx_elements[i];
    auto y_element = seqy_elements[i];
    if (x_element->GetType()->type_id() == kObjectTypeTensorType ||
        y_element->GetType()->type_id() == kObjectTypeTensorType) {
      MS_EXCEPTION(TypeError) << "For primitive '" << prim_name << "', the input element must be scalar, but got "
                              << x_element->ToString() << " and " << y_element->ToString();
    }
    auto x_value = x_element->GetValue();
    auto y_value = y_element->GetValue();
    if (x_value->ContainsValueAny() || y_value->ContainsValueAny()) {
      return std::make_shared<abstract::AbstractScalar>(kValueAny, kBool);
    }

    auto x = GetScalarCastValue<double>(prim_name, x_value);
    auto y = GetScalarCastValue<double>(prim_name, y_value);
    if (x > y) {
      return std::make_shared<abstract::AbstractScalar>(false);
    } else if (x < y) {
      return std::make_shared<abstract::AbstractScalar>(true);
    }
  }
  return std::make_shared<abstract::AbstractScalar>(is_less_equal);
}

AbstractBasePtr SequenceLessInferInner(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args,
                                       const bool is_less_equal = true) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  constexpr int64_t input_num = 2;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, prim_name);
  for (const auto &item : input_args) {
    MS_EXCEPTION_IF_NULL(item);
  }

  constexpr size_t x_index = 0;
  constexpr size_t y_index = 1;
  auto x_input = input_args[x_index];
  auto y_input = input_args[y_index];
  if (!(x_input->isa<abstract::AbstractSequence>() && y_input->isa<abstract::AbstractSequence>())) {
    MS_EXCEPTION(TypeError) << "For primitive '" << prim_name << "', the input must be a list or tuple, "
                            << "but got: " << x_input->ToString() << " and " << y_input->ToString();
  }
  auto seqx_abs = x_input->cast<abstract::AbstractSequencePtr>();
  auto seqy_abs = y_input->cast<abstract::AbstractSequencePtr>();
  if (seqx_abs->dynamic_len() || seqy_abs->dynamic_len()) {
    auto dynamic_output = std::make_shared<abstract::AbstractScalar>(kValueAny, kBool);
    return dynamic_output;
  }
  const auto &seqx_elements = seqx_abs->elements();
  const auto &seqy_elements = seqy_abs->elements();

  return LessImpl(seqx_elements, seqy_elements, prim_name, is_less_equal);
}

class SequenceLessThanInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    auto x_input = input_args[kIndex0];
    auto y_input = input_args[kIndex1];
    if (!(CheckAndConvertUtils::IsSequence(x_input) && CheckAndConvertUtils::IsSequence(y_input))) {
      MS_EXCEPTION(TypeError) << "For primitive '" << prim_name << "', the input must be a list or tuple, "
                              << "but got: " << x_input->ToString() << " and " << y_input->ToString();
    }
    return abstract::kNoShape;
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    auto x_input = input_args[kIndex0];
    auto y_input = input_args[kIndex1];
    if (!(CheckAndConvertUtils::IsSequence(x_input) && CheckAndConvertUtils::IsSequence(y_input))) {
      MS_EXCEPTION(TypeError) << "For primitive '" << prim_name << "', the input must be a list or tuple, "
                              << "but got: " << x_input->ToString() << " and " << y_input->ToString();
    }
    return kBool;
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceLessInferInner(primitive, input_args, false);
  }
};

class SequenceLessEqualInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    auto x_input = input_args[kIndex0];
    auto y_input = input_args[kIndex1];
    if (!(CheckAndConvertUtils::IsSequence(x_input) && CheckAndConvertUtils::IsSequence(y_input))) {
      MS_EXCEPTION(TypeError) << "For primitive '" << prim_name << "', the input must be a list or tuple, "
                              << "but got: " << x_input->ToString() << " and " << y_input->ToString();
    }
    return abstract::kNoShape;
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    auto x_input = input_args[kIndex0];
    auto y_input = input_args[kIndex1];
    if (!(CheckAndConvertUtils::IsSequence(x_input) && CheckAndConvertUtils::IsSequence(y_input))) {
      MS_EXCEPTION(TypeError) << "For primitive '" << prim_name << "', the input must be a list or tuple, "
                              << "but got: " << x_input->ToString() << " and " << y_input->ToString();
    }
    return kBool;
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceLessInferInner(primitive, input_args);
  }
};

MIND_API_OPERATOR_IMPL(tuple_le, BaseOperator);
MIND_API_OPERATOR_IMPL(tuple_lt, BaseOperator);
MIND_API_OPERATOR_IMPL(list_le, BaseOperator);
MIND_API_OPERATOR_IMPL(list_lt, BaseOperator);
REGISTER_PRIMITIVE_OP_INFER_IMPL(tuple_le, prim::kPrimTupleLessEqual, SequenceLessEqualInfer, false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(list_le, prim::kPrimListLessEqual, SequenceLessEqualInfer, false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(tuple_lt, prim::kPrimTupleLessThan, SequenceLessThanInfer, false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(list_lt, prim::kPrimListLessThan, SequenceLessThanInfer, false);
}  // namespace ops
}  // namespace mindspore
