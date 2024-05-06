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

#include <string>
#include "abstract/abstract_value.h"
#include "utils/check_convert_utils.h"
#include "utils/log_adapter.h"
#include "ops/op_utils.h"
#include "ops/op_name.h"
#include "ops/ops_func_impl/prelu.h"
#include "utils/ms_context.h"

namespace mindspore::ops {
bool IsAscend() {
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  return context->get_param<std::string>(MS_CTX_DEVICE_TARGET) == kAscendDevice;
}

BaseShapePtr PReLUFuncImpl::InferShape(const PrimitivePtr &primitive,
                                       const std::vector<AbstractBasePtr> &input_args) const {
  auto x_shape_ptr = input_args[kInputIndex0]->GetShape();
  MS_EXCEPTION_IF_NULL(x_shape_ptr);
  return x_shape_ptr->Clone();
}

TypePtr PReLUFuncImpl::InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const {
  auto prim_name = primitive->name();
  auto x_type = input_args[kInputIndex0]->GetType();
  MS_EXCEPTION_IF_NULL(x_type);
  auto weight_type = input_args[kInputIndex1]->GetType();
  MS_EXCEPTION_IF_NULL(weight_type);
  auto valid_types = {kFloat16, kFloat32};
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x", x_type, valid_types, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("weight", weight_type, valid_types, prim_name);
  return x_type->Clone();
}

int32_t PReLUFuncImpl::CheckValidation(const PrimitivePtr &primitive,
                                       const std::vector<AbstractBasePtr> &input_args) const {
  auto prim_name = primitive->name();
  auto x_shape_ptr = input_args[kInputIndex0]->GetShape();
  auto weight_shape_ptr = input_args[kInputIndex1]->GetShape();
  MS_EXCEPTION_IF_NULL(x_shape_ptr);
  MS_EXCEPTION_IF_NULL(weight_shape_ptr);
  auto x_shape = x_shape_ptr->GetShapeVector();
  auto weight_shape = weight_shape_ptr->GetShapeVector();

  if (IsDynamic(x_shape) || IsDynamic(weight_shape)) {
    return OP_CHECK_RETRY;
  }

  auto weight_rank = weight_shape.size();
  MS_CHECK_VALUE(weight_rank == 1, "The dimension of 'weight' must be 1");

  auto x_rank = x_shape.size();

  if (IsAscend() && x_rank <= 1) {
    MS_EXCEPTION(ValueError)
      << "For '" << prim_name
      << "', the dimension of 'x' can not be 0-D or 1-D when the platform is \"Ascend\", but got dimension of 'x' is "
      << x_rank << ".";
  }

  auto channel_num = x_rank <= 1 ? 1 : x_shape[1];
  auto weight_len = weight_shape[0];
  if (weight_len != 1 && weight_len != channel_num) {
    MS_EXCEPTION(ValueError) << "For '" << prim_name
                             << "', the length of 'weight' must be equal to number of channels: " << channel_num
                             << ", but got " << weight_shape << ".";
  }

  return OP_CHECK_SUCCESS;
}
}  // namespace mindspore::ops
