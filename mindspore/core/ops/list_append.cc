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

#include "ops/list_append.h"

#include <memory>
#include <string>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/primitive.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sequence_ops.h"
#include "ops/primitive_c.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
AbstractBasePtr ListAppendInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  constexpr size_t input_len = 2;
  constexpr size_t data_index = 0;
  constexpr size_t target_index = 1;
  (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_len, prim_name);
  auto data_abs = dyn_cast<abstract::AbstractSequence>(input_args[data_index]);
  MS_EXCEPTION_IF_NULL(data_abs);
  auto target_abs = input_args[target_index];
  if (!data_abs->isa<abstract::AbstractSequence>() ||
      (!target_abs->isa<abstract::AbstractScalar>() && !target_abs->isa<abstract::AbstractTensor>())) {
    MS_EXCEPTION(TypeError) << "The prim '" << prim_name
                            << "', the input_data must be list and target must be scalar or tensor, but got "
                            << data_abs->ToString() << " target is " << target_abs->ToString();
  }

  if (data_abs->dynamic_len()) {
    auto data_element_abs = data_abs->dynamic_len_element_abs();
    if (data_element_abs == nullptr) {
      // The element type of the dynamic length sequence is not determined before list append.
      // Fix the element abstract as the target element
      auto ret = data_abs->Clone();
      ret->cast<abstract::AbstractListPtr>()->set_dynamic_len_element_abs(target_abs);
      return ret;
    }
    // If abstract of element is fixed, the abstract of target should have the same shape and type with the
    // abstract of element.
    CheckAndConvertUtils::CheckAbstractTypeAndShapeSame({data_element_abs, target_abs},
                                                        "For " + prim::kPrimListAppend->ToString(),
                                                        "mutable list existing item", "new added item");
    return data_abs->Clone();
  }

  const auto &elements = data_abs->elements();
  abstract::AbstractBasePtrList abs;
  if (elements.empty()) {
    abs.push_back(target_abs);
    return std::make_shared<abstract::AbstractList>(abs);
  }
  auto first_element = elements[0];
  CheckAndConvertUtils::CheckAbstractTypeAndShapeSame(
    {first_element, target_abs}, "For " + prim::kPrimListAppend->ToString(), "list existing item", "new added item");
  for (size_t i = 0; i < data_abs->size(); ++i) {
    abs.push_back(data_abs->elements()[i]);
  }
  abs.push_back(target_abs);
  return std::make_shared<abstract::AbstractList>(abs);
}
MIND_API_OPERATOR_IMPL(ListAppend, BaseOperator);

class MIND_API AGListAppendInfer : public abstract::OpInferBase {
 public:
  // This is used for backend infer by kernel tensor.
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    auto seq_input = input_args[kIndex0];
    auto item_input = input_args[kIndex1];

    auto seq_shape = seq_input->GetShape();
    MS_EXCEPTION_IF_NULL(seq_shape);
    auto list_shape = seq_shape->cast<abstract::SequenceShapePtr>();
    if (list_shape == nullptr) {
      MS_LOG(EXCEPTION) << "Invalid shape, need list:" << seq_shape->ToString();
    }
    auto item_shape = item_input->GetShape();
    auto list_shape_elements = list_shape->shape();
    list_shape_elements.emplace_back(item_shape->Clone());
    return std::make_shared<abstract::ListShape>(list_shape_elements);
  }

  // This is used for backend infer by kernel tensor.
  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    auto seq_input = input_args[kIndex0];
    auto item_input = input_args[kIndex1];

    auto seq_type = seq_input->GetType();
    MS_EXCEPTION_IF_NULL(seq_type);
    auto list_type = seq_type->cast<ListPtr>();
    MS_EXCEPTION_IF_NULL(list_type);
    auto item_type = item_input->GetType();
    list_type->elements().emplace_back(item_type->Clone());
    return std::make_shared<List>(list_type->elements());
  }

  // This is used for frontend infer by abstract.
  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return ListAppendInfer(engine, primitive, input_args);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(ListAppend, prim::kPrimListAppend, AGListAppendInfer, false);
}  // namespace ops
}  // namespace mindspore
