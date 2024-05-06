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
#include "mindspore/core/symbolic_shape/operation_builder.h"
#include "mindspore/core/ops/symbol_ops_impl/make_tuple.h"

namespace mindspore {
namespace symshape {
namespace ops {
// only support IntList value for shape calculation
SymbolPtr StackValue(OperationBuilder *b) {
  SymbolPtrList result;
  if (b->input_num() == 1) {
    return b->GetInputValue(kIndex0);
  }
  // inputs of Stack is spread.
  // todo, remove this branch, then the Stack can be replace to 'Transparent' builder.
  return MakeTupleBuilder(b);
}

REG_SYMBOL_OP_BUILDER("Stack").SetValueFunc(StackValue);
}  // namespace ops
}  // namespace symshape
}  // namespace mindspore
