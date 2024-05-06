/**
 * Copyright 2020-2021 Huawei Technologies Co., Ltd
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

#include "backend/graph_compiler/graph_partition.h"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include "include/common/utils/anfalgo.h"
#include "mindspore/core/ops/array_ops.h"
#include "mindspore/core/ops/framework_ops.h"
#include "mindspore/core/ops/nn_ops.h"
#include "mindspore/core/ops/sequence_ops.h"
#include "mindspore/core/ops/structure_ops.h"
#include "mindspore/ccsrc/runtime/hardware/device_context.h"
#include "utils/anf_utils.h"
#include "utils/ms_context.h"
namespace mindspore {
namespace compile {
namespace {
constexpr const char kOnlySupport2DiffTarget[] = "Only support two different target";
const size_t kMaxDiffTargetNum = 2;

std::string GetOtherTarget(const std::vector<AnfNodePtr> &nodes) {
  auto context_ptr = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context_ptr);
  std::string default_target = context_ptr->get_param<std::string>(MS_CTX_DEVICE_TARGET);
  for (auto &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    if (!node->isa<CNode>()) {
      continue;
    }
    std::string cur_target = GetCNodeTarget(node);
    if (cur_target != default_target) {
      return cur_target;
    }
  }
  return "";
}

void CheckDiffTargetNum(const std::vector<AnfNodePtr> &nodes) {
  auto context_ptr = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context_ptr);
  std::string default_target = context_ptr->get_param<std::string>(MS_CTX_DEVICE_TARGET);
  std::set<std::string> target_set;
  (void)target_set.emplace(default_target);
  for (auto &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    if (!node->isa<CNode>()) {
      continue;
    }
    std::string cur_target = GetCNodeTarget(node);
    if (target_set.find(cur_target) == target_set.end()) {
      (void)target_set.emplace(cur_target);
    }
  }

  if (target_set.size() > kMaxDiffTargetNum) {
    MS_LOG(EXCEPTION) << kOnlySupport2DiffTarget;
  }
}

void CalcNodeRefCount(const FuncGraphPtr &graph, std::map<AnfNodePtr, size_t> *nodes_ref) {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(nodes_ref);
  std::queue<AnfNodePtr> queue;
  queue.push(graph->get_return());
  std::set<AnfNodePtr> visited;
  while (!queue.empty()) {
    auto node = queue.front();
    queue.pop();
    MS_EXCEPTION_IF_NULL(node);
    if (!node->isa<CNode>()) {
      continue;
    }
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    for (auto &input : cnode->inputs()) {
      auto iter = nodes_ref->find(input);
      if (iter != nodes_ref->end()) {
        iter->second++;
      } else {
        (void)nodes_ref->emplace(input, 1UL);
      }
      if (visited.find(input) != visited.end()) {
        continue;
      }
      visited.insert(input);
      queue.push(input);
    }
  }
}

std::vector<AnfNodePtr> ReorderVirtualNode(const std::vector<AnfNodePtr> &nodes, const PrimitivePtr &reorder_prim) {
  std::vector<AnfNodePtr> result;
  std::map<size_t, std::vector<AnfNodePtr>> insert_positions;
  std::map<AnfNodePtr, size_t> node_positions;
  auto add_insert_position = [&insert_positions, &node_positions](const AnfNodePtr &node, const AnfNodePtr &parent) {
    if (parent == nullptr) {
      return false;
    }
    const auto &iter = node_positions.find(parent);
    if (iter != node_positions.end()) {
      size_t position = iter->second;
      const auto &iter_nodes = insert_positions.find(position);
      if (iter_nodes != insert_positions.end()) {
        iter_nodes->second.push_back(node);
      } else {
        (void)insert_positions.emplace(position, std::vector<AnfNodePtr>{node});
      }
      return true;
    }
    return false;
  };
  for (auto &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    if (IsPrimitiveCNode(node, reorder_prim)) {
      auto cnode = node->cast<CNodePtr>();
      MS_EXCEPTION_IF_NULL(cnode);
      auto &inputs = cnode->inputs();
      AnfNodePtr parent = nullptr;
      const size_t depend_input_size = 2;
      if (reorder_prim == prim::kPrimDepend && inputs.size() == depend_input_size + 1 && !inputs[1]->isa<CNode>()) {
        parent = inputs[depend_input_size];
      } else if (reorder_prim == prim::kPrimTupleGetItem && inputs.size() > 1) {
        parent = inputs[1];
      }
      if (add_insert_position(node, parent)) {
        continue;
      }
    }
    result.emplace_back(node);
    node_positions[node] = result.size();
  }

  size_t insert_num = 0;
  for (const auto &item : insert_positions) {
    auto position = SizeToLong(item.first + insert_num);
    (void)result.insert(result.cbegin() + position, item.second.cbegin(), item.second.cend());
    insert_num += item.second.size();
  }
  return result;
}

std::vector<AnfNodePtr> GetNextNodes(const AnfNodePtr &node, std::map<AnfNodePtr, size_t> *nodes_ref,
                                     std::vector<AnfNodePtr> *result) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(nodes_ref);
  MS_EXCEPTION_IF_NULL(result);
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  auto node_inputs = cnode->inputs();
  if (!IsPrimitiveCNode(node, prim::kPrimSwitch)) {
    std::reverse(node_inputs.begin(), node_inputs.end());
    return node_inputs;
  }
  std::vector<AnfNodePtr> extend_inputs;
  for (auto &input : node_inputs) {
    MS_EXCEPTION_IF_NULL(input);
    if (IsPrimitiveCNode(input, prim::kPrimPartial)) {
      auto iter = nodes_ref->find(input);
      if (iter != nodes_ref->end() && iter->second == 1) {
        iter->second--;
        result->emplace_back(input);
        auto partial_cnode = input->cast<CNodePtr>();
        MS_EXCEPTION_IF_NULL(partial_cnode);
        auto partial_inputs = partial_cnode->inputs();
        std::reverse(partial_inputs.begin(), partial_inputs.end());
        (void)extend_inputs.insert(extend_inputs.cend(), partial_inputs.cbegin(), partial_inputs.cend());
        continue;
      }
    }
    extend_inputs.emplace_back(input);
  }
  return extend_inputs;
}

std::vector<AnfNodePtr> SplitSort(const FuncGraphPtr &graph, const std::string &default_target) {
  MS_EXCEPTION_IF_NULL(graph);
  std::vector<AnfNodePtr> result;
  std::stack<AnfNodePtr> to_visit;
  std::stack<AnfNodePtr> next_to_visit;
  std::map<AnfNodePtr, size_t> nodes_ref;
  CalcNodeRefCount(graph, &nodes_ref);
  std::string handle_target = default_target;
  std::string next_target;
  to_visit.push(graph->get_return());
  while (!to_visit.empty() || !next_to_visit.empty()) {
    if (to_visit.empty()) {
      to_visit.swap(next_to_visit);
      handle_target = next_target;
    }
    auto node = to_visit.top();
    MS_EXCEPTION_IF_NULL(node);
    to_visit.pop();
    result.emplace_back(node);
    if (!node->isa<CNode>()) {
      continue;
    }
    auto next_nodes = GetNextNodes(node, &nodes_ref, &result);
    for (auto &input : next_nodes) {
      MS_EXCEPTION_IF_NULL(input);
      auto iter = nodes_ref.find(input);
      if (iter != nodes_ref.end()) {
        iter->second--;
        if (iter->second != 0) {
          continue;
        }
      }
      if (!input->isa<CNode>()) {
        to_visit.push(input);
        continue;
      }
      std::string input_target = GetCNodeTarget(input);
      if (input_target == handle_target) {
        to_visit.push(input);
      } else if (next_to_visit.empty() || input_target == next_target) {
        next_to_visit.push(input);
        next_target = input_target;
      } else {
        MS_LOG(EXCEPTION) << kOnlySupport2DiffTarget;
      }
    }
  }
  std::reverse(result.begin(), result.end());
  return result;
}

std::vector<AnfNodePtr> LazySort(const std::vector<AnfNodePtr> &nodes, const PrimitiveSet &primitive_set) {
  std::vector<AnfNodePtr> result;
  std::set<AnfNodePtr> visited;
  std::vector<AnfNodePtr> lazy_sort_node;
  for (auto &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    if (IsOneOfPrimitiveCNode(node, primitive_set)) {
      lazy_sort_node.emplace_back(node);
    } else if (!node->isa<CNode>()) {
      result.emplace_back(node);
      visited.insert(node);
    } else {
      auto cnode = node->cast<CNodePtr>();
      MS_EXCEPTION_IF_NULL(cnode);
      auto node_inputs = cnode->inputs();
      bool all_visited = std::all_of(node_inputs.begin(), node_inputs.end(), [&visited](const AnfNodePtr &input) {
        return visited.find(input) != visited.end();
      });
      if (all_visited) {
        result.emplace_back(node);
        visited.insert(node);
      } else {
        lazy_sort_node.emplace_back(node);
      }
    }
  }
  result.insert(result.end(), lazy_sort_node.begin(), lazy_sort_node.end());
  return result;
}

struct GraphNodesDependencyInfo {
  std::stack<AnfNodePtr> independent_nodes_;
  std::map<AnfNodePtr, size_t> input_num_;
  std::map<AnfNodePtr, std::vector<AnfNodePtr>> output_edges_;
};

GraphNodesDependencyInfo GetNodesDependencyInfo(const FuncGraphPtr &graph) {
  MS_EXCEPTION_IF_NULL(graph);
  GraphNodesDependencyInfo info;
  std::stack<AnfNodePtr> to_visit;
  std::map<AnfNodePtr, size_t> nodes_ref;
  CalcNodeRefCount(graph, &nodes_ref);
  to_visit.push(graph->get_return());
  while (!to_visit.empty()) {
    auto node = to_visit.top();
    to_visit.pop();
    if (node == nullptr || !node->isa<CNode>()) {
      continue;
    }
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    auto node_inputs = cnode->inputs();
    bool independent = true;
    for (auto &input : node_inputs) {
      MS_EXCEPTION_IF_NULL(input);
      if (input->isa<CNode>()) {
        independent = false;
        auto output_edge_iter = info.output_edges_.find(input);
        if (output_edge_iter != info.output_edges_.end()) {
          auto &edges = output_edge_iter->second;
          edges.emplace_back(node);
        } else {
          info.output_edges_[input] = {node};
        }
        auto input_num_iter = info.input_num_.find(node);
        if (input_num_iter != info.input_num_.end()) {
          input_num_iter->second++;
        } else {
          info.input_num_[node] = 1;
        }
      }
      auto ref_iter = nodes_ref.find(input);
      if (ref_iter != nodes_ref.end()) {
        ref_iter->second--;
        if (ref_iter->second != 0) {
          continue;
        }
      }
      to_visit.push(input);
    }
    if (independent) {
      info.independent_nodes_.push(node);
    }
  }
  return info;
}

struct VisitNodesInfo {
  std::queue<AnfNodePtr> default_target_nodes_;
  std::queue<AnfNodePtr> other_target_nodes_;
  std::map<AnfNodePtr, AnfNodePtr> seed_cast_next_node_;
};

VisitNodesInfo GetVisitNodesInfo(const GraphNodesDependencyInfo &dependency_info, const std::string &default_target,
                                 const std::string &other_target) {
  VisitNodesInfo result;
  auto independent_nodes = dependency_info.independent_nodes_;
  while (!independent_nodes.empty()) {
    auto seed_node = independent_nodes.top();
    independent_nodes.pop();
    MS_EXCEPTION_IF_NULL(seed_node);
    auto node_target = GetCNodeTarget(seed_node);
    if (IsPrimitiveCNode(seed_node, prim::kPrimCast)) {
      auto output_edges_iter = dependency_info.output_edges_.find(seed_node);
      if (output_edges_iter != dependency_info.output_edges_.end() && output_edges_iter->second.size() == 1) {
        auto &cast_next_node = output_edges_iter->second[0];
        auto input_num_iter = dependency_info.input_num_.find(cast_next_node);
        if (input_num_iter == dependency_info.input_num_.end()) {
          MS_LOG(EXCEPTION) << "Node input num not found!";
        }
        if (input_num_iter->second > 1 && node_target == GetCNodeTarget(cast_next_node)) {
          result.seed_cast_next_node_[cast_next_node] = seed_node;
          continue;
        }
      }
    }
    if (node_target == default_target) {
      result.default_target_nodes_.push(seed_node);
    } else if (node_target == other_target) {
      result.other_target_nodes_.push(seed_node);
    } else {
      MS_LOG(EXCEPTION) << "Only support two difference targets";
    }
  }
  return result;
}

std::string ParallelSortDecideNextHandleTarget(const std::vector<AnfNodePtr> &output_edges,
                                               const std::string &node_target,
                                               std::map<AnfNodePtr, std::string> *node_input_target_map) {
  MS_EXCEPTION_IF_NULL(node_input_target_map);
  std::string next_target = node_target;
  for (auto &dst_node : output_edges) {
    auto input_target_iter = node_input_target_map->find(dst_node);
    if (input_target_iter != node_input_target_map->end() && input_target_iter->second != node_target) {
      next_target = input_target_iter->second;
      break;
    }
    auto dst_node_target = GetCNodeTarget(dst_node);
    if (dst_node_target != node_target) {
      next_target = dst_node_target;
      break;
    }
  }
  for (auto &dst_node : output_edges) {
    (*node_input_target_map)[dst_node] = node_target;
  }
  return next_target;
}

void ParallelSortVisitNodeEdges(const std::vector<AnfNodePtr> &output_edges, GraphNodesDependencyInfo *dependency_info,
                                VisitNodesInfo *visit_nodes_info, const std::string &default_target) {
  MS_EXCEPTION_IF_NULL(dependency_info);
  MS_EXCEPTION_IF_NULL(visit_nodes_info);
  for (auto &dst_node : output_edges) {
    auto dst_node_target = GetCNodeTarget(dst_node);
    auto input_num_iter = dependency_info->input_num_.find(dst_node);
    if (input_num_iter == dependency_info->input_num_.end()) {
      MS_LOG(EXCEPTION) << "Node input num not found!";
    }
    input_num_iter->second--;
    if (input_num_iter->second == 1 &&
        visit_nodes_info->seed_cast_next_node_.find(dst_node) != visit_nodes_info->seed_cast_next_node_.end()) {
      input_num_iter->second--;
    }
    if (input_num_iter->second > 0) {
      continue;
    }
    if (dst_node_target == default_target) {
      visit_nodes_info->default_target_nodes_.push(dst_node);
    } else {
      visit_nodes_info->other_target_nodes_.push(dst_node);
    }
  }
}

std::vector<AnfNodePtr> ParallelSort(const FuncGraphPtr &graph, const std::string &default_target,
                                     const std::string &other_target) {
  MS_EXCEPTION_IF_NULL(graph);
  auto dependency_info = GetNodesDependencyInfo(graph);
  auto visit_nodes_info = GetVisitNodesInfo(dependency_info, default_target, other_target);
  std::vector<AnfNodePtr> result;
  std::string handle_target;
  if (!visit_nodes_info.default_target_nodes_.empty()) {
    handle_target = default_target;
  } else {
    handle_target = other_target;
  }
  std::map<AnfNodePtr, std::string> node_input_target_map;
  while (!visit_nodes_info.default_target_nodes_.empty() || !visit_nodes_info.other_target_nodes_.empty()) {
    AnfNodePtr ready_node;
    if ((!visit_nodes_info.default_target_nodes_.empty() && handle_target == default_target) ||
        visit_nodes_info.other_target_nodes_.empty()) {
      ready_node = visit_nodes_info.default_target_nodes_.front();
      visit_nodes_info.default_target_nodes_.pop();
      handle_target = default_target;
    } else {
      ready_node = visit_nodes_info.other_target_nodes_.front();
      visit_nodes_info.other_target_nodes_.pop();
      handle_target = other_target;
    }
    MS_EXCEPTION_IF_NULL(ready_node);
    const auto &cast_map_iter = visit_nodes_info.seed_cast_next_node_.find(ready_node);
    if (cast_map_iter != visit_nodes_info.seed_cast_next_node_.cend()) {
      result.emplace_back(cast_map_iter->second);
    }
    result.emplace_back(ready_node);
    auto output_edges_iter = dependency_info.output_edges_.find(ready_node);
    if (output_edges_iter == dependency_info.output_edges_.end()) {
      continue;
    }
    auto &output_edges = output_edges_iter->second;
    handle_target = ParallelSortDecideNextHandleTarget(output_edges, handle_target, &node_input_target_map);
    ParallelSortVisitNodeEdges(output_edges, &dependency_info, &visit_nodes_info, default_target);
  }
  return result;
}

void AddSegmentDependency(const FuncGraphPtr &graph, const std::map<AnfNodePtr, GraphSegmentPtr> &node_to_segment) {
  MS_EXCEPTION_IF_NULL(graph);
  std::stack<AnfNodePtr> to_visit;
  std::map<AnfNodePtr, size_t> nodes_ref;
  CalcNodeRefCount(graph, &nodes_ref);
  to_visit.push(graph->get_return());
  while (!to_visit.empty()) {
    auto &node = to_visit.top();
    MS_EXCEPTION_IF_NULL(node);
    to_visit.pop();
    if (!node->isa<CNode>()) {
      continue;
    }
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    auto node_inputs = cnode->inputs();
    GraphSegmentPtr node_segment{nullptr};
    auto node_iter = node_to_segment.find(node);
    if (node_iter != node_to_segment.end()) {
      node_segment = node_iter->second;
    }
    for (auto &input : node_inputs) {
      if (node_segment != nullptr && !node_segment->is_cut_ && input != nullptr && input->isa<CNode>()) {
        GraphSegmentPtr input_segment{nullptr};
        auto input_iter = node_to_segment.find(input);
        if (input_iter != node_to_segment.end()) {
          input_segment = input_iter->second;
        }
        if (input_segment != nullptr && input_segment != node_segment && !input_segment->is_cut_) {
          node_segment->AddPreSegment(input_segment);
        }
      }
      auto ref_iter = nodes_ref.find(input);
      if (ref_iter != nodes_ref.end()) {
        ref_iter->second--;
        if (ref_iter->second != 0) {
          continue;
        }
      }
      to_visit.push(input);
    }
  }
}

void RemoveUselessDependency(const std::vector<GraphSegmentPtr> *segments) {
  MS_EXCEPTION_IF_NULL(segments);
  for (auto &segment : *segments) {
    MS_EXCEPTION_IF_NULL(segment);
    if (segment->is_cut_) {
      continue;
    }
    bool total_virtual_node = true;
    for (auto &node : segment->nodes_) {
      if (IsPrimitiveCNode(node, prim::kPrimImageSummary) || IsPrimitiveCNode(node, prim::kPrimScalarSummary) ||
          IsPrimitiveCNode(node, prim::kPrimTensorSummary) || IsPrimitiveCNode(node, prim::kPrimHistogramSummary) ||
          IsPrimitiveCNode(node, prim::kPrimDepend) || IsPrimitiveCNode(node, prim::kPrimLoad) ||
          IsPrimitiveCNode(node, prim::kPrimUpdateState) || IsPrimitiveCNode(node, prim::kPrimMakeTuple) ||
          IsPrimitiveCNode(node, prim::kPrimTupleGetItem)) {
        continue;
      }
      total_virtual_node = false;
      break;
    }
    if (total_virtual_node) {
      segment->pre_segments_.clear();
    }
  }
}

bool IsSubGraph(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (node->isa<CNode>()) {
    auto cnode = node->cast<CNodePtr>();
    auto &inputs = cnode->inputs();
    if (inputs.empty()) {
      MS_LOG(EXCEPTION) << "Inputs of apply node is empty";
    }

    AnfNodePtr fn = inputs[0];
    if (!IsValueNode<Primitive>(fn)) {
      return false;
    }
    auto node_prim = GetValueNode<PrimitivePtr>(fn);
    if (node_prim->name() == prim::kPrimPartial->name()) {
      return true;
    }
  } else if (IsValueNode<FuncGraph>(node)) {
    return true;
  }
  return false;
}

void AddSegment(const std::vector<AnfNodePtr> &nodes, std::vector<GraphSegmentPtr> *segments,
                std::map<AnfNodePtr, GraphSegmentPtr> *node_to_segment) {
  MS_EXCEPTION_IF_NULL(segments);
  MS_EXCEPTION_IF_NULL(node_to_segment);
  auto segment = std::make_shared<GraphSegment>(nodes, false);
  segments->emplace_back(segment);
  for (auto &node : nodes) {
    (*node_to_segment)[node] = segment;
  }
}

struct SplitDynamicNodesHelper {
  void AddNode(const AnfNodePtr &node, bool is_dynamic_shape) {
    if (is_dynamic_shape) {
      pre_dynamic_nodes.emplace_back(node);
      pre_dynamic_nodes_set.insert(node);
    } else {
      pre_common_nodes.emplace_back(node);
      pre_common_nodes_set.insert(node);
    }
    pre_nodes.emplace_back(node);
  }

  void AddSegments(std::vector<GraphSegmentPtr> *segments, std::map<AnfNodePtr, GraphSegmentPtr> *node_to_segment) {
    MS_EXCEPTION_IF_NULL(segments);
    MS_EXCEPTION_IF_NULL(node_to_segment);
    if (pre_nodes.size() < merge_node_threshold) {
      AddSegment(pre_nodes, segments, node_to_segment);
    } else {
      if (!pre_common_nodes.empty()) {
        AddSegment(pre_common_nodes, segments, node_to_segment);
      }
      if (!pre_dynamic_nodes.empty()) {
        AddSegment(pre_dynamic_nodes, segments, node_to_segment);
      }
    }
    pre_common_nodes.clear();
    pre_common_nodes_set.clear();
    pre_dynamic_nodes.clear();
    pre_dynamic_nodes_set.clear();
    pre_nodes.clear();
  }
  std::vector<AnfNodePtr> pre_nodes;
  std::vector<AnfNodePtr> pre_dynamic_nodes;
  std::vector<AnfNodePtr> pre_common_nodes;
  std::set<AnfNodePtr> pre_common_nodes_set;
  std::set<AnfNodePtr> pre_dynamic_nodes_set;
  size_t merge_node_threshold = 6;
};

void SplitDynamicNodeSegment(const std::vector<AnfNodePtr> &segment_nodes, std::vector<GraphSegmentPtr> *segments,
                             std::map<AnfNodePtr, GraphSegmentPtr> *node_to_segment,
                             const std::set<AnfNodePtr> &dynamic_nodes_set) {
  SplitDynamicNodesHelper helper;
  for (auto &node : segment_nodes) {
    MS_EXCEPTION_IF_NULL(node);
    auto cnode = node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(cnode);
    auto &inputs = cnode->inputs();
    bool has_dynamic_shape = dynamic_nodes_set.find(node) != dynamic_nodes_set.cend();
    bool depend_common_node = false;
    bool depend_dynamic_node = false;
    bool is_last_node_dynamic = false;
    for (size_t i = 1; i < inputs.size(); ++i) {
      if (dynamic_nodes_set.find(inputs[i]) != dynamic_nodes_set.end()) {
        has_dynamic_shape = true;
      }
      if (helper.pre_common_nodes_set.find(inputs[i]) != helper.pre_common_nodes_set.end()) {
        depend_common_node = true;
      }
      if (helper.pre_dynamic_nodes_set.find(inputs[i]) != helper.pre_dynamic_nodes_set.end()) {
        depend_dynamic_node = true;
      }
    }
    if (has_dynamic_shape) {
      if (depend_common_node) {
        helper.AddSegments(segments, node_to_segment);
      }
      is_last_node_dynamic = true;
    } else {
      if (depend_dynamic_node) {
        helper.AddSegments(segments, node_to_segment);
      }
      is_last_node_dynamic = false;
    }
    helper.AddNode(node, is_last_node_dynamic);
  }
  helper.AddSegments(segments, node_to_segment);
}

void NodesToSegments(const std::vector<AnfNodePtr> &segment_nodes, std::vector<GraphSegmentPtr> *segments,
                     std::map<AnfNodePtr, GraphSegmentPtr> *node_to_segment) {
  if (segment_nodes.empty()) {
    return;
  }
  auto segment_target = GetCNodeTarget(segment_nodes[0]);
  auto context_ptr = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context_ptr);
  bool enable_mindrt = context_ptr->get_param<bool>(MS_CTX_ENABLE_MINDRT);
  if (segment_target != kAscendDevice || enable_mindrt) {
    AddSegment(segment_nodes, segments, node_to_segment);
    return;
  }
  MS_EXCEPTION_IF_NULL(segments);
  MS_EXCEPTION_IF_NULL(node_to_segment);
  std::set<AnfNodePtr> dynamic_nodes_set;
  for (auto &node : segment_nodes) {
    MS_EXCEPTION_IF_NULL(node);
    auto cnode = node->cast<CNodePtr>();
    if (common::AnfAlgo::IsNodeOutputDynamicShape(cnode)) {
      (void)dynamic_nodes_set.insert(node);
    }
  }
  if (dynamic_nodes_set.empty()) {
    AddSegment(segment_nodes, segments, node_to_segment);
    return;
  }
  SplitDynamicNodeSegment(segment_nodes, segments, node_to_segment, dynamic_nodes_set);
}

void ProcessCloseFollowing(const FuncGraphPtr &graph, const AnfNodePtr &cut_node,
                           mindspore::HashMap<AnfNodePtr, std::vector<AnfNodePtr>> *close_following) {
  auto context_ptr = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context_ptr);
  static const bool is_enable_ge = context_ptr->backend_policy() == "ge";
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  const auto is_cell_reuse = context->CellReuseLevel() != CellReuseLevel::kNoCellReuse;
  if (!is_enable_ge || !is_cell_reuse) {
    return;
  }
  auto cnode = dyn_cast_ptr<CNode>(cut_node);
  if (cnode == nullptr || !cnode->HasPrimalAttr(kAttrNodeWithoutOutput)) {
    return;
  }

  auto manager = graph->manager();
  if (manager == nullptr) {
    return;
  }
  auto &node_user = manager->node_users();
  if (node_user[cut_node].size() != 1) {
    MS_LOG(EXCEPTION) << "Error Node without output: " << cut_node->fullname_with_scope() << ", node user must be 1";
  }

  std::vector<AnfNodePtr> follow_set;
  auto seen = NewSeenGeneration();
  std::queue<AnfNodePtr> node_queue;
  node_queue.push(cut_node);

  while (!node_queue.empty()) {
    auto top_node = node_queue.front();
    node_queue.pop();
    top_node->seen_ = seen;
    follow_set.push_back(top_node);
    for (auto &next : node_user[top_node]) {
      auto next_node = next.first;
      if (next_node->seen_ == seen) {
        continue;
      }
      auto next_cnode = dyn_cast_ptr<CNode>(next_node);
      if (next_cnode != nullptr && next_cnode->HasPrimalAttr(kAttrNodeCloseFollowing)) {
        node_queue.push(next_node);
      }
    }
    if (top_node == cut_node) {
      continue;
    }
    auto top_cnode = dyn_cast_ptr<CNode>(top_node);
    if (top_cnode == nullptr) {
      continue;
    }
    const auto &inputs = top_cnode->inputs();
    for (auto iter = inputs.begin() + 1; iter != inputs.end(); iter++) {
      const auto &next = *iter;
      MS_EXCEPTION_IF_NULL(next);
      if (next->seen_ == seen) {
        continue;
      }
      node_queue.push(next);
    }
  }

  (*close_following)[cut_node] = follow_set;
}

bool IsNeedInline(const CNodePtr &cnode) {
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  if (!context->IsKByKExecutorMode() || context->CellReuseLevel() != CellReuseLevel::kLazyInline) {
    return false;
  }
  const auto &inputs = cnode->inputs();
  AnfNodePtr fn = inputs[0];
  FuncGraphPtr child_graph = common::AnfAlgo::GetValueNodeFuncGraph(fn);
  if (child_graph != nullptr && child_graph->has_flag(FUNC_GRAPH_FLAG_CELL_REUSE)) {
    // call
    cnode->AddPrimalAttr(kAttrNeedInline, MakeValue(true));
    cnode->AddPrimalAttr(kAttrNotCut, MakeValue(true));
    return true;
  } else if (IsPrimitiveCNode(fn, prim::kPrimTupleGetItem)) {
    // partial call
    auto tuple_get_node = fn->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(tuple_get_node);
    auto get_from_node = tuple_get_node->input(kIndexOne);
    MS_EXCEPTION_IF_NULL(get_from_node);
    if (!get_from_node->isa<CNode>()) {
      return false;
    }
    auto get_from_code = get_from_node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(get_from_code);
    if (get_from_code->HasPrimalAttr(kAttrNeedInline)) {
      cnode->AddPrimalAttr(kAttrNeedInline, MakeValue(true));
      cnode->AddPrimalAttr(kAttrNotCut, MakeValue(true));
      return true;
    }
    return false;
  } else {
    return false;
  }
}
}  // namespace

GraphPartition::GraphPartition(const std::vector<PrimitivePtr> &cut_list, const std::string &backend_name)
    : cut_list_(cut_list), backend_name_(backend_name) {}

bool GraphPartition::IsCut(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  if (node->isa<CNode>()) {
    auto cnode = node->cast<CNodePtr>();
    auto &inputs = cnode->inputs();
    if (inputs.empty()) {
      MS_LOG(EXCEPTION) << "Inputs of apply node is empty";
    }
    if (IsNeedInline(cnode)) {
      return false;
    }
    AnfNodePtr fn = inputs[0];
    if (!IsValueNode<Primitive>(fn)) {
      // Call node not cut in pynative control for dynamic shape
      if (common::AnfAlgo::HasNodeAttr(kAttrJitCallNode, cnode)) {
        return false;
      }
      if (IsPrimitiveCNode(fn, prim::kPrimSwitch) && fn->cast<CNodePtr>()->HasPrimalAttr(kAttrNotCut)) {
        return false;
      }
      return true;
    }
    if (cnode->HasPrimalAttr(kAttrNotCut)) {
      return false;
    }
    auto node_prim = GetValueNode<PrimitivePtr>(fn);
    for (auto &prim : cut_list_) {
      MS_EXCEPTION_IF_NULL(prim);
      if (prim->name() == node_prim->name()) {
        if (prim->name() == prim::kPrimBpropCut->name()) {
          auto ms_context = MsContext::GetInstance();
          MS_EXCEPTION_IF_NULL(ms_context);
          ms_context->set_param<bool>(MS_CTX_ENABLE_PYNATIVE_HOOK, true);
        }
        if ((backend_name_ == kMsConvert || backend_name_ == kGeVm) && prim->name() == prim::kPrimMakeTuple->name()) {
          if (inputs.size() <= 1) {
            return false;
          }
          auto ret = IsSubGraph(inputs[1]);
          return ret;
        }
        return true;
      }
    }
  }
  return false;
}

namespace {
bool IsAnyTypeCut(const AnfNodePtr &node) {
  return common::AnfAlgo::CheckPrimitiveType(node, prim::kPrimPyExecute) && common::AnfAlgo::IsAnyTypeOutput(node);
}

void ProcessNodeToSegments(const std::string &cur_flag, const std::string &flag, std::vector<AnfNodePtr> *segment_nodes,
                           std::vector<GraphSegmentPtr> *segments,
                           std::map<AnfNodePtr, GraphSegmentPtr> *node_to_segment) {
  if (!flag.empty() && cur_flag != flag) {
    NodesToSegments(*segment_nodes, segments, node_to_segment);
    segment_nodes->clear();
  }
}

}  // namespace

std::vector<GraphSegmentPtr> GraphPartition::Partition(const FuncGraphPtr &graph, bool *multi_target) {
  MS_EXCEPTION_IF_NULL(graph);
  auto nodes = TopoSort(graph->get_return());
  MS_LOG(DEBUG) << "Split all nodes size:" << nodes.size();
  bool contain_multi_target = ContainMultiTarget(nodes);
  if (multi_target != nullptr) {
    *multi_target = contain_multi_target;
  }

  auto context_ptr = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context_ptr);
  auto enable_loop_sink = context_ptr->get_param<bool>(MS_CTX_ENABLE_LOOP_SINK);
  std::string default_target = context_ptr->get_param<std::string>(MS_CTX_DEVICE_TARGET);
  if (contain_multi_target || !enable_loop_sink) {
    if (context_ptr->get_param<bool>(MS_CTX_ENABLE_PARALLEL_SPLIT)) {
      auto other_target = GetOtherTarget(nodes);
      nodes = ParallelSort(graph, default_target, other_target);
    } else {
      nodes = SplitSort(graph, default_target);
    }
    if (context_ptr->IsKByKExecutorMode()) {
      // Keep the cutting position as far back as possible
      nodes = LazySort(nodes, {prim::kPrimPartial});
    }
    nodes = ReorderVirtualNode(nodes, prim::kPrimTupleGetItem);
    nodes = ReorderVirtualNode(nodes, prim::kPrimDepend);
  }

  CheckDiffTargetNum(nodes);
  std::vector<GraphSegmentPtr> segments;
  std::vector<AnfNodePtr> segment_nodes;
  std::map<AnfNodePtr, GraphSegmentPtr> node_to_segment;
  std::string last_target;
  std::string graph_group;
  mindspore::HashSet<AnfNodePtr> has_cut;
  mindspore::HashMap<AnfNodePtr, std::vector<AnfNodePtr>> close_following;
  for (auto &node : nodes) {
    MS_EXCEPTION_IF_NULL(node);
    if (has_cut.find(node) != has_cut.end()) {
      continue;
    }
    (void)has_cut.insert(node);
    ProcessCloseFollowing(graph, node, &close_following);
    if (IsCut(node)) {
      std::vector<AnfNodePtr> need_in_segement;
      for (auto &seg_node : segment_nodes) {
        auto iter = close_following.find(seg_node);
        if (iter == close_following.end()) {
          continue;
        }
        for (auto &succ : iter->second) {
          if (has_cut.find(succ) == has_cut.end()) {
            (void)has_cut.insert(succ);
            need_in_segement.push_back(succ);
          }
        }
      }
      for (auto &succ : need_in_segement) {
        MS_LOG(INFO) << "Find succ push to segment: " << succ->DebugString();
        segment_nodes.push_back(succ);
      }
      NodesToSegments(segment_nodes, &segments, &node_to_segment);
      segment_nodes.clear();
      segment_nodes.emplace_back(node);
      auto segment = std::make_shared<GraphSegment>(segment_nodes, true);
      segments.push_back(segment);
      segment_nodes.clear();
    } else if (node->isa<CNode>()) {
      std::string cur_target = GetCNodeTarget(node);
      std::string cur_graph_group = common::AnfAlgo::GetGraphSplitGroup(node);
      ProcessNodeToSegments(cur_target, last_target, &segment_nodes, &segments, &node_to_segment);
      ProcessNodeToSegments(cur_graph_group, graph_group, &segment_nodes, &segments, &node_to_segment);
      graph_group = cur_graph_group;
      last_target = cur_target;
      segment_nodes.emplace_back(node);
      if (IsAnyTypeCut(node)) {
        NodesToSegments(segment_nodes, &segments, &node_to_segment);
        segment_nodes.clear();
      }
    }
  }
  MS_LOG(DEBUG) << "Segment size:" << segments.size();
  if (contain_multi_target) {
    AddSegmentDependency(graph, node_to_segment);
    RemoveUselessDependency(&segments);
  }
  return segments;
}
}  // namespace compile
}  // namespace mindspore
