/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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
#include "mindspore/core/symbolic_shape/operation_builder.h"

namespace mindspore {
namespace symshape {
namespace ops {
REG_SYMBOL_OP_BUILDER("RmsNorm").SetShapeFunc([](OperationBuilder *b) -> SymbolPtr {
  auto inp = b->GetInputShape(kIndex0)->as_sptr<ListSymbol>();
  MS_EXCEPTION_IF_NULL(inp);
  if (inp->is_dyn_len()) {
    return nullptr;
  }
  auto rstd_shape = inp->symbols();
  rstd_shape.back() = IntSymbol::Make(1LL);
  return ListSymbol::Make(SymbolPtrList{inp, ListSymbol::Make(std::move(rstd_shape))});
});
}  // namespace ops
}  // namespace symshape
}  // namespace mindspore
