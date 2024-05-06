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

#include "ops/make_range.h"
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "abstract/abstract_value.h"
#include "abstract/ops/op_infer.h"
#include "abstract/ops/primitive_infer_map.h"
#include "base/base.h"
#include "include/common/utils/utils.h"
#include "ir/anf.h"
#include "ir/dtype/number.h"
#include "ir/dtype/type.h"
#include "ir/primitive.h"
#include "ir/scalar.h"
#include "ir/value.h"
#include "ops/op_utils.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/array_ops.h"
#include "ops/primitive_c.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
namespace {
bool CheckMakeRangeInput(const std::vector<AbstractBasePtr> &input_args, const std::string &prim_name) {
  constexpr size_t max_args_size = 3;
  constexpr size_t min_args_size = 1;
  auto inputs_size = input_args.size();
  if (inputs_size > max_args_size || inputs_size < min_args_size) {
    MS_LOG(EXCEPTION) << "For '" << prim_name << "', the input size should within [" << min_args_size << ", "
                      << max_args_size << "] but got" << inputs_size;
  }
  bool has_variable = false;
  for (size_t i = 0; i < input_args.size(); ++i) {
    auto element = input_args[i];
    MS_EXCEPTION_IF_NULL(element);
    auto element_type = element->GetType();
    if (element_type->type_id() != kInt64->type_id() && element_type->type_id() != kInt32->type_id()) {
      MS_EXCEPTION(TypeError) << "For '" << prim_name << "', the " << i << "th input should be a int scalar but got "
                              << element->ToString();
    }
    if (!has_variable && element->GetValue()->ContainsValueAny()) {
      has_variable = true;
    }
  }
  return has_variable;
}

abstract::AbstractTuplePtr CalcSlidePara(const std::vector<int64_t> &values, const std::string &prim_name,
                                         const TypePtr &type) {
  auto values_size = values.size();
  int64_t start = values_size == kIndex1 ? 0LL : values[kIndex0];
  int64_t stop = values_size == kIndex1 ? values[kIndex0] : values[kIndex1];
  int64_t step = values_size <= kIndex2 ? 1LL : values[kIndex2];

  if (step == 0) {
    MS_LOG(EXCEPTION) << "For 'range', the argument 'step' could not be 0.";
  }

  AbstractBasePtrList args;
  if (start <= stop) {
    if (step <= 0) {
      return std::make_shared<abstract::AbstractTuple>(args);
    }

    for (int64_t i = start; i < stop; i += step) {
      args.push_back(std::make_shared<abstract::AbstractScalar>(MakeValue(i), type));
      if (i > 0 && INT_MAX - i < step) {
        MS_EXCEPTION(ValueError) << "Integer overflow error occurred when traversing the range. "
                                 << "Please check the inputs of range.";
      }
    }
  } else {
    if (step >= 0) {
      return std::make_shared<abstract::AbstractTuple>(args);
    }

    for (int64_t i = start; i > stop; i += step) {
      args.push_back(std::make_shared<abstract::AbstractScalar>(MakeValue(i), type));
      if (i < 0 && INT_MIN - i > step) {
        MS_EXCEPTION(ValueError) << "Integer overflow error occurred when traversing the range. "
                                 << "Please check the inputs of range.";
      }
    }
  }
  return std::make_shared<abstract::AbstractTuple>(args);
}

AbstractBasePtr InferImplMakeRange(const PrimitivePtr &primitive, const AbstractBasePtrList &args_spec_list) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  bool has_variable = CheckMakeRangeInput(args_spec_list, prim_name);
  auto type = args_spec_list[0]->GetType();
  if (has_variable) {
    // If the input to make_range has variable input, the output abs should be dynamic length sequence.
    auto element = std::make_shared<abstract::AbstractScalar>(kValueAny, type);
    auto ret = std::make_shared<abstract::AbstractTuple>(AbstractBasePtrList{element});
    ret->CheckAndConvertToDynamicLenSequence();
    return ret;
  }
  std::vector<int64_t> values;
  for (size_t i = 0; i < args_spec_list.size(); ++i) {
    auto element = args_spec_list[i];
    auto element_type = element->GetType();
    auto element_val = element->GetValue();
    if (element_type->type_id() == kNumberTypeInt64) {
      values.push_back(GetScalarValue<int64_t>(element_val).value());
    } else if (element_type->type_id() == kNumberTypeInt32) {
      values.push_back(GetScalarValue<int>(element_val).value());
    } else {
      MS_EXCEPTION(TypeError) << "For '" << prim_name << "', the " << i << "th input should be a int scalar but got "
                              << element->ToString();
    }
  }
  return CalcSlidePara(values, prim_name, type);
}
}  // namespace

MIND_API_OPERATOR_IMPL(make_range, BaseOperator);

// AG means auto generated
class MIND_API AGMakeRangeInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return InferImplMakeRange(primitive, input_args)->GetShape();
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return InferImplMakeRange(primitive, input_args)->GetType();
  }

  AbstractBasePtr InferShapeAndType(const abstract::AnalysisEnginePtr &engine, const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const override {
    return InferImplMakeRange(primitive, input_args);
  }

  std::set<int64_t> GetValueDependArgIndices() const override { return {0, 1, 2}; }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(make_range, prim::kPrimMakeRange, AGMakeRangeInfer, false);
}  // namespace ops
}  // namespace mindspore
