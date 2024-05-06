/**
 * Copyright 2020-2023 Huawei Technologies Co., Ltd
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
#include "backend/common/pass/convert_const_input_to_tensor_input.h"
#include <memory>
#include <set>
#include <vector>

#include "include/backend/anf_runtime_algorithm.h"
#include "include/backend/kernel_graph.h"
#include "include/backend/optimizer/helper.h"
#include "include/common/utils/anfalgo.h"
#include "ir/graph_utils.h"
#include "ops/array_op_name.h"
#include "ops/framework_ops.h"
#include "ops/sequence_ops.h"
#include "ops/op_def.h"

namespace mindspore {
namespace opt {
namespace {
AnfNodePtr ConstInputToTensorInput(const FuncGraphPtr &func_graph, const CNodePtr &cnode) {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(cnode);
  const std::set<std::string> no_need_to_convert_nodes = {kStackOpName};
  auto node_type = common::AnfAlgo::GetCNodeName(cnode);
  if (no_need_to_convert_nodes.find(node_type) != no_need_to_convert_nodes.end()) {
    return nullptr;
  }
  auto op_def = mindspore::ops::GetOpDef(node_type);
  if (op_def != nullptr) {
    return nullptr;
  }
  std::vector<AnfNodePtr> new_inputs;
  auto kernel_graph = func_graph->cast<std::shared_ptr<session::KernelGraph>>();
  auto inputs = cnode->inputs();
  new_inputs.push_back(inputs[0]);
  bool need_update = false;
  std::vector<int64_t> fake_tensor_pos;
  std::vector<int64_t> value_list_pos;
  // the first input is primitive node which is not the real input
  for (size_t i = 0; i < inputs.size() - 1; ++i) {
    auto input_node = inputs[i + 1];
    if (AnfAlgo::IsScalarConvertToTensor(input_node, cnode) || IsValueNode<ValueSequence>(input_node)) {
      auto tensor_input = CreateTensorInput(kernel_graph, input_node);
      if (tensor_input == nullptr) {
        new_inputs.push_back(input_node);
        continue;
      }
      new_inputs.push_back(tensor_input);
      fake_tensor_pos.push_back(i);
      need_update = true;
      if (IsValueNode<ValueList>(input_node)) {
        value_list_pos.push_back(i);
      }
    } else {
      new_inputs.push_back(input_node);
    }
  }
  if (need_update) {
    MS_EXCEPTION_IF_NULL(func_graph);
    auto new_cnode = NewCNode(new_inputs, func_graph);
    MS_EXCEPTION_IF_NULL(new_cnode);
    new_cnode->set_primal_attrs(cnode->primal_attrs());
    new_cnode->set_attrs(cnode->attrs());
    if (common::AnfAlgo::CheckPrimitiveType(cnode, prim::kPrimDepend)) {
      new_cnode->set_abstract(new_inputs[1]->abstract());
    } else {
      new_cnode->set_abstract(cnode->abstract());
    }
    new_cnode->set_scope(cnode->scope());
    common::AnfAlgo::CopyNodeAttrs(cnode, new_cnode);
    if (kernel_graph != nullptr) {
      kernel_graph->FrontBackendlMapUpdate(cnode, new_cnode);
    }
    if (common::AnfAlgo::CheckPrimitiveType(new_cnode, prim::kPrimPrint)) {
      if (fake_tensor_pos.size() > 0) {
        common::AnfAlgo::SetNodeAttr(kFakeTensorPos, MakeValue<std::vector<int64_t>>(fake_tensor_pos), new_cnode);
      }
      if (value_list_pos.size() > 0) {
        common::AnfAlgo::SetNodeAttr(kFakeTensorListPos, MakeValue<std::vector<int64_t>>(value_list_pos), new_cnode);
      }
    }
    return new_cnode;
  }
  return nullptr;
}
}  // namespace

const AnfNodePtr ConvertConstInputToTensorInput::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                                         const EquivPtr &) const {
  // The virtual node maybe the output node of graph and can't miss the attribute of value.
  if (node == nullptr || func_graph == nullptr || common::AnfAlgo::CheckPrimitiveType(node, prim::kPrimTupleGetItem) ||
      common::AnfAlgo::CheckPrimitiveType(node, prim::kPrimMakeTuple) ||
      common::AnfAlgo::CheckPrimitiveType(node, prim::kPrimDepend) ||
      common::AnfAlgo::CheckPrimitiveType(node, prim::kPrimPyExecute)) {
    return nullptr;
  }
  if (!node->isa<CNode>()) {
    return nullptr;
  }

  return ConstInputToTensorInput(func_graph, node->cast<CNodePtr>());
}

const AnfNodePtr ConvertConstInputToTensorInputForPrint::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                                                 const EquivPtr &) const {
  // convert const input to tensor input for print op.
  if (node == nullptr || func_graph == nullptr || !common::AnfAlgo::CheckPrimitiveType(node, prim::kPrimPrint)) {
    return nullptr;
  }
  if (!node->isa<CNode>()) {
    return nullptr;
  }

  return ConstInputToTensorInput(func_graph, node->cast<CNodePtr>());
}

}  // namespace opt
}  // namespace mindspore
