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

#include <memory>
#include <string>
#include <vector>

#include "abstract/ops/op_infer.h"
#include "include/common/utils/utils.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/arithmetic_ops.h"
#include "ops/op_utils.h"
#include "ops/scalar_bitwise_and.h"
#include "ops/scalar_bitwise_or.h"
#include "utils/check_convert_utils.h"

namespace mindspore {
namespace ops {
template <typename T>
T BitwiseImpl(const ValuePtr &x_value, const ValuePtr &y_value, const std::string &op_name) {
  MS_EXCEPTION_IF_NULL(x_value);
  MS_EXCEPTION_IF_NULL(y_value);
  auto x = GetScalarCastValue<T>(op_name, x_value);
  auto y = GetScalarCastValue<T>(op_name, y_value);
  if (op_name == mindspore::kScalarBitwiseAndOpName) {
    return x & y;
  } else {
    return x | y;
  }
}

class ScalarBitwiseInfer : public abstract::OpInferBase {
 public:
  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto prim_name = primitive->name();
    auto x_type = input_args[0]->GetType();
    auto y_type = input_args[kIndex1]->GetType();
    std::set<TypePtr> check_types = {kInt32, kInt64, kBool};
    (void)CheckAndConvertUtils::CheckSubClass("x_dtype", x_type, check_types, prim_name);
    (void)CheckAndConvertUtils::CheckSubClass("y_dtype", y_type, check_types, prim_name);
    return HighPriorityType(x_type, y_type, prim_name);
  }

  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    auto op_name = primitive->name();
    const int64_t input_len = 2;
    (void)CheckAndConvertUtils::CheckInteger("input number", SizeToLong(input_args.size()), kEqual, input_len, op_name);
    auto elem_x = input_args[0];
    auto elem_y = input_args[kIndex1];
    if (!CheckAndConvertUtils::IsScalar(elem_x) && !CheckAndConvertUtils::IsScalar(elem_y)) {
      MS_EXCEPTION(TypeError) << "For '" << op_name << "', the input should be scalar but got x: " << elem_x->ToString()
                              << " and y: " << elem_y->ToString();
    }
    return abstract::kNoShape;
  }

  ValuePtr InferValue(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const {
    MS_EXCEPTION_IF_NULL(primitive);
    const int64_t input_num = 2;
    auto op_name = primitive->name();
    CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, op_name);
    for (const auto &item : input_args) {
      MS_EXCEPTION_IF_NULL(item);
    }
    constexpr size_t x_index = 0;
    constexpr size_t y_index = 1;
    auto x_elem = input_args[x_index];
    auto y_elem = input_args[y_index];
    if (!CheckAndConvertUtils::IsScalar(x_elem) && !CheckAndConvertUtils::IsScalar(y_elem)) {
      MS_EXCEPTION(TypeError) << "For '" << op_name << "', the input should be scalar but got x: " << x_elem->ToString()
                              << " and y: " << y_elem->ToString();
    }

    auto x_value = x_elem->GetValue();
    auto y_value = y_elem->GetValue();
    if (x_value->ContainsValueAny() || y_value->ContainsValueAny()) {
      return nullptr;
    }
    auto res_type = InferType(primitive, input_args);
    ValuePtr res;
    switch (res_type->type_id()) {
      case kNumberTypeInt32: {
        res = MakeValue(BitwiseImpl<int32_t>(x_value, y_value, op_name));
        break;
      }
      case kNumberTypeInt64: {
        res = MakeValue(BitwiseImpl<int64_t>(x_value, y_value, op_name));
        break;
      }
      case kNumberTypeBool: {
        res = MakeValue(BitwiseImpl<bool>(x_value, y_value, op_name));
        break;
      }
      default: {
        MS_EXCEPTION(TypeError) << "For '" << op_name
                                << "', the supported type is in the list: [int32, int64, bool], but got "
                                << res_type->ToString() << ".";
      }
    }
    return res;
  }
};
MIND_API_OPERATOR_IMPL(bit_or, BaseOperator);
MIND_API_OPERATOR_IMPL(bit_and, BaseOperator);
REGISTER_PRIMITIVE_OP_INFER_IMPL(bit_or, prim::kPrimScalarBitwiseOr, ScalarBitwiseInfer, true);
REGISTER_PRIMITIVE_OP_INFER_IMPL(bit_and, prim::kPrimScalarBitwiseAnd, ScalarBitwiseInfer, true);
}  // namespace ops
}  // namespace mindspore
