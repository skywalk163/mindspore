/**
 * Copyright 2020-2024 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_IRPASS_INLINE_H_
#define MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_IRPASS_INLINE_H_

#include <vector>
#include <utility>
#include <algorithm>

#include "utils/hash_map.h"
#include "mindspore/core/ops/framework_ops.h"
#include "frontend/optimizer/irpass.h"
#include "include/common/utils/parallel_context.h"
#include "frontend/optimizer/optimizer.h"
#include "frontend/optimizer/anf_visitor.h"
#include "ir/func_graph.h"
#include "ir/func_graph_cloner.h"
#include "ir/tensor.h"
#include "frontend/operator/ops.h"
#include "abstract/abstract_value.h"
#include "include/common/utils/utils.h"
#include "pipeline/jit/ps/pipeline.h"

namespace mindspore {
namespace opt {
namespace irpass {
class ReplaceApplicator : public AnfVisitor {
 public:
  AnfNodePtr operator()(const OptimizerPtr &, const AnfNodePtr &node) override {
    if (!IsValueNode<FuncGraph>(node)) {
      return nullptr;
    }
    auto fg = GetValueNode<FuncGraphPtr>(node);
    if (NoInline(fg)) {
      return nullptr;
    }

    auto out = fg->output();
    MS_EXCEPTION_IF_NULL(out);
    if (!out->isa<CNode>()) {
      return nullptr;
    }

    auto &inputs = out->cast<CNodePtr>()->inputs();
    auto params = fg->parameters();

    // Exclude first elements of inputs which is fn.
    auto input_size = inputs.size();
    auto param_size = params.size();
    if ((input_size == 1 && param_size == 0) || (input_size > 1 && (input_size - 1) == param_size &&
                                                 std::equal(inputs.begin() + 1, inputs.end(), params.begin()))) {
      auto inner = inputs[0];
      if (IsValueNode<Primitive>(inner)) {
        return inner;
      }
      if (IsValueNode<FuncGraph>(inner) && GetValueNode<FuncGraphPtr>(inner)->parent() == nullptr) {
        const auto &inner_fg = GetValueNode<FuncGraphPtr>(inner);
        MS_EXCEPTION_IF_NULL(inner_fg);
        bool is_recursive = (inner_fg->has_flag(FUNC_GRAPH_FLAG_NO_RECURSIVE) ? false : inner_fg->recursive());
        if (is_recursive) {
          return nullptr;
        }
        return inner;
      }
    }

    return nullptr;
  }

  bool NoInline(const FuncGraphPtr &fg) const {
    if (fg == nullptr || fg->has_flag(FUNC_GRAPH_FLAG_NO_INLINE) || fg->has_flag(FUNC_GRAPH_FLAG_DEFER_INLINE) ||
        fg->stub() || *(fg->indirect())) {
      return true;
    }
    // Defer inlining in the case of pipeline.
    auto stage_num = parallel::ParallelContext::GetInstance()->pipeline_stage_split_num();
    if (fg->stage() != -1 && stage_num > 1) {
      return true;
    }
    // Defer inlining for:
    // 1. The func_graph which is set recomputed.
    // 2. The k graph whose primal is set non-recomputed when enable graph reuse.
    auto context = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(context);
    const auto cell_reuse = context->CellReuseLevel() != CellReuseLevel::kNoCellReuse;
    return fg->has_flag(FUNC_GRAPH_OUTPUT_NO_RECOMPUTE) ||
           (cell_reuse &&
            (fg->has_flag(FUNC_GRAPH_NOT_RECOMPUTE_K_GRAPH) || fg->has_flag(FUNC_GRAPH_RECOMPUTE_K_GRAPH)));
  }
};

class InlinerBase;
using CriterionFuncType = std::function<bool(InlinerBase *, const FuncGraphPtr &, const AnfNodePtr &)>;

bool IsUniqueUse(InlinerBase *, const FuncGraphPtr &fg, const AnfNodePtr &);

bool IsTrivial(InlinerBase *, const FuncGraphPtr &fg, const AnfNodePtr &);
bool IsInside(InlinerBase *, const FuncGraphPtr &, const AnfNodePtr &node);
bool IsCore(InlinerBase *, const FuncGraphPtr &fg, const AnfNodePtr &);
bool IsDirectParentCall(InlinerBase *, const FuncGraphPtr &fg, const AnfNodePtr &node);
bool IsNotRecursive(InlinerBase *inliner, const FuncGraphPtr &fg, const AnfNodePtr &);
bool IsForceInline(InlinerBase *, const FuncGraphPtr &fg, const AnfNodePtr &) {
  return fg->has_flag(FUNC_GRAPH_FLAG_FORCE_INLINE);
}

// {G, Xs}
class InlinerBase : public AnfVisitor {
 public:
  explicit InlinerBase(const std::vector<std::vector<CriterionFuncType>> &criterions, bool use_move = true)
      : use_move_(use_move), criterions_(criterions) {}
  ~InlinerBase() override = default;
  AnfNodePtr operator()(const OptimizerPtr &optimizer, const AnfNodePtr &node) override {
    auto cnode = dyn_cast<CNode>(node);
    if (cnode == nullptr || cnode->size() < 1) {
      return nullptr;
    }

    // Check if no recursive flag was set in top graph.
    CheckNoRecursive(optimizer);

    auto &inputs = cnode->inputs();
    // G
    auto fg = GetValueNode<FuncGraphPtr>(inputs[0]);
    if (!CheckFlag(fg)) {
      return nullptr;
    }

    Reset();

    // 'criterions_': {criterion_group_1:{criterion1, criterion2, ...}, criterion_group_2:{...}, ...}
    // All the criterions of 'criterion group' are true would set 'criterion group' as 'true'. As [AND].
    // Anyone of 'criterion group' in 'criterions_' is 'true' would be matched. As [OR].
    bool is_match = ApplyCriterions(node, fg);
    if (!is_match) {
      return nullptr;
    }

    std::vector<AnfNodePtr> args;
    (void)std::copy(inputs.begin() + 1, inputs.end(), std::back_inserter(args));
    // Compare size to avoid the case that the function has default value after grad.
    // for which after renormalize, the function default value will be an input
    if (fg->parameters().size() != args.size()) {
      return nullptr;
    }

    if (IsForceInline(this, fg, node)) {
      if (IsUniqueUse(nullptr, fg, nullptr)) {
        return InlineMove(node, fg, args, inputs);
      }
      return InlineClone(fg, node->func_graph(), args, inputs[0]->scope(), cnode->debug_info());
    }

    if (IsUniqueUse(nullptr, fg, nullptr)) {
      // For the single used fg, including non-after and after not matched above,
      // we move the whole fg nodes.
      auto res_node = InlineForUniqueUse(node, fg, args, inputs);
      if (res_node != nullptr) {
        return res_node;
      }
    } else {
      // We don't expand the middle multiple used after block, except the last one.
      if (GraphHasBranch(fg)) {
        return nullptr;
      }
      // Check if parameters' changed for the first met branch calling.
      if (fg->has_flag(FUNC_GRAPH_FLAG_AFTER_BLOCK)) {
        auto param_simplified_caller = SimplifyAfterParameter(fg, node, args);
        if (param_simplified_caller != nullptr) {
          return param_simplified_caller;
        }
      }
    }
    // Or, just make a clone for not single used fg.
    auto res = InlineClone(fg, node->func_graph(), args, inputs[0]->scope(), cnode->debug_info());
    return res;
  }

  bool CheckFlag(const FuncGraphPtr &fg) const {
    if (fg == nullptr || fg->has_flag(FUNC_GRAPH_FLAG_NO_INLINE) || fg->has_flag(FUNC_GRAPH_FLAG_DEFER_INLINE) ||
        fg->stub()) {
      return false;
    }
    // Defer inlining in the case of pipeline.
    auto stage_num = parallel::ParallelContext::GetInstance()->pipeline_stage_split_num();
    if (fg->stage() != -1 && stage_num > 1) {
      return false;
    }
    // Defer inlining for:
    // 1. The func_graph which is set recomputed.
    // 2. The k graph whose primal is set non-recomputed when enable graph reuse.
    auto context = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(context);
    const auto cell_reuse = context->CellReuseLevel() != CellReuseLevel::kNoCellReuse;
    if (fg->has_flag(FUNC_GRAPH_OUTPUT_NO_RECOMPUTE) ||
        (cell_reuse &&
         (fg->has_flag(FUNC_GRAPH_NOT_RECOMPUTE_K_GRAPH) || fg->has_flag(FUNC_GRAPH_RECOMPUTE_K_GRAPH)))) {
      return false;
    }
    return true;
  }

  bool IsRecursive(const FuncGraphPtr &fg) {
    // The user guarantees that fg has no recursive.
    if (no_recursive_) {
      return false;
    }

    if (!is_checked_) {
      is_checked_ = true;
      is_recursive_ = (fg->has_flag(FUNC_GRAPH_FLAG_NO_RECURSIVE) ? false : fg->recursive());
    }
    return is_recursive_;
  }

  bool no_recursive() const { return no_recursive_; }

 private:
  AnfNodePtr InlineMove(const AnfNodePtr &node, const FuncGraphPtr &fg, const std::vector<AnfNodePtr> &args,
                        const std::vector<AnfNodePtr> &inputs) const {
    auto mng = fg->manager();
    MS_EXCEPTION_IF_NULL(mng);
    ReplaceParams(mng, args, fg);
    auto out_node = fg->output();
    const auto update_debug_info = MsContext::GetInstance()->get_param<int>(MS_CTX_DEBUG_LEVEL) == kLevelDebug ||
                                   MsContext::GetInstance()->GetSaveGraphsLevel() > 0;
    mng->MoveAllCNodeDropGraph(fg, node->func_graph(), node, inputs[0]->scope(), update_debug_info);
    return out_node;
  }

  AnfNodePtr InlineForUniqueUse(const AnfNodePtr &node, const FuncGraphPtr &fg, const std::vector<AnfNodePtr> &args,
                                const std::vector<AnfNodePtr> &inputs) const {
    if (use_move_) {
      return InlineMove(node, fg, args, inputs);
    }

    // The other branch calling the last after block.
    if (fg->has_flag(FUNC_GRAPH_FLAG_AFTER_BLOCK)) {
      // Check if parameters' changed.
      auto param_simplified_caller = SimplifyAfterParameter(fg, node, args);
      if (param_simplified_caller != nullptr) {
        return param_simplified_caller;
      }
    }
    return nullptr;
  }

  bool ApplyCriterions(const AnfNodePtr &node, const FuncGraphPtr &fg) {
    bool is_match = false;
    for (auto &criterions : criterions_) {  // Each 'criterion group' in criterions_.
      is_match = true;
      for (auto &criterion : criterions) {  // Each criterion in 'criterion group'.
        if (!criterion(this, fg, node)) {
          is_match = false;
          break;
        }
      }
      if (is_match) {
        break;
      }
    }
    return is_match;
  }

  void ReplaceParams(const FuncGraphManagerPtr &mng, const std::vector<AnfNodePtr> &new_params,
                     const FuncGraphPtr &fg) const {
    auto params = fg->parameters();
    auto old_size = params.size();
    constexpr auto print_deep = 10;
    if (old_size != new_params.size()) {
      MS_LOG(INTERNAL_EXCEPTION) << "Parameter size not match." << old_size << " new " << new_params.size()
                                 << fg->output()->DebugString(print_deep);
    }
    for (size_t i = 0; i < old_size; i++) {
      (void)mng->Replace(params[i], new_params[i]);
    }
  }

  void CheckNoRecursive(const OptimizerPtr &optimizer) {
    // Check if no recursive flag was set in top graph.
    const auto &resource = std::dynamic_pointer_cast<pipeline::Resource>(optimizer->resource());
    if (resource == nullptr) {
      return;
    }
    const auto &top_graph = resource->func_graph();
    if (top_graph == nullptr) {
      return;
    }
    if (top_graph->has_flag(FUNC_GRAPH_FLAG_NO_RECURSIVE)) {
      no_recursive_ = true;
    }
  }

  void Reset() {
    is_checked_ = false;
    is_recursive_ = false;
  }

  // For after block which contains branch call, delete the parameters which is not used.
  // In most cases, it may be a `Module` or other constant input.
  AnfNodePtr SimplifyAfterParameter(const FuncGraphPtr &fg, const AnfNodePtr &node,
                                    const std::vector<AnfNodePtr> &args) const {
    auto &fg_params = fg->parameters();
    std::vector<int64_t> used_param_index;
    auto mng = fg->manager();
    MS_EXCEPTION_IF_NULL(mng);
    bool should_simplify = false;
    for (size_t i = 0; i < fg_params.size(); i++) {
      if (mng->node_users()[fg_params[i]].size() != 0) {
        (void)used_param_index.emplace_back(i);
      } else {
        MS_LOG(DEBUG) << "Not used parameter " << fg_params[i]->DebugString() << " for calling " << fg->ToString();
        should_simplify = true;
      }
    }
    if (!should_simplify) {
      return nullptr;
    }
    MS_LOG(DEBUG) << "Parameter not used found for graph :" << fg->ToString();
    // Clone a new graph and ignore the not used parameters
    auto new_fg = TransformableClone(fg);
    auto &new_fg_params = new_fg->parameters();
    std::vector<AnfNodePtr> new_params;
    std::transform(used_param_index.begin(), used_param_index.end(), std::back_inserter(new_params),
                   [&new_fg_params](size_t i) { return new_fg_params[i]; });
    new_fg->set_parameters(new_params);

    std::vector<AnfNodePtr> node_inputs;
    node_inputs.push_back(NewValueNode(new_fg));
    std::transform(used_param_index.begin(), used_param_index.end(), std::back_inserter(node_inputs),
                   [&args](size_t i) { return args[i]; });
    auto ret_node = node->func_graph()->NewCNode(node_inputs);
    ret_node->set_abstract(node->abstract());
    return ret_node;
  }

  bool CheckSwitchInputs(const std::vector<AnfNodePtr> &sw_inputs) const {
    // When branch has dead node or poly node, do not perform inline.
    if (IsDeadNode(sw_inputs[kSwitchTrueBranchIndex]) || IsPolyNode(sw_inputs[kSwitchTrueBranchIndex]) ||
        IsDeadNode(sw_inputs[kSwitchFalseBranchIndex]) || IsPolyNode(sw_inputs[kSwitchFalseBranchIndex])) {
      return true;
    }
    return !sw_inputs[1]->isa<ValueNode>() || IsValueNode<tensor::Tensor>(sw_inputs[1]);
  }

  // This is a try-best algorithm to find a graph which may generate branch call.
  // It does not handle high-order function call. For high-orderer call branch, it still may be inlined.
  bool GraphHasBranch(const FuncGraphPtr &fg) {
    if (graph_branch_cache_.find(fg) != graph_branch_cache_.end()) {
      return graph_branch_cache_[fg];
    }
    bool has_branch = false;
    auto nodes = fg->nodes();
    for (auto &item : nodes) {
      if (IsPrimitiveCNode(item, prim::kPrimSwitch)) {
        auto sw_inputs = item->cast<CNodePtr>()->inputs();
        if (sw_inputs.size() != kIndex4) {
          MS_LOG(EXCEPTION) << "Switch inputs should be 4";
        }
        if (CheckSwitchInputs(sw_inputs)) {
          has_branch = true;
          break;
        }
      } else if (IsCNodeGraph(item)) {
        auto cinputs = item->cast<CNodePtr>()->inputs();
        if (cinputs.size() < 1) {
          MS_LOG(EXCEPTION) << "Graph call inputs should be greater than 1";
        }
        FuncGraphPtr call_fg = GetValueNode<FuncGraphPtr>(cinputs[0]);
        bool call_fg_has_branch = GraphHasBranch(call_fg);
        if (call_fg_has_branch) {
          has_branch = true;
          break;
        }
      } else if (IsPrimitiveCNode(item, prim::kPrimPartial)) {
        auto cinputs = item->cast<CNodePtr>()->inputs();
        if (cinputs.size() < kIndex2) {
          MS_LOG(EXCEPTION) << "Partial call inputs should be greater than 2";
        }
        FuncGraphPtr call_fg = GetValueNode<FuncGraphPtr>(cinputs[1]);
        if (call_fg == nullptr) {
          continue;
        }
        bool call_fg_has_branch = GraphHasBranch(call_fg);
        if (call_fg_has_branch) {
          has_branch = true;
          break;
        }
      }
    }
    graph_branch_cache_[fg] = has_branch;
    return has_branch;
  }

  bool is_checked_{false};
  bool is_recursive_{false};
  // If the user guarantee that fg has no recursive.
  bool no_recursive_{false};
  bool use_move_;
  std::vector<std::vector<CriterionFuncType>> criterions_;
  mindspore::HashMap<FuncGraphPtr, bool> graph_branch_cache_;
};

bool IsUniqueUse(InlinerBase *, const FuncGraphPtr &fg, const AnfNodePtr &) {
  const auto &users = fg->func_graph_cnodes_index();
  int64_t n_use = std::accumulate(
    users.begin(), users.end(), 0,
    [](int64_t sum, const std::pair<const CNodeIndexPairPtr, int64_t> &item) { return sum + item.second; });
  return n_use == 1;
}

bool IsTrivial(InlinerBase *, const FuncGraphPtr &fg, const AnfNodePtr &) {
  auto n_cnode = fg->nodes().size() - fg->parameters().size();
  // There is at least one CNode(return, other_node).
  constexpr size_t least_size = 2;
  return n_cnode <= least_size;
}

bool IsInside(InlinerBase *, const FuncGraphPtr &, const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node->func_graph());
  return node->func_graph()->has_flag("inline_inside");
}

bool IsCore(InlinerBase *, const FuncGraphPtr &fg, const AnfNodePtr &) { return fg->has_flag("core"); }

bool IsDirectParentCall(InlinerBase *inliner, const FuncGraphPtr &fg, const AnfNodePtr &node) {
  bool is_recursive = (inliner->no_recursive() ? false : fg->recursive());
  if (fg->parent() != nullptr && is_recursive) {
    if (fg->parent() == node->func_graph() && IsUniqueUse(nullptr, fg, nullptr)) {
      return true;
    }
  }
  return false;
}

bool IsNotRecursive(InlinerBase *inliner, const FuncGraphPtr &fg, const AnfNodePtr &) {
  return !inliner->IsRecursive(fg);
}

class Inliner : public InlinerBase {
 public:
  explicit Inliner(bool use_move = true)
      : InlinerBase(
          // Supports AND conditions in one criterion, Ex. {IsUniqueUse, IsNotRecursive}.
          {
            {IsTrivial},
            {IsInside},
            {IsCore},
            {IsNotRecursive},
            {IsDirectParentCall},
          },
          use_move) {}

  ~Inliner() override = default;
};

class DirectInliner : public InlinerBase {
 public:
  explicit DirectInliner(bool use_move = true)
      : InlinerBase(
          // Supports AND conditions in one criterion, Ex. {IsUniqueUse, IsNotRecursive}.
          {
            {IsForceInline},
            {IsDirectParentCall},
          },
          use_move) {}
  ~DirectInliner() override = default;
};
}  // namespace irpass
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_FRONTEND_OPTIMIZER_IRPASS_INLINE_H_
