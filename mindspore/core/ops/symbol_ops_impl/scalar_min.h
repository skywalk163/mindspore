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
#ifndef MINDSPORE_CORE_OPS_SYMBOL_OPS_IMPL_SCALAR_MIN_H_
#define MINDSPORE_CORE_OPS_SYMBOL_OPS_IMPL_SCALAR_MIN_H_

#include <algorithm>
#include "mindspore/core/ops/symbol_ops_impl/common.h"

namespace mindspore {
namespace symshape {
namespace ops {
class MS_CORE_API ScalarMin : public ScalarOp {
 public:
  using ScalarOp::ScalarOp;
  ScalarMin(const SymbolPtr &lhs, const SymbolPtr &rhs) : ScalarOp({lhs, rhs}) { support_commutative_law_ = true; }
  MS_DECLARE_PARENT(ScalarMin, ScalarOp)

 protected:
  SymbolPtr Eval() override;
  void EvalOnRun() override { output_as<IntSymbol>()->SetValue(std::min(AsInt(input(0)), AsInt(input(1)))); }
  void UpdateMathInfo() override;
};
}  // namespace ops
}  // namespace symshape
}  // namespace mindspore
#endif  // MINDSPORE_CORE_OPS_SYMBOL_OPS_IMPL_SCALAR_MIN_H_
