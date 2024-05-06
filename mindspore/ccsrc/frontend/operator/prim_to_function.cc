/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#include "frontend/operator/prim_to_function.h"

#include "mindspore/core/ops/arithmetic_ops.h"
#include "mindspore/core/ops/comparison_ops.h"
#include "mindspore/core/ops/structure_ops.h"
namespace mindspore {
// namespace to support prim related definition
namespace prim {

PrimToFunction::PrimToFunction()
    : prim_func_type_map_({{"bool_not", kPrimTypeNumOneArg},
                           {"scalar_cos", kPrimTypeNumOneArg},
                           {"scalar_exp", kPrimTypeNumOneArg},
                           {kScalarFloorOpName, kPrimTypeNumOneArg},
                           {"ScalarLog", kPrimTypeNumOneArg},
                           {"scalar_sin", kPrimTypeNumOneArg},
                           {"scalar_tan", kPrimTypeNumOneArg},
                           {kScalarTruncOpName, kPrimTypeNumOneArg},
                           {"typeof", kPrimTypeNumOneArg},
                           {"ScalarUadd", kPrimTypeNumOneArg},
                           {"ScalarUsub", kPrimTypeNumOneArg},
                           {"ScalarAdd", kPrimTypeNumTwoArgs},
                           {"bool_and", kPrimTypeNumTwoArgs},
                           {"bool_eq", kPrimTypeNumTwoArgs},
                           {"bool_or", kPrimTypeNumTwoArgs},
                           {"ScalarDiv", kPrimTypeNumTwoArgs},
                           {"ScalarEq", kPrimTypeNumTwoArgs},
                           {"ScalarGe", kPrimTypeNumTwoArgs},
                           {"ScalarGt", kPrimTypeNumTwoArgs},
                           {"ScalarLe", kPrimTypeNumTwoArgs},
                           {"ScalarLt", kPrimTypeNumTwoArgs},
                           {"scalar_ne", kPrimTypeNumTwoArgs},
                           {"ScalarMod", kPrimTypeNumTwoArgs},
                           {"ScalarMul", kPrimTypeNumTwoArgs},
                           {"ScalarPow", kPrimTypeNumTwoArgs},
                           {"ScalarSub", kPrimTypeNumTwoArgs},
                           {"ScalarFloorDiv", kPrimTypeNumTwoArgs},
                           {kScalarBitwiseAndOpName, kPrimTypeNumTwoArgs},
                           {kScalarBitwiseOrOpName, kPrimTypeNumTwoArgs},
                           {"bit_xor", kPrimTypeNumTwoArgs},
                           {"bit_left_shift", kPrimTypeNumTwoArgs},
                           {"bit_right_shift", kPrimTypeNumTwoArgs},
                           {kStringNotOpName, kPrimTypeStrOneArg},
                           {kStringConcatOpName, kPrimTypeStrTwoArgs},
                           {kStringInOpName, kPrimTypeStrTwoArgs},
                           {kStringEqOpName, kPrimTypeStrTwoArgs},
                           {kStringLtOpName, kPrimTypeStrTwoArgs},
                           {kStringGtOpName, kPrimTypeStrTwoArgs},
                           {kStringLeOpName, kPrimTypeStrTwoArgs},
                           {kStringGeOpName, kPrimTypeStrTwoArgs}}) {}

bool PrimToFunction::GetFunction(const PrimitivePtr &prim, FunctionPtr *func) const {
  if (func != nullptr) {
    int64_t args_num = GetPrimType(prim);
    switch (args_num) {
      case kPrimTypeNumOneArg: {
        std::vector<TypePtr> num_one_arg{std::make_shared<Number>()};
        *func = Function(num_one_arg, std::make_shared<Number>()).DeepCopy()->cast<FunctionPtr>();
        return true;
      }
      case kPrimTypeNumTwoArgs: {
        std::vector<TypePtr> num_two_args{std::make_shared<Number>(), std::make_shared<Number>()};
        *func = Function(num_two_args, std::make_shared<Number>()).DeepCopy()->cast<FunctionPtr>();
        return true;
      }
      case kPrimTypeStrOneArg: {
        std::vector<TypePtr> str_one_arg{std::make_shared<String>()};
        *func = Function(str_one_arg, std::make_shared<String>()).DeepCopy()->cast<FunctionPtr>();
        return true;
      }
      case kPrimTypeStrTwoArgs: {
        std::vector<TypePtr> str_two_args{std::make_shared<String>(), std::make_shared<String>()};
        *func = Function(str_two_args, std::make_shared<String>()).DeepCopy()->cast<FunctionPtr>();
        return true;
      }
      default:
        return false;
    }
  }
  return false;
}

int64_t PrimToFunction::GetPrimType(const PrimitivePtr &prim) const {
  MS_EXCEPTION_IF_NULL(prim);
  int64_t prim_type = static_cast<int64_t>(kPrimTypeUnknown);

  auto value = prim_func_type_map_.find(prim->name());
  if (value != prim_func_type_map_.end()) {
    prim_type = value->second;
  }
  return prim_type;
}
}  // namespace prim
}  // namespace mindspore
