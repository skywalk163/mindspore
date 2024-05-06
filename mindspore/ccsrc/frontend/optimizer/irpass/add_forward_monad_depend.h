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

#ifndef MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_IRPASS_ADD_FORWARD_MONAD_DEPEND_H_
#define MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_IRPASS_ADD_FORWARD_MONAD_DEPEND_H_

#include <vector>
#include "frontend/optimizer/optimizer.h"
#include "frontend/optimizer/ad/grad.h"
#include "ir/func_graph.h"

namespace mindspore {
namespace opt {
namespace irpass {
namespace internal {
constexpr char kFlagAddedForwardU[] = "added_forward_u";

AnfNodePtr GetBpropGetter(const FuncGraphManagerPtr &manager, const CNodePtr &node) {
  const auto &user_nodes = manager->node_users()[node];
  for (const auto &iter : user_nodes) {
    if (IsPrimitiveCNode(iter.first, prim::kPrimTupleGetItem)) {
      auto idx = GetValueNode<Int64ImmPtr>(iter.first->cast<CNodePtr>()->input(kInputNodeOutputIndexInTupleGetItem));
      if (idx != nullptr && idx->value() == 1) {
        return iter.first;
      }
    }
  }
  return nullptr;
}

AnfNodePtr GetBpropUser(const FuncGraphManagerPtr &manager, const AnfNodePtr &bprop_getter) {
  MS_EXCEPTION_IF_NULL(manager);
  const auto &node_users = manager->node_users();
  auto iter = node_users.find(bprop_getter);
  if (iter == node_users.end()) {
    return nullptr;
  }
  if (iter->second.size() != 1) {
    MS_LOG(EXCEPTION) << "The number of bprop caller should be 1, but got " << iter->second.size()
                      << ", bprop_getter: " << bprop_getter->DebugString();
  }
  auto user_node_idx = iter->second.begin();
  if (user_node_idx->second != 0) {
    MS_LOG(EXCEPTION) << "The bprop_getter should be used in input 0, but got " << user_node_idx->second;
  }
  return user_node_idx->first;
}

void AddDependNodes(const FuncGraphManagerPtr &manager, const FuncGraphPtr &bprop_graph, const AbstractBasePtr &u_abs) {
  auto new_u_para = bprop_graph->add_parameter();
  new_u_para->debug_info()->set_name("forward_u");
  new_u_para->set_abstract(u_abs);
  bprop_graph->set_flag(kFlagAddedForwardU, true);
  std::vector<CNodePtr> side_effect_bprop_apps;
  std::vector<CNodePtr> side_effect_bprop_app_propagate_nodes;
  for (const auto &node : bprop_graph->nodes()) {
    auto cnode = dyn_cast<CNode>(node);
    if (cnode == nullptr) {
      continue;
    }
    if (cnode->HasAttr(kAttrSideEffectBpropAppPropagate)) {
      (void)side_effect_bprop_app_propagate_nodes.emplace_back(cnode);
    }
    if (cnode->HasAttr(kAttrSideEffectBpropApp)) {
      (void)side_effect_bprop_apps.emplace_back(cnode);
    }
  }
  if (side_effect_bprop_apps.empty() && side_effect_bprop_app_propagate_nodes.empty()) {
    return;
  }
  for (const auto &bprop_app : side_effect_bprop_apps) {
    std::vector<AnfNodePtr> depend_inputs{NewValueNode(prim::kPrimDepend), bprop_app->input(1), new_u_para};
    manager->SetEdge(bprop_app, 1, bprop_graph->NewCNode(depend_inputs));
  }
  for (const auto &propagate_node : side_effect_bprop_app_propagate_nodes) {
    manager->AddEdge(propagate_node, new_u_para);
    auto bprop_getter_abs = dyn_cast<abstract::FuncGraphAbstractClosure>(propagate_node->input(0)->abstract());
    if (bprop_getter_abs == nullptr) {
      MS_LOG(INTERNAL_EXCEPTION) << "The node " << propagate_node->input(0)->DebugString()
                                 << " should have a FuncGraphAbstractClosure abstract.";
    }
    auto bprop_fg = bprop_getter_abs->func_graph();
    MS_EXCEPTION_IF_NULL(bprop_fg);
    if (bprop_fg->has_flag(kFlagAddedForwardU)) {
      continue;
    }
    AddDependNodes(manager, bprop_fg, u_abs);
  }
}
}  // namespace internal

// The origin pattern:
// %0 = U
// %1 = call fprop(x, y, %0)
// %2 = get_item(%1, 1)
// %3 = %2(dout)
//
// After the pass:
// kLevelNone(no changes)
// %0 = U
// %1 = call fprop(x, y, %0)
// %2 = get_item(%1, 1)
// %3 = %2(dout)
//
// kLevelTop
// %0 = U
// %1 = call fprop(x, y, %0)
// %2 = get_item(%1, 1)
// %3 = %2(dout, %0)
//
// kLevelWhole
// %0 = U
// %1 = call fprop(x, y, %0)
// %2 = UpdateState(U, %1)
// %3 = get_item(%1, 1)
// %4 = %2(dout, %2)
bool AddForwardMonadDepend(const FuncGraphPtr &root, const opt::OptimizerPtr &opt) {
  MS_EXCEPTION_IF_NULL(root);
  MS_EXCEPTION_IF_NULL(opt);
  auto manager = opt->manager();
  MS_EXCEPTION_IF_NULL(manager);
  std::vector<FuncGraphPtr> top_k_graphs;
  for (const auto &fg : root->func_graphs_used_total()) {
    MS_EXCEPTION_IF_NULL(fg);
    if (fg->has_attr(kAttrBpropAutoMonadLevel) && fg->has_flag(kAttrSideEffectBpropAppPropagate)) {
      (void)top_k_graphs.emplace_back(fg);
    }
  }

  bool changed = false;
  for (const auto &top_k_graph : top_k_graphs) {
    auto bprop_auto_monad_level = GetValue<int>(top_k_graph->get_attr(kAttrBpropAutoMonadLevel));
    top_k_graph->erase_flag(kAttrBpropAutoMonadLevel);
    if (bprop_auto_monad_level == ad::BpropAutoMonadLevel::kLevelNone) {
      break;
    }
    FuncGraphPtr bprop_graph = nullptr;
    AbstractBasePtr u_abs = nullptr;
    for (const auto &entry : top_k_graph->func_graph_cnodes_index()) {
      auto k_graph_caller = entry.first->first->cast<CNodePtr>();
      auto index = entry.first->second;
      // Get the real graph caller.
      if (index != 0) {
        continue;
      }
      // Get the monad input.
      auto umonad_input = k_graph_caller->input(k_graph_caller->size() - 1);
      if (!HasAbstractUMonad(umonad_input)) {
        continue;
      }
      // Only handle the fprop which has bprop getter.
      auto bprop_getter = internal::GetBpropGetter(manager, k_graph_caller);
      if (bprop_getter == nullptr) {
        continue;
      }
      auto bprop_getter_abs = dyn_cast<abstract::FuncGraphAbstractClosure>(bprop_getter->abstract());
      if (bprop_getter_abs == nullptr) {
        MS_LOG(INTERNAL_EXCEPTION) << "The node " << bprop_getter->DebugString()
                                   << " should have a FuncGraphAbstractClosure abstract.";
      }
      if (bprop_graph == nullptr) {
        bprop_graph = bprop_getter_abs->func_graph();
      } else if (bprop_getter_abs->func_graph() != bprop_graph) {
        MS_LOG(INTERNAL_EXCEPTION) << "The bprop graphs are not same for the k graph: " << top_k_graph->ToString();
      }
      auto bprop_user = internal::GetBpropUser(manager, bprop_getter);
      if (bprop_user == nullptr) {
        continue;
      }

      auto update_state_to_depend = umonad_input;
      if (bprop_auto_monad_level == ad::BpropAutoMonadLevel::kLevelWhole) {
        std::vector<AnfNodePtr> new_update_state_inputs = {NewValueNode(prim::kPrimUpdateState), umonad_input,
                                                           k_graph_caller};
        update_state_to_depend = k_graph_caller->func_graph()->NewCNodeInOrder(new_update_state_inputs);
        update_state_to_depend->set_abstract(umonad_input->abstract());
      }
      manager->AddEdge(bprop_user, update_state_to_depend);
      changed = true;
      u_abs = umonad_input->abstract();
    }
    internal::AddDependNodes(manager, bprop_graph, u_abs);
  }
  return changed;
}
}  // namespace irpass
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_IRPASS_ADD_FORWARD_MONAD_DEPEND_H_
