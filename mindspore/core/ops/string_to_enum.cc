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

#include "ops/string_to_enum.h"

#include <memory>
#include <string>
#include <vector>

#include "abstract/ops/primitive_infer_map.h"
#include "ir/dtype/tensor_type.h"
#include "ir/primitive.h"
#include "utils/log_adapter.h"
#include "utils/check_convert_utils.h"
#include "ops/other_ops.h"
#include "mindapi/src/helper.h"
#include "ops/op_enum.h"
#include "ops/op_utils.h"

namespace mindspore {
namespace ops {

MIND_API_OPERATOR_IMPL(StringToEnum, BaseOperator);
class MIND_API StringToEnumInfer : public abstract::OpInferBase {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive,
                          const std::vector<AbstractBasePtr> &input_args) const override {
    return std::make_shared<abstract::NoShape>();
  }

  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    return kInt64;
  }

  ValuePtr InferValue(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    MS_EXCEPTION_IF_NULL(primitive);
    constexpr size_t input_num = 3;
    MS_CHECK_VALUE(input_args.size() == input_num, CheckAndConvertUtils::FormatCheckIntegerMsg(
                                                     "input num", input_args.size(), kEqual, input_num, primitive));
    const auto &op_name = GetValue<std::string>(input_args[kInputIndex0]->GetValue());
    const auto &arg_name = GetValue<std::string>(input_args[kInputIndex1]->GetValue());
    MS_EXCEPTION_IF_NULL(input_args[kInputIndex2]);
    const auto &input_value = input_args[kInputIndex2]->GetValue();
    MS_EXCEPTION_IF_NULL(input_value);
    if (!input_value->isa<StringImm>()) {
      MS_EXCEPTION(TypeError) << "For '" << op_name << "', the value of '" << arg_name
                              << "' should be a string, but got " << input_value->ToString();
    }
    const auto &enum_str = GetValue<std::string>(input_value);
    const auto enum_int = StringToEnumImpl(op_name, arg_name, enum_str);
    return MakeValue(enum_int);
  }
};

REGISTER_PRIMITIVE_OP_INFER_IMPL(StringToEnum, prim::kPrimStringToEnum, StringToEnumInfer, true);
}  // namespace ops
}  // namespace mindspore
