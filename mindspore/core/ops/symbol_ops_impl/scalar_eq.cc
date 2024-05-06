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
#include "mindspore/core/ops/symbol_ops_impl/scalar_eq.h"

namespace mindspore {
namespace symshape {
namespace ops {
SymbolPtr ScalarEq::Eval() {
  // only eval on Building
  auto lhs = input_as<IntSymbol>(0);
  auto rhs = input_as<IntSymbol>(1);
  if (lhs->HasData() && rhs->HasData()) {
    return BoolSymbol::Make(lhs->value() == rhs->value());
  }
  return (*lhs == *rhs) ? BoolSymbol::Make(true) : BoolSymbol::Make(shared_from_this());
}

REG_SYMBOL_OP_BUILDER("ScalarEq").SetValueFunc(DefaultBuilder<ScalarEq, 2>);
REG_SYMBOL_OP_BUILDER("scalar_eq").SetValueFunc(DefaultBuilder<ScalarEq, 2>);
}  // namespace ops
}  // namespace symshape
}  // namespace mindspore
