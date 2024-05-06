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
#ifndef MINDSPORE_CCSRC_BACKEND_COMMON_PASS_ADD_ATTR_TO_NODE_ADD_ATTR_TO_NODE_H_
#define MINDSPORE_CCSRC_BACKEND_COMMON_PASS_ADD_ATTR_TO_NODE_ADD_ATTR_TO_NODE_H_

#include "include/backend/optimizer/optimizer.h"

namespace mindspore {
namespace opt {
class AddAttrToNode : public PatternProcessPass {
 public:
  explicit AddAttrToNode(bool multigraph = true) : PatternProcessPass("add_attr_to_node", multigraph) {}
  ~AddAttrToNode() override = default;

  const AnfNodePtr Process(const FuncGraphPtr &graph, const AnfNodePtr &node, const EquivPtr &) const override;
};
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_COMMON_PASS_ADD_ATTR_TO_NODE_ADD_ATTR_TO_NODE_H_
