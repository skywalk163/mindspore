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
#include "mindspore/core/ops/symbol_ops_impl/scalar_min.h"

namespace mindspore {
namespace symshape {
namespace ops {
SymbolPtr ScalarMin::Eval() {
  // only eval on Building
  auto lhs = input_as_sptr<IntSymbol>(0);
  auto rhs = input_as_sptr<IntSymbol>(1);
  if (lhs->HasData() && rhs->HasData()) {
    return GenInt(std::min(lhs->value(), rhs->value()));
  }
  if (*lhs <= *rhs) {
    DoNotEvalOnRun();
    return lhs;
  }
  if (*rhs < *lhs) {
    DoNotEvalOnRun();
    return rhs;
  }
  return GenVInt();
}
void ScalarMin::UpdateMathInfo() {
  if (!need_eval()) {
    return;
  }
  auto lhs = input_as<IntSymbol>(0);
  auto rhs = input_as<IntSymbol>(1);
  auto out = output_as<IntSymbol>();
  out->SetRange(std::min(lhs->range_min(), rhs->range_min()), std::min(lhs->range_max(), rhs->range_max()));
  auto d1 = lhs->divisor();
  auto r1 = lhs->remainder();
  auto d2 = rhs->divisor();
  auto r2 = rhs->remainder();
  out->SetDivisorRemainder(std::gcd(std::gcd(std::gcd(d1, d2), r1), r2), 0);
}
}  // namespace ops
}  // namespace symshape
}  // namespace mindspore
