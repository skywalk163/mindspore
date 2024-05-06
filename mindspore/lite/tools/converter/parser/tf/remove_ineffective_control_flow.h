/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_TF_REMOVE_INEFFECTIVE_CONTROL_FLOW_H
#define MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_TF_REMOVE_INEFFECTIVE_CONTROL_FLOW_H

#include <map>
#include "ir/func_graph.h"
#include "proto/graph.pb.h"
#include "proto/node_def.pb.h"

namespace mindspore {
namespace lite {
class RemoveIneffectiveControlFlow {
 public:
  RemoveIneffectiveControlFlow() = default;
  ~RemoveIneffectiveControlFlow() = default;
  bool Run(const FuncGraphPtr &func_graph, std::map<AnfNodePtr, int> *ineffective_if_op_map);

 private:
  bool CheckIfIneffective(const CNodePtr &merge);
  bool CheckIfCondIsConstTensor(std::map<AnfNodePtr, int> *ineffective_if_op_map, const CNodePtr &anf_node);
  AnfNodePtr shared_input_{nullptr};
};
}  // namespace lite
}  // namespace mindspore

#endif  // MINDSPORE_LITE_TOOLS_CONVERTER_PARSER_TF_REMOVE_INEFFECTIVE_CONTROL_FLOW_H
