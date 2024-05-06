/**
 * This is the C++ adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
 *
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#include "ir/graph_utils.h"

#include <algorithm>
#include <deque>
#include <utility>

#include "ir/anf.h"
#include "ir/func_graph.h"
#include "utils/hash_map.h"
#include "utils/hash_set.h"
#include "utils/log_adapter.h"
#include "utils/ms_context.h"
#include "include/common/utils/utils.h"

namespace mindspore {
// Dump the circle from the strike node `next`.
static size_t DumpSortingCircleList(const std::deque<AnfNodePtr> &todo, const AnfNodePtr &next, SeenNum seen) {
  size_t pos = 0;
  auto circle_node_it = std::find(todo.begin(), todo.end(), next);
  for (; circle_node_it != todo.end(); circle_node_it++) {
    auto circle_node = *circle_node_it;
    if (circle_node->seen_ == seen) {
      MS_LOG(ERROR) << "#" << pos << ": " << circle_node->DebugString();
      pos++;
    }
  }
  return pos;
}

AnfNodePtrList TopoSort(const AnfNodePtr &root, const SuccFunc &succ, const IncludeFunc &include) {
  constexpr auto kVecReserve = 64;
  constexpr auto kRecursiveLevel = 2;
  AnfNodePtrList res;
  if (root == nullptr) {
    return res;
  }
  res.reserve(kVecReserve);
  auto seen = NewSeenGeneration();
  std::deque<AnfNodePtr> todo;
  (void)todo.emplace_back(root);
  while (!todo.empty()) {
    AnfNodePtr &node = todo.back();
    if (node->extra_seen_ == seen) {  // We use extra_seen_ as finish flag
      todo.pop_back();
      continue;
    }
    auto incl = include(node);
    if (node->seen_ == seen) {  // We use seen_ as checking flag
      node->extra_seen_ = seen;
      if (incl != EXCLUDE) {
        (void)res.emplace_back(std::move(node));
      }
      todo.pop_back();
      continue;
    }
    node->seen_ = seen;
    if (incl == FOLLOW) {
      for (auto &weak_next : succ(node)) {
        auto next = weak_next.lock();
        if (next == nullptr || next->extra_seen_ == seen) {
          continue;
        }
        if (next->seen_ != seen) {
          (void)todo.emplace_back(std::move(next));
          continue;
        }
        auto fg = next->func_graph();
        if (fg != nullptr && fg->return_node() == next) {
          continue;
        }
        // To dump all nodes in a circle.
        MS_LOG(ERROR) << "Graph cycle exists. Circle is: ";
        auto circle_len = DumpSortingCircleList(todo, next, seen);
        MS_LOG(INTERNAL_EXCEPTION) << "Graph cycle exists, size: " << circle_len
                                   << ", strike node: " << next->DebugString(kRecursiveLevel);
      }
    } else if (incl > EXCLUDE) {  // Not NOFOLLOW or EXCLUDE
      MS_LOG(INTERNAL_EXCEPTION) << "The result of include(node) must be one of: \"follow\", \"nofollow\", \"exclude\"";
    }
  }
  return res;
}

// @deprecated
// To use 'AnfNodePtrList TopoSort(const AnfNodePtr &, const SuccFunc &, const IncludeFunc &)' instead.
AnfNodePtrList TopoSort(const AnfNodePtr &root, const DeprecatedSuccFunc &deprecated_succ, const IncludeFunc &include) {
  SuccFunc compatible_adapter_succ = [&deprecated_succ](const AnfNodePtr &node) -> AnfNodeWeakPtrList {
    auto nodes = deprecated_succ(node);
    AnfNodeWeakPtrList weak_nodes;
    weak_nodes.reserve(nodes.size());
    std::transform(nodes.cbegin(), nodes.cend(), std::back_inserter(weak_nodes),
                   [](const AnfNodePtr &node) -> AnfNodeWeakPtr { return AnfNodeWeakPtr(node); });
    return weak_nodes;
  };
  return TopoSort(root, compatible_adapter_succ, include);
}

// Search all CNode in root's graph only.
std::vector<CNodePtr> BroadFirstSearchGraphCNodes(const CNodePtr &root) {
  constexpr size_t kVecReserve = 64;
  std::vector<CNodePtr> cnodes;
  cnodes.reserve(kVecReserve);
  auto seen = NewSeenGeneration();
  MS_EXCEPTION_IF_NULL(root);
  root->seen_ = seen;
  (void)cnodes.emplace_back(root);
  for (size_t i = 0; i < cnodes.size(); ++i) {
    CNodePtr &node = cnodes[i];
    for (auto &weak_input : node->weak_inputs()) {
      auto input = weak_input.lock();
      if (input == nullptr) {
        MS_LOG(INTERNAL_EXCEPTION) << "The input is null, node: " << node << "/" << node->DebugString();
      }
      if (input->seen_ == seen) {
        continue;
      }
      input->seen_ = seen;
      auto input_cnode = input->cast<CNodePtr>();
      if (input_cnode != nullptr) {
        (void)cnodes.emplace_back(std::move(input_cnode));
      }
    }
  }
  return cnodes;
}

// Search all CNode match the predicate in roots' graph only.
CNodePtr BroadFirstSearchFirstOf(const std::vector<CNodePtr> &roots, const MatchFunc &match_predicate) {
  std::deque<CNodePtr> todo;
  (void)todo.insert(todo.end(), roots.begin(), roots.end());
  auto seen = NewSeenGeneration();
  while (!todo.empty()) {
    CNodePtr top = todo.front();
    todo.pop_front();
    if (match_predicate(top)) {
      return top;
    }
    for (auto &weak_input : top->weak_inputs()) {
      auto input = weak_input.lock();
      MS_EXCEPTION_IF_NULL(input);
      if (input->seen_ == seen) {
        continue;
      }

      if (input->isa<CNode>()) {
        todo.push_back(input->cast<CNodePtr>());
      }
      input->seen_ = seen;
    }
  }
  return nullptr;
}

std::vector<FuncGraphPtr> BroadFirstSearchGraphUsed(const FuncGraphPtr &root, const GraphFilterFunc &filter) {
  std::vector<FuncGraphPtr> todo;
  todo.push_back(root);
  auto seen = NewSeenGeneration();
  size_t top_idx = 0;
  while (top_idx < todo.size()) {
    FuncGraphPtr top = todo[top_idx];
    top_idx++;
    auto used = top->func_graphs_used();
    for (auto &item : used) {
      if (item.first->seen_ == seen) {
        continue;
      }
      if (filter && filter(item.first)) {
        continue;
      }
      todo.push_back(item.first);
      item.first->seen_ = seen;
    }
  }
  return todo;
}

// To get CNode inputs to a vector as successors for TopoSort().
static void FetchCNodeSuccessors(const CNodePtr &cnode, AnfNodeWeakPtrList *vecs) {
  auto &inputs = cnode->weak_inputs();
  vecs->reserve(vecs->size() + inputs.size());

  // To keep sort order from left to right in default, if kAttrTopoSortRhsFirst not set.
  auto attr_sort_rhs_first = cnode->GetAttr(kAttrTopoSortRhsFirst);
  auto sort_rhs_first =
    attr_sort_rhs_first != nullptr && attr_sort_rhs_first->isa<BoolImm>() && GetValue<bool>(attr_sort_rhs_first);
  if (sort_rhs_first) {
    (void)vecs->insert(vecs->end(), inputs.cbegin(), inputs.cend());
  } else {
    (void)vecs->insert(vecs->end(), inputs.crbegin(), inputs.crend());
  }
}

AnfNodeWeakPtrList SuccDeeperSimple(const AnfNodePtr &node) {
  AnfNodeWeakPtrList vecs;
  if (node == nullptr) {
    return vecs;
  }

  auto graph = GetValuePtr<FuncGraph>(node);
  if (graph != nullptr) {
    auto &res = graph->return_node();
    if (res != nullptr) {
      vecs.push_back(res);
    }
  } else if (node->isa<CNode>()) {
    FetchCNodeSuccessors(node->cast<CNodePtr>(), &vecs);
  }
  return vecs;
}

AnfNodeWeakPtrList SuccIncoming(const AnfNodePtr &node) {
  AnfNodeWeakPtrList vecs;
  auto cnode = dyn_cast<CNode>(node);
  if (cnode != nullptr) {
    FetchCNodeSuccessors(cnode, &vecs);
  }
  return vecs;
}

AnfNodeWeakPtrList SuccIncludeFV(const FuncGraphPtr &fg, const AnfNodePtr &node) {
  auto cnode = dyn_cast<CNode>(node);
  if (cnode == nullptr) {
    return {};
  }
  AnfNodeWeakPtrList vecs;
  const auto &inputs = cnode->inputs();
  // Check if free variables used.
  for (const auto &input : inputs) {
    auto input_fg = GetValuePtr<FuncGraph>(input);
    if (input_fg != nullptr) {
      for (auto &fv : input_fg->free_variables_nodes()) {
        MS_EXCEPTION_IF_NULL(fv);
        if (fv->func_graph() == fg && fg->nodes().contains(fv)) {
          vecs.push_back(fv);
        }
      }
    }
  }
  FetchCNodeSuccessors(cnode, &vecs);
  return vecs;
}

AnfNodeWeakPtrList SuccWithFilter(const GraphFilterFunc &graph_filter, const AnfNodePtr &node) {
  AnfNodeWeakPtrList vecs;
  if (node == nullptr) {
    return vecs;
  }

  auto graph = GetValueNode<FuncGraphPtr>(node);
  if (graph != nullptr) {
    if (graph_filter != nullptr && graph_filter(graph)) {
      return vecs;
    }
    auto &res = graph->return_node();
    if (res != nullptr) {
      vecs.push_back(res);
    }
  } else if (node->isa<CNode>()) {
    FetchCNodeSuccessors(node->cast<CNodePtr>(), &vecs);
  }
  return vecs;
}

const AnfNodePtrList GetInputs(const AnfNodePtr &node) {
  static AnfNodePtrList empty_inputs;
  auto cnode = dyn_cast_ptr<CNode>(node);
  if (cnode != nullptr) {
    return cnode->inputs();
  }
  return empty_inputs;
}

const AnfNodeWeakPtrList &GetWeakInputs(const AnfNodePtr &node) {
  static AnfNodeWeakPtrList empty_inputs;
  auto cnode = dyn_cast_ptr<CNode>(node);
  if (cnode != nullptr) {
    return cnode->weak_inputs();
  }
  return empty_inputs;
}

IncludeType IncludeBelongGraph(const FuncGraphPtr &fg, const AnfNodePtr &node) {
  if (node->func_graph() == fg) {
    return FOLLOW;
  } else {
    return EXCLUDE;
  }
}
}  // namespace mindspore
