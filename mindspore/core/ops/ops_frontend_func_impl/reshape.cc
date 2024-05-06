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

#include "ops/ops_frontend_func_impl.h"
#include "utils/check_convert_utils.h"
#include "ops/op_utils.h"

namespace mindspore::ops {
class ReshapeFrontendFuncImpl : public OpFrontendFuncImpl {
 public:
  // Do not override this interface if the op has no InferValue
  ValuePtr InferValue(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override {
    if (input_args.size() != 2) {
      return nullptr;
    }
    if (!IsValueKnown(input_args[1]->GetValue())) {
      return nullptr;
    }
    return InferValueCallback::GetInstance().CallPyInferValue("Reshape", input_args);
  }
};

REGISTER_PRIMITIVE_FUNCTION_FRONTEND_FUNC_IMPL("Reshape", ReshapeFrontendFuncImpl);
}  // namespace mindspore::ops
