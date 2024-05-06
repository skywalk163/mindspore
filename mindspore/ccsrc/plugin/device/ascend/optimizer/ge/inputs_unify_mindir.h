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

#ifndef MINDSPORE_CCSRC_BACKEND_OPTIMIZER_ASCEND_GE_OPTIMIZER_IRPASS_INPUTS_UNIFY_MINDIR_H_
#define MINDSPORE_CCSRC_BACKEND_OPTIMIZER_ASCEND_GE_OPTIMIZER_IRPASS_INPUTS_UNIFY_MINDIR_H_

#include <string>
#include "include/backend/optimizer/optimizer.h"

namespace mindspore {
namespace opt {
class InputsUnifyMindIR : public PatternProcessPass {
 public:
  explicit InputsUnifyMindIR(bool multigraph = true) : PatternProcessPass("inputs_unify_mindir", multigraph) {
    is_add_ = false;
  }
  ~InputsUnifyMindIR() override = default;

  const AnfNodePtr Process(const FuncGraphPtr &, const AnfNodePtr &, const EquivPtr &) const override;

 private:
  ValueNodePtr CreateValueTensor(const FuncGraphPtr &func_graph, const AnfNodePtr &node) const;
  CNodePtr CreateTupleToTensor(const FuncGraphPtr &func_graph, const AnfNodePtr &node) const;
  CNodePtr CreateScalarToTensor(const FuncGraphPtr &func_graph, const AnfNodePtr &node) const;
  AnfNodePtr CreateCastNode(const FuncGraphPtr &func_graph, const AnfNodePtr &node, const TypePtr &data_type) const;
};
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_OPTIMIZER_ASCEND_GE_OPTIMIZER_IRPASS_MAKETUPLE_UNIFY_MINDIR_H_
