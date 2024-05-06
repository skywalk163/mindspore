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

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "abstract/abstract_value.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "abstract/param_validator.h"
#include "abstract/utils.h"
#include "base/base.h"
#include "ir/anf.h"
#include "ir/primitive.h"
#include "ir/scalar.h"
#include "ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/sequence_ops.h"
#include "ops/base_operator.h"
#include "ops/list_getitem.h"
#include "ops/primitive_c.h"
#include "ops/real_tuple_getitem.h"
#include "ops/tuple_get_item.h"
#include "utils/check_convert_utils.h"
#include "utils/convert_utils_base.h"
#include "utils/log_adapter.h"
#include "ops/op_utils.h"

namespace mindspore {
namespace ops {
AbstractBasePtr SequenceGetItemInnerInfer(const PrimitivePtr &primitive,
                                          const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto op_name = primitive->name();
  // Inputs: a tuple or list and a scalar whose value is an int32 number.
  constexpr int args_spec_size = 2;
  abstract::CheckArgsSize(op_name, input_args, args_spec_size);
  auto data_abs = input_args[0];
  MS_EXCEPTION_IF_NULL(data_abs);
  if (data_abs->isa<abstract::AbstractAny>()) {
    return std::make_shared<abstract::AbstractAny>();
  }
  auto queue = abstract::CheckArg<abstract::AbstractSequence>(op_name, input_args, 0);
  abstract::AbstractScalarPtr index = abstract::CheckArg<abstract::AbstractScalar>(op_name, input_args, 1);

  // For list/tuple with dynamic len, getitem can not be folded.
  if (queue->dynamic_len()) {
    // The value of dynamic_len_element_abs is kValueAny, do not need to Broaden.
    auto element_abs = queue->dynamic_len_element_abs();
    if (element_abs == nullptr) {
      MS_LOG(EXCEPTION) << "Getitem can not get element from an empty dynamic length sequence.";
    }
    return element_abs->Clone();
  }

  ValuePtr index_value = index->GetValue();
  MS_EXCEPTION_IF_NULL(index_value);
  std::size_t nelems = queue->elements().size();
  if (nelems == 0) {
    MS_EXCEPTION(ValueError) << "For primitive:'" << op_name << "', cannot get item by index from an empty sequence.";
  }
  // Input or index is variable, items shape and type should be same.
  if (index_value->ContainsValueAny()) {
    SetSequenceElementsUseFlagsRecursively(queue, true);
    if (CheckAndConvertUtils::CheckContainNestedOrIrregularSequence(input_args)) {
      // Sequence ops with nested or irregular sequence input should be convert to PyExecute node.
      return std::make_shared<abstract::AbstractAny>();
    }
    const auto &elements = queue->elements();
    CheckAndConvertUtils::CheckAbstractTypeAndShapeSame(elements, "For " + op_name + ", when index is not constant");
    auto ret = elements[0];
    MS_EXCEPTION_IF_NULL(ret);
    return abstract::AbstractBroaden(ret);
  }
  // For constant index, return input[index] of sequence.
  if (!index_value->isa<Int64Imm>()) {
    MS_EXCEPTION(IndexError) << op_name << " evaluator index should be an int64 number, but got " << index->ToString();
  }
  auto index_int64_value = GetValue<int64_t>(index_value);
  if (index_int64_value >= SizeToLong(nelems) || index_int64_value < -SizeToLong(nelems)) {
    MS_EXCEPTION(IndexError) << op_name << " evaluator index should be in range[-" << SizeToLong(nelems) << ", "
                             << SizeToLong(nelems) << "), but got " << index_int64_value << ".";
  }

  std::size_t index_unsigned_value = 0;
  if (index_int64_value >= 0) {
    index_unsigned_value = LongToSize(index_int64_value);
  } else {
    index_unsigned_value = LongToSize(index_int64_value + SizeToLong(nelems));
  }
  MS_LOG(DEBUG) << "GetItem use flags, index: " << index_unsigned_value << ", for " << queue->ToString();
  SetSequenceElementsUseFlags(queue, index_unsigned_value, true);
  return queue->elements()[index_unsigned_value];
}

class SequenceGetItemInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto op_name = primitive->name();
    constexpr int args_spec_size = 2;
    abstract::CheckArgsSize(op_name, input_args, args_spec_size);
    auto data_abs = input_args[0];
    MS_EXCEPTION_IF_NULL(data_abs);
    auto seq_shape = data_abs->GetShape()->cast<abstract::SequenceShapePtr>();
    MS_EXCEPTION_IF_NULL(seq_shape);
    auto nelems = seq_shape->size();
    auto index_int64_value = ops::GetScalarValue<int64_t>(input_args[1]->GetValue()).value();
    if (index_int64_value >= SizeToLong(nelems) || index_int64_value < -SizeToLong(nelems)) {
      MS_EXCEPTION(IndexError) << op_name << " evaluator index should be in range[-" << SizeToLong(nelems) << ", "
                               << SizeToLong(nelems) << "), but got " << index_int64_value << ".";
    }

    std::size_t index_unsigned_value = 0;
    if (index_int64_value >= 0) {
      index_unsigned_value = LongToSize(index_int64_value);
    } else {
      index_unsigned_value = LongToSize(index_int64_value + SizeToLong(nelems));
    }
    return (*seq_shape)[index_unsigned_value];
  }

  template <class T_PTR>
  TypePtr InferSequenceType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const {
    auto data_abs = input_args[0];
    MS_EXCEPTION_IF_NULL(data_abs);
    auto seq_type = data_abs->GetType()->cast<T_PTR>();
    MS_EXCEPTION_IF_NULL(seq_type);
    auto nelems = seq_type->size();
    auto index_int64_value = ops::GetScalarValue<int64_t>(input_args[1]->GetValue()).value();
    if (index_int64_value >= SizeToLong(nelems) || index_int64_value < -SizeToLong(nelems)) {
      MS_EXCEPTION(IndexError) << primitive->name() << " evaluator index should be in range[-" << SizeToLong(nelems)
                               << ", " << SizeToLong(nelems) << "), but got " << index_int64_value << ".";
    }

    std::size_t index_unsigned_value = 0;
    if (index_int64_value >= 0) {
      index_unsigned_value = LongToSize(index_int64_value);
    } else {
      index_unsigned_value = LongToSize(index_int64_value + SizeToLong(nelems));
    }
    return seq_type->elements()[index_unsigned_value];
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto op_name = primitive->name();
    constexpr int args_spec_size = 2;
    abstract::CheckArgsSize(op_name, input_args, args_spec_size);
    if (CheckAndConvertUtils::IsTuple(input_args[0])) {
      return InferSequenceType<TuplePtr>(primitive, input_args);
    }
    if (CheckAndConvertUtils::IsList(input_args[0])) {
      return InferSequenceType<ListPtr>(primitive, input_args);
    }
    MS_LOG(EXCEPTION) << "Unexpected sequence type: " << input_args[0]->ToString();
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return SequenceGetItemInnerInfer(primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {1}; }
};
MIND_API_OPERATOR_IMPL(TupleGetItem, BaseOperator);
MIND_API_OPERATOR_IMPL(RealTupleGetItem, BaseOperator);
MIND_API_OPERATOR_IMPL(list_getitem, BaseOperator);
REGISTER_PRIMITIVE_OP_INFER_IMPL(TupleGetItem, prim::kPrimTupleGetItem, SequenceGetItemInfer, false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(RealTupleGetItem, prim::kPrimRealTupleGetItem, SequenceGetItemInfer, false);
REGISTER_PRIMITIVE_OP_INFER_IMPL(list_getitem, prim::kPrimListGetItem, SequenceGetItemInfer, false);
}  // namespace ops
}  // namespace mindspore
