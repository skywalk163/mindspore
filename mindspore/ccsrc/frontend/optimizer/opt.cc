/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#include "frontend/optimizer/opt.h"

#include <deque>
#include <memory>
#include <algorithm>
#include <utility>

#include "mindspore/core/ops/structure_ops.h"
#include "utils/hash_map.h"
#include "ir/anf.h"
#include "ir/manager.h"
#include "frontend/optimizer/optimizer.h"
#include "utils/log_adapter.h"
#include "utils/compile_config.h"

namespace mindspore {
/* namespace to support opt */
namespace opt {
SubstitutionPtr MakeSubstitution(const OptimizerCallerPtr &transform, const std::string &name, const PrimitivePtr &prim,
                                 const RenormAction &renorm_action, bool has_priority_pattern) {
  auto fn = [prim](const AnfNodePtr &node) -> bool { return IsPrimitiveCNode(node, prim); };
  return std::make_shared<Substitution>(transform, name, fn, renorm_action, has_priority_pattern);
}

SubstitutionPtr MakeSubstitution(const OptimizerCallerPtr &transform, const std::string &name,
                                 const std::vector<PrimitivePtr> &prims, const RenormAction &renorm_action,
                                 bool has_priority_pattern) {
  auto fn = [prims](const AnfNodePtr &node) -> bool {
    auto cnode = dyn_cast_ptr<CNode>(node);
    if (cnode == nullptr) {
      return false;
    }
    auto cnode_prim = GetValuePtr<Primitive>(cnode->input(0));
    if (cnode_prim == nullptr) {
      return false;
    }
    auto hash = cnode_prim->Hash();
    const auto &name = cnode_prim->name();
    return std::any_of(prims.begin(), prims.end(), [&hash, &name](const PrimitivePtr &prim) {
      return (prim->Hash() == hash) && (prim->name() == name);
    });
  };
  return std::make_shared<Substitution>(transform, name, fn, renorm_action, has_priority_pattern);
}

SubstitutionPtr MakeSubstitution(const OptimizerCallerPtr &transform, const std::string &name,
                                 const PredicateFuncType &predicate, const RenormAction &renorm_action,
                                 bool has_priority_pattern) {
  return std::make_shared<Substitution>(transform, name, predicate, renorm_action, has_priority_pattern);
}

AnfNodePtr Substitution::operator()(const OptimizerPtr &optimizer, const AnfNodePtr &node) {
  AnfNodePtr result;
  if (optimizer != nullptr) {
    MsProfileStatGuard stat_subs_guard("substitution." + name_);
    MsProfileStatGuard stat_match_guard("match." + name_);
    result = (*transform_)(optimizer, node);
    if (result == nullptr) {
      stat_match_guard.Interrupt();
    }
  } else {
    result = (*transform_)(optimizer, node);
  }

  if (optimizer != nullptr && optimizer->is_watch_renormalize() && result != nullptr) {
    if ((renorm_action_ == FORCE_RENORM) || (result->abstract() == nullptr)) {
      optimizer->set_is_untyped_generated();
    }
  }

  return result;
}

static inline bool isTraversable(const AnfNodePtr &node) {
  if (node->isa<CNode>() || node->isa<Parameter>()) {
    return true;
  }
  // FuncGraph or RefKey value node is traversable.
  auto value_node = dyn_cast_ptr<ValueNode>(node);
  MS_EXCEPTION_IF_NULL(value_node);
  const auto &value = value_node->value();
  return (value != nullptr) && (value->isa<FuncGraph>() || value->isa<RefKey>() || value->isa<MindIRClassType>() ||
                                value->isa<MindIRMetaFuncGraph>() || value->isa<parse::ClassType>() ||
                                value->isa<prim::DoSignaturePrimitive>() || value->isa<ValueSequence>() ||
                                value->isa<parse::NameSpace>() || value->isa<ValueDictionary>());
}

static AnfNodePtr DoTransform(const OptimizerPtr &optimizer, const AnfNodePtr &node,
                              const SubstitutionPtr &substitution) {
  auto manager = optimizer->manager();
  MS_EXCEPTION_IF_NULL(manager);
  bool is_match = substitution->predicate_(node);
  if (is_match) {
    TraceGuard trace_guard(std::make_shared<TraceOpt>(node->debug_info()));
    ScopeGuard scope_guard(node->scope());
    auto res = (*substitution)(optimizer, node);
    if (res != nullptr && res != node) {
      MsProfileStatGuard stat_guard("replace." + substitution->name_);
      MS_LOG(DEBUG) << "Replace " << node->DebugString() << " with " << res->DebugString() << ", by "
                    << substitution->name_;
      (void)manager->Replace(node, res);
      return res;
    }
  }
  return nullptr;
}

static void UpdateTransformingListForSubstitutions(const AnfNodePtr &node, std::deque<AnfNodePtr> *todo, bool change) {
  auto fg = GetValuePtr<FuncGraph>(node);
  if (fg != nullptr) {
    (void)todo->emplace_back(fg->return_node());
  }

  if (change) {
    (void)todo->emplace_back(node);
  } else {
    auto cnode = dyn_cast_ptr<CNode>(node);
    if (cnode != nullptr) {
      const auto &inputs = cnode->inputs();
      (void)todo->insert(todo->end(), inputs.cbegin(), inputs.cend());
    }
  }
}

static void UpdateTransformingListForIR(const AnfNodePtr &node, std::deque<AnfNodePtr> *todo, bool change,
                                        const SubstitutionPtr &substitution) {
  auto fg = GetValuePtr<FuncGraph>(node);
  if (fg != nullptr) {
    (void)todo->emplace_back(fg->return_node());
  }

  // If there is a priority pattern in substitution, don't transform the new node,
  // otherwise some nodes may match the wrong patterns.
  if (change && substitution != nullptr && !substitution->has_priority_pattern_) {
    (void)todo->emplace_back(node);
  } else {
    auto cnode = dyn_cast_ptr<CNode>(node);
    if (cnode != nullptr) {
      const auto &inputs = cnode->inputs();
      (void)todo->insert(todo->end(), inputs.cbegin(), inputs.cend());
    }
  }
}

static void UpdateTransformingListWithUserNodes(const FuncGraphManagerPtr &manager, const AnfNodePtr &node,
                                                std::deque<AnfNodePtr> *todo, bool change, SeenNum seen) {
  if (!change) {
    return;
  }
  MS_EXCEPTION_IF_NULL(manager);
  auto &node_users = manager->node_users();
  auto users_iterator = node_users.find(node);
  if (users_iterator == node_users.end()) {
    return;
  }
  auto users = users_iterator->second;
  for (auto &use : users) {
    auto use_node = use.first;
    if (use_node == nullptr) {
      continue;
    }
    (*todo).emplace_back(use_node);
    if (use_node->seen_ == seen) {
      use_node->seen_--;
    }
  }
}

bool SubstitutionList::ApplyIRToSubstitutions(const OptimizerPtr &optimizer, const FuncGraphPtr &func_graph) const {
  MsProfileStatGuard stat_guard("opt.transform." + optimizer->name());
  FuncGraphManagerPtr manager = optimizer->manager();
  auto seen = NewSeenGeneration();
  std::deque<AnfNodePtr> todo;
  (void)todo.emplace_back(func_graph->return_node());
  bool changes = false;
  auto &all_nodes = manager->all_nodes();
  while (!todo.empty()) {
    AnfNodePtr node = std::move(todo.front());
    todo.pop_front();

    if (node == nullptr || node->seen_ == seen || !isTraversable(node) || !all_nodes.contains(node)) {
      continue;
    }
    node->seen_ = seen;

    bool change = false;
    for (auto &substitution : list_) {
      auto res = DoTransform(optimizer, node, substitution);
      if (res != nullptr) {
        change = true;
        changes = true;
        node = res;
        break;
      }
    }
    UpdateTransformingListForSubstitutions(node, &todo, change);
    UpdateTransformingListWithUserNodes(manager, node, &todo, change, seen);
  }
  return changes;
}

bool SubstitutionList::ApplySubstitutionToIR(const OptimizerPtr &optimizer, const FuncGraphPtr &func_graph,
                                             const SubstitutionPtr &substitution) const {
  MsProfileStatGuard stat_guard("opt.transform." + optimizer->name());
  FuncGraphManagerPtr manager = optimizer->manager();
  MS_EXCEPTION_IF_NULL(manager);
  auto seen = NewSeenGeneration();
  std::deque<AnfNodePtr> todo;
  (void)todo.emplace_back(func_graph->return_node());
  bool changes = false;

  auto &all_nodes = manager->all_nodes();
  while (!todo.empty()) {
    AnfNodePtr node = todo.front();
    todo.pop_front();

    if (node == nullptr || node->seen_ == seen || !isTraversable(node) || !all_nodes.contains(node)) {
      continue;
    }
    node->seen_ = seen;

    bool change = false;
    auto res = DoTransform(optimizer, node, substitution);
    if (res != nullptr) {
      change = true;
      changes = true;
      node = res;
    }
    UpdateTransformingListForIR(node, &todo, change, substitution);
    UpdateTransformingListWithUserNodes(manager, node, &todo, change, seen);
  }
  return changes;
}

void SubstitutionList::DisplayStatusOfSubstitution(const mindspore::HashMap<std::string, std::vector<bool>> &status,
                                                   const OptimizerPtr &optimizer, size_t space) const {
  constexpr int pad_width = 4;
  std::stringstream ss;
  ss << std::endl
     << "Pass: " << optimizer->name() << "(" << optimizer->current_pass_.counter << ")_"
     << optimizer->current_pass_.name << std::endl;
  for (size_t i = 0; i < list_.size(); i++) {
    auto name = list_[i]->name_;
    ss << std::left << std::setw(SizeToInt(space) + pad_width) << name << "\t";
    auto iter = status.find(name + std::to_string(i));
    if (iter == status.cend()) {
      continue;
    }
    for (auto change : iter->second) {
      ss << change << " ";
    }
    ss << std::endl;
  }
  MS_LOG(DEBUG) << ss.str();
}

bool SubstitutionList::ApplySubstitutionsToIR(const OptimizerPtr &optimizer, const FuncGraphPtr &func_graph) const {
  // Add for substitution status counting
  size_t space = 0;
  mindspore::HashMap<std::string, std::vector<bool>> status;
  if (optimizer->is_on_debug_) {
    for (size_t i = 0; i < list_.size(); i++) {
      status[list_[i]->name_ + std::to_string(i)] = {};
    }
  }

  bool changes = false;
  bool loop = true;
  while (loop) {
    loop = false;
    for (size_t i = 0; i < list_.size(); i++) {
      const auto &substitution = list_[i];
      MS_LOG(INFO) << "Start substitution: " << substitution->name_;
      bool change = ApplySubstitutionToIR(optimizer, func_graph, substitution);
      MS_LOG(INFO) << "End substitution: " << substitution->name_ << ", change: " << change;
      changes = changes || change;
      loop = loop || change;
#ifdef ENABLE_DUMP_IR
      static const auto enable_dump_pass = GetDumpConfig().enable_dump_pass_ir;
      static const auto input_name = common::GetEnv("MS_DEV_DUMP_IR_PASSES");
      auto enable_dump_pass_ir = (input_name.size() != 0) || enable_dump_pass;
      auto context = MsContext::GetInstance();
      if ((enable_dump_pass_ir && context->CanDump(kIntroductory)) || context->CanDump(kFully)) {
        auto fg_name = optimizer->name() + "_r" + std::to_string(optimizer->current_pass_.counter) + "_" +
                       optimizer->current_pass_.name + "_" + substitution->name_;
        static const auto switch_order = (common::GetEnv("MS_DEV_SAVE_GRAPHS_SORT_MODE") == "1");
        if (switch_order) {
          ExportIR(fg_name + ".ir", func_graph);
        } else {
          DumpIR(fg_name + ".ir", func_graph);
        }
        if (context->CanDump(kFully)) {
          draw::Draw(fg_name + ".dot", func_graph);
        }
      }
#endif

      // Record the status of each substitution
      if (optimizer->is_on_debug_) {
        status[substitution->name_ + std::to_string(i)].push_back(change);
        space = std::max(substitution->name_.size(), space);
      }
    }
    if (is_once_) {
      break;
    }
  }

  // Display the status of each substitution
  if (optimizer->is_on_debug_) {
    DisplayStatusOfSubstitution(status, optimizer, space);
  }
  return changes;
}

bool SubstitutionList::operator()(const FuncGraphPtr &func_graph, const OptimizerPtr &optimizer) const {
  MS_EXCEPTION_IF_NULL(optimizer);
  MS_EXCEPTION_IF_NULL(func_graph);
  FuncGraphManagerPtr manager = optimizer->manager();
  MS_EXCEPTION_IF_NULL(manager);
  manager->AddFuncGraph(func_graph);
  bool changes = false;
  static const auto traverse_mode =
    (common::GetCompileConfig("TRAVERSE_SUBSTITUTIONS_MODE") != "1" ? kOptTraverseFromIRToSubstitutions
                                                                    : kOptTraverseFromSubstitutionsToIR);
  if (traverse_mode == kOptTraverseFromIRToSubstitutions &&
      MsContext::GetInstance()->get_param<int>(MS_CTX_EXECUTION_MODE) != kPynativeMode &&
      optimizer->traverse_nodes_first() && !is_once_ && !global_sensitive_) {
    MS_LOG(INFO) << "IR >> SUB, *, " << optimizer->name() << "(r" << optimizer->current_pass_.counter << ")_"
                 << optimizer->current_pass_.name;
    changes = ApplyIRToSubstitutions(optimizer, func_graph);
  } else {
    MS_LOG(INFO) << "SUB >> IR, " << optimizer->name() << "(r" << optimizer->current_pass_.counter << ")_"
                 << optimizer->current_pass_.name;
    changes = ApplySubstitutionsToIR(optimizer, func_graph);
  }
  return changes;
}

bool SimpleRewriter::Run() {
  bool changed = false;
  auto seen = NewSeenGeneration();
  std::deque<AnfNodePtr> todo;
  auto add_todo = [&seen, &todo](const AnfNodePtr &node) {
    if (node != nullptr && node->seen_ != seen) {
      (void)todo.emplace_back(node);
    }
  };
  (void)todo.emplace_back(root_graph_->return_node());
  auto &all_nodes = manager_->all_nodes();
  while (!todo.empty()) {
    AnfNodePtr node = std::move(todo.front());
    todo.pop_front();
    if (node == nullptr || node->seen_ == seen || !all_nodes.contains(node)) {
      continue;
    }
    node->seen_ = seen;
    auto cnode = node->cast_ptr<CNode>();
    if (cnode != nullptr) {
      for (auto &input : cnode->weak_inputs()) {
        add_todo(input.lock());
      }
    } else {
      auto fg = GetValuePtr<FuncGraph>(node);
      if (fg != nullptr) {
        add_todo(fg->return_node());
      }
    }
    TraceGuard trace_guard(std::make_shared<TraceOpt>(node->debug_info()));
    ScopeGuard scope_guard(node->scope());
    auto new_node = NodeRewrite(node);
    if (new_node != nullptr) {
      (void)manager_->Replace(node, new_node);
      changed = true;
      // Need push the users of new_node to the deque.
      UpdateTransformingListWithUserNodes(manager_, new_node, &todo, changed, seen);
    }
  }
  return changed;
}
}  // namespace opt
}  // namespace mindspore
