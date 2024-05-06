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

#include "ops/ops_func_impl/floor_div.h"
#include "utils/check_convert_utils.h"
#include "ops/op_utils.h"

namespace mindspore {
namespace ops {
BaseShapePtr FloorDivFuncImpl::InferShape(const PrimitivePtr &primitive,
                                          const std::vector<AbstractBasePtr> &input_args) const {
  auto prim_name = primitive->name();
  auto input0_shape = input_args[kIndex0]->GetShape();
  auto input1_shape = input_args[kIndex1]->GetShape();

  const int64_t max_dim = 8;
  MS_CHECK_VALUE(input0_shape->GetShapeVector().size() < max_dim,
                 CheckAndConvertUtils::FormatCheckIntegerMsg("The dimension of FloorDiv input",
                                                             SizeToLong(input0_shape->GetShapeVector().size()),
                                                             kLessThan, max_dim, primitive));
  MS_CHECK_VALUE(input1_shape->GetShapeVector().size() < max_dim,
                 CheckAndConvertUtils::FormatCheckIntegerMsg("The dimension of FloorDiv input",
                                                             SizeToLong(input1_shape->GetShapeVector().size()),
                                                             kLessThan, max_dim, primitive));
  return BroadCastInferShape(prim_name, input_args);
}

TypePtr FloorDivFuncImpl::InferType(const PrimitivePtr &primitive,
                                    const std::vector<AbstractBasePtr> &input_args) const {
  return input_args[0]->GetType()->Clone();
}
}  // namespace ops
}  // namespace mindspore
