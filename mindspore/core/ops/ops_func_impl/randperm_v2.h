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

#ifndef MINDSPORE_CORE_OPS_OPS_FUNC_IMPL_RANDPERM_V2_H_
#define MINDSPORE_CORE_OPS_OPS_FUNC_IMPL_RANDPERM_V2_H_

#include <vector>
#include <set>
#include "ops/op_name.h"
#include "ops/ops_func_impl/op_func_impl.h"

namespace mindspore::ops {
/// \brief Implementation of InferShape and InferType functions for operator 'RandpermV2'
class MIND_API RandpermV2FuncImpl : public OpFuncImpl {
 public:
  BaseShapePtr InferShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override;
  TypePtr InferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) const override;
  std::set<int64_t> GetValueDependArgIndices() const {
    return {kInputIndex0, kInputIndex1, kInputIndex2, kInputIndex3};
  }
};
}  // namespace mindspore::ops

#endif  // MINDSPORE_CORE_OPS_OPS_FUNC_IMPL_RANDPERM_V2_H_
