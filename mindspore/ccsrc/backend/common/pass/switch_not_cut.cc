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
#include "backend/common/pass/switch_not_cut.h"

#include <memory>
#include <vector>
#include <utility>
#include "ops/other_ops.h"
#include "ops/framework_ops.h"
#include "utils/ms_context.h"
#include "include/common/utils/anfalgo.h"

namespace mindspore {
namespace opt {
namespace {
bool IsValidFuncGraph(const FuncGraphPtr &func_graph, std::set<FuncGraphPtr> *checked_graphs,
                      std::set<CNodePtr> *inline_call_nodes);
bool IsValidInlinePartial(const AnfNodePtr &node, std::set<FuncGraphPtr> *checked_graphs) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(checked_graphs);
  if (!common::AnfAlgo::CheckPrimitiveType(node, prim::kPrimPartial)) {
    MS_LOG(DEBUG) << "Invalid partial node:" << node->DebugString();
    return false;
  }
  const auto &cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  if (cnode->size() <= kPartialGraphIndex) {
    MS_LOG(DEBUG) << "Invalid partial node:" << node->DebugString();
    return false;
  }
  auto sub_graph = common::AnfAlgo::GetValueNodeFuncGraph(cnode->input(kIndex1));
  if (sub_graph == nullptr || sub_graph->return_node() == nullptr || sub_graph->return_node()->size() <= 1) {
    MS_LOG(DEBUG) << "Invalid partial node:" << node->DebugString();
    return false;
  }
  // Output valuenode check should be in partial check, as the root graph could.
  const auto &outputs = common::AnfAlgo::GetAllOutputWithIndex(sub_graph->return_node()->input(1));
  if (std::any_of(outputs.begin(), outputs.end(), [](const std::pair<AnfNodePtr, int64_t> &pair) {
        return pair.first != nullptr && pair.first->isa<ValueNode>();
      })) {
    MS_LOG(DEBUG) << "Partial graph:" << sub_graph->ToString()
                  << " has value node output for node:" << node->DebugString();
    return false;
  }
  if (!IsValidFuncGraph(sub_graph, checked_graphs, nullptr)) {
    MS_LOG(DEBUG) << "Partial graph:" << sub_graph->ToString() << " is not valid for node:" << node->DebugString();
    return false;
  }
  return true;
}

bool IsValidInlineSwitch(const AnfNodePtr &node, std::set<FuncGraphPtr> *checked_graphs) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(checked_graphs);
  if (!common::AnfAlgo::CheckPrimitiveType(node, prim::kPrimSwitch)) {
    MS_LOG(DEBUG) << "Invalid switch node:" << node->DebugString();
    return false;
  }
  const auto &cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  if (cnode->size() != kSwitchInputSize) {
    MS_LOG(DEBUG) << "Invalid switch node" << cnode->DebugString();
    return false;
  }
  if ((!IsValidInlinePartial(cnode->input(kSwitchTrueBranchIndex), checked_graphs)) ||
      (!IsValidInlinePartial(cnode->input(kSwitchFalseBranchIndex), checked_graphs))) {
    MS_LOG(DEBUG) << "Invalid partial input for switch node:" << node->DebugString();
    return false;
  }
  return true;
}

bool IsValidAbstract(const abstract::AbstractBasePtr &abstract) {
  if (abstract == nullptr) {
    return true;
  }
  if (abstract->isa<abstract::AbstractFunction>() || abstract->isa<abstract::AbstractAny>()) {
    MS_LOG(DEBUG) << "Invalid abstract:" << abstract->ToString();
    return false;
  }

  if (!abstract->isa<abstract::AbstractSequence>()) {
    return true;
  }

  const auto &sequence_abs = abstract->cast<abstract::AbstractSequencePtr>();
  MS_EXCEPTION_IF_NULL(sequence_abs);
  if (sequence_abs->dynamic_len()) {
    MS_LOG(DEBUG) << "Invalid abstract:" << abstract->ToString();
    return false;
  }

  if (std::any_of(sequence_abs->elements().begin(), sequence_abs->elements().end(),
                  [](const abstract::AbstractBasePtr &sub_abstract) { return !IsValidAbstract(sub_abstract); })) {
    return false;
  }
  return true;
}

bool IsValidFuncGraph(const FuncGraphPtr &func_graph, std::set<FuncGraphPtr> *checked_graphs,
                      std::set<CNodePtr> *inline_call_nodes) {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(checked_graphs);
  if (checked_graphs->find(func_graph) != checked_graphs->end()) {
    MS_LOG(EXCEPTION) << "Circle call exist in funcgraph:" << func_graph->ToString();
  }
  MS_LOG(INFO) << "Check funcgraph:" << func_graph->ToString() << " in control flow inline.";
  checked_graphs->emplace(func_graph);

  // Check input.
  if (std::any_of(func_graph->parameters().begin(), func_graph->parameters().end(), [](const AnfNodePtr &parameter) {
        return parameter->abstract() != nullptr && (!IsValidAbstract(parameter->abstract()));
      })) {
    MS_LOG(DEBUG) << "Invalid input node for funcgraph:" << func_graph->ToString();
    return false;
  }
  // Check output.
  MS_LOG(INFO) << "Enable Switch Inline";
  AnfNodePtr return_node = func_graph->get_return();
  MS_EXCEPTION_IF_NULL(return_node);
  std::vector<AnfNodePtr> all_nodes = TopoSort(return_node);
  std::string last_target;
  for (auto &node : all_nodes) {
    if (!node->isa<CNode>()) {
      continue;
    }
    MS_LOG(DEBUG) << "Check cnode:" << node->DebugString();
    const auto &cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    std::string current_target = GetCNodeTarget(cnode);
    if (last_target != "" && current_target != last_target) {
      MS_LOG(DEBUG) << "Heter in node:" << cnode->DebugString();
      return false;
    }
    last_target = current_target;
    if (cnode->inputs().empty()) {
      continue;
    }
    if (IsPrimitiveCNode(cnode, prim::kPrimSwitchLayer)) {
      MS_LOG(DEBUG) << "Switch layer not suppoer inline.";
      return false;
    }
    if (!common::AnfAlgo::IsCallNode(cnode)) {
      continue;
    }
    if (common::AnfAlgo::HasIncorporateCallNode(cnode)) {
      continue;
    }
    auto primitive_input = cnode->input(kAnfPrimitiveIndex);
    if (!IsPrimitiveCNode(primitive_input, prim::kPrimSwitch) ||
        (!IsValidInlineSwitch(primitive_input, checked_graphs)) || (!IsValidAbstract(cnode->abstract()))) {
      MS_LOG(DEBUG) << "Invalid switch node:" << node->DebugString();
      return false;
    }
    if (inline_call_nodes != nullptr) {
      MS_LOG(DEBUG) << "Inline for node:" << node->DebugString();
      inline_call_nodes->emplace(cnode);
    }
  }
  return true;
}
}  // namespace
bool SwitchNotCut::Run(const FuncGraphPtr &func_graph) {
  std::set<FuncGraphPtr> checked_graphs;
  std::set<CNodePtr> inline_call_nodes;
  if (IsValidFuncGraph(func_graph, &checked_graphs, &inline_call_nodes)) {
    for (const auto &cnode : inline_call_nodes) {
      cnode->AddPrimalAttr(kAttrNotCut, MakeValue(true));
      const auto &switch_node = cnode->input(0)->cast<CNodePtr>();
      switch_node->AddPrimalAttr(kAttrNotCut, MakeValue(true));
      const auto &true_partial_node = switch_node->input(kSwitchTrueBranchIndex)->cast<CNodePtr>();
      true_partial_node->AddPrimalAttr(kAttrNotCut, MakeValue(true));
      auto true_partial_graph = true_partial_node->input(kIndex1);
      auto true_sub_graph = common::AnfAlgo::GetValueNodeFuncGraph(true_partial_graph);
      MS_EXCEPTION_IF_NULL(true_sub_graph);
      true_sub_graph->set_flag(kFlagSwitchInline, true);
      const auto &false_partial_node = switch_node->input(kSwitchFalseBranchIndex)->cast<CNodePtr>();
      false_partial_node->AddPrimalAttr(kAttrNotCut, MakeValue(true));
      auto false_partial_graph = false_partial_node->input(kIndex1);
      auto false_sub_graph = common::AnfAlgo::GetValueNodeFuncGraph(false_partial_graph);
      MS_EXCEPTION_IF_NULL(false_sub_graph);
      false_sub_graph->set_flag(kFlagSwitchInline, true);
    }
  }
  return false;
}
}  // namespace opt
}  // namespace mindspore
