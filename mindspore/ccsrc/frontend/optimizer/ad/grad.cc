/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#include "frontend/optimizer/ad/grad.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include "frontend/optimizer/ad/dfunctor.h"
#include "frontend/optimizer/irpass.h"
#include "ir/func_graph_cloner.h"
#include "utils/ms_context.h"
#include "utils/symbolic.h"
#include "include/common/utils/parallel_context.h"

namespace mindspore {
namespace ad {
namespace {
FuncGraphPtr PartialEliminateOptPass(const pipeline::ResourcePtr &resource, const FuncGraphPtr &func_graph) {
  MS_EXCEPTION_IF_NULL(resource);

  opt::irpass::OptimizeIRPassLib irpass;
  opt::OptPassConfig partial_eliminate_opt_ = opt::OptPassConfig(
    {irpass.partial_eliminate_, irpass.switch_partial_eliminater_, irpass.switch_layer_partial_eliminater_});
  opt::OptPassGroupMap map({{"partial_eliminate_", partial_eliminate_opt_}});

  auto after_lift_opt = opt::Optimizer::MakeOptimizer("partial_eliminate", resource, map);

  FuncGraphPtr opt_fg = nullptr;
  ProfileExecute(MsProfile::GetProfile()->Step("partial_eliminate_before_grad"),
                 [&after_lift_opt, func_graph, &opt_fg]() { opt_fg = after_lift_opt->step(func_graph, true); });
  return opt_fg;
}

FuncGraphVector PartialEliminateMulti(const pipeline::ResourceBasePtr &resource, const FuncGraphVector &func_graphs) {
  auto new_res = std::dynamic_pointer_cast<pipeline::Resource>(resource);
  if (new_res == nullptr) {
    MS_LOG(INTERNAL_EXCEPTION) << "Parameter resources is not a pipeline::Resource";
  }
  FuncGraphVector opt_fgs;
  for (const auto &func_graph : func_graphs) {
    auto opt_fg = PartialEliminateOptPass(new_res, func_graph);
#ifdef ENABLE_DUMP_IR
    auto context = MsContext::GetInstance();
    MS_EXCEPTION_IF_NULL(context);
    if (context->CanDump(kIntroductory)) {
      DumpIR("after_opt_" + opt_fg->ToString() + ".ir", opt_fg);
    }
#endif
    opt_fgs.push_back(opt_fg);
  }
  return opt_fgs;
}

FuncGraphPtr LiftFv(const pipeline::ResourceBasePtr &resource, const FuncGraphPtr &func_graph) {
#ifdef ENABLE_DUMP_IR
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  bool enable_save_graphs = context->CanDump(kIntroductory);
  if (enable_save_graphs) {
    DumpIR("before_lift_" + func_graph->ToString() + ".ir", func_graph);
  }
#endif
  FuncGraphPtr new_fg = LiftingClone(func_graph);
#ifdef ENABLE_DUMP_IR
  if (enable_save_graphs) {
    DumpIR("after_lift_" + new_fg->ToString() + ".ir", new_fg);
  }
#endif
  auto new_res = std::dynamic_pointer_cast<pipeline::Resource>(resource);
  if (new_res == nullptr) {
    MS_LOG(INTERNAL_EXCEPTION) << "Parameter resources is not a pipeline::Resource";
  }
  auto opt_fg = PartialEliminateOptPass(new_res, new_fg);
#ifdef ENABLE_DUMP_IR
  if (enable_save_graphs) {
    DumpIR("after_opt_" + opt_fg->ToString() + ".ir", opt_fg);
  }
#endif
  return opt_fg;
}

FuncGraphVector LiftFvMulti(const pipeline::ResourceBasePtr &resource, const FuncGraphVector &func_graphs) {
#ifdef ENABLE_DUMP_IR
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  if (context->CanDump(kIntroductory)) {
    for (const auto &func_graph : func_graphs) {
      DumpIR("before_lift_" + func_graph->ToString() + ".ir", func_graph);
    }
  }
#endif
  bool has_used_fg = std::any_of(func_graphs.cbegin(), func_graphs.cend(), [](const FuncGraphPtr &func_graph) {
    return func_graph->func_graphs_used().size() != 0;
  });
  // All func_graphs being graded don't have used funcgraphs, no need to do lifting clone.
  if (!has_used_fg) {
    return func_graphs;
  }
  FuncGraphVector new_fgs = LiftingCloneMulti(func_graphs);
#ifdef ENABLE_DUMP_IR
  if (context->CanDump(kIntroductory)) {
    for (const auto &new_fg : new_fgs) {
      DumpIR("after_lift_" + new_fg->ToString() + ".ir", new_fg);
    }
  }
#endif
  return PartialEliminateMulti(resource, new_fgs);
}

bool ForwardInputsEqual(const AnfNodeWeakPtrList &first_inputs, const AnfNodeWeakPtrList &second_inputs) {
  if (first_inputs.size() != second_inputs.size()) {
    return false;
  }
  for (size_t i = 1; i < first_inputs.size(); ++i) {
    if (HasAbstractMonad(first_inputs[i].lock()) && HasAbstractMonad(second_inputs[i].lock())) {
      continue;
    }
    if (first_inputs[i].lock() != second_inputs[i].lock()) {
      return false;
    }
  }
  return true;
}

AnfNodePtr GetJUser(const FuncGraphManagerPtr &manager, const AnfNodePtr &j_node) {
  auto iter = manager->node_users().find(j_node);
  if (iter == manager->node_users().end()) {
    return nullptr;
  }
  auto users = iter->second;
  if (users.size() != 1) {
    MS_LOG(EXCEPTION) << "The size of J users should be 1, but got " << users.size();
  }
  return users.begin()->first;
}
}  // namespace

FuncGraphPtr GradOneFuncGraph(const FuncGraphPtr &func_graph, const opt::OptimizerPtr &optimizer, bool is_top,
                              BpropAutoMonadLevel level) {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto gradkv = func_graph->transforms().find("grad");
  if (gradkv != func_graph->transforms().end()) {
    return gradkv->second.func_graph();
  }
  const auto &resources = optimizer->resource();
  auto manager_ptr = resources->manager();
  MS_EXCEPTION_IF_NULL(manager_ptr);
  manager_ptr->AddFuncGraph(func_graph);
  auto multi_graph_sink = [&func_graph](const FuncGraphPtr &f) {
    if (MsContext::GetInstance()->get_param<bool>(MS_CTX_IS_MULTI_GRAPH_SINK)) {
      if (func_graph->has_flag(FUNC_GRAPH_FLAG_IGNORE_VALUE)) {
        f->set_flag(FUNC_GRAPH_FLAG_IGNORE_VALUE, true);
      }
    }
  };

  auto f = std::make_shared<DFunctor>(func_graph, resources, is_top);
  auto user_defined = f->KUserDefined(func_graph);
  if (user_defined != nullptr) {
    multi_graph_sink(user_defined);
    if (is_top) {
      DFunctor::Clear();
    }
    return user_defined;
  }
  f->Init(is_top);
  f->MapObject();
  f->MapMorphism();
  f->Finish();
  auto res = f->k_graph();
  res->set_attr(kAttrBpropAutoMonadLevel, MakeValue<int>(level));
  auto tape = f->tape();
  tape->set_flag(mindspore::kFuncGraphFlagBackPropEntry, true);
  if (is_top) {
    DFunctor::Clear();
  }

  multi_graph_sink(res);
  (void)func_graph->transforms().emplace("grad", FuncGraphTransform(res));
  return res;
}

FuncGraphPtr Grad(const FuncGraphPtr &func_graph, const opt::OptimizerPtr &optimizer, bool is_top,
                  BpropAutoMonadLevel level) {
  MS_EXCEPTION_IF_NULL(func_graph);
  auto gradkv = func_graph->transforms().find("grad");
  if (gradkv != func_graph->transforms().end()) {
    return gradkv->second.func_graph();
  }

  const auto &resources = optimizer->resource();
  auto manager_ptr = resources->manager();
  MS_EXCEPTION_IF_NULL(manager_ptr);
  manager_ptr->AddFuncGraph(func_graph);

  FuncGraphPtr grad_fg = func_graph;
  if (func_graph->func_graphs_used().size() != 0 && optimizer->is_first_order_j()) {
    lift_fv_before_grad = true;
    grad_fg = LiftFv(resources, func_graph);
  } else {
    lift_fv_before_grad = false;
  }
  return GradOneFuncGraph(grad_fg, optimizer, is_top, level);
}

FuncGraphVector GradMultiFuncGraph(const FuncGraphVector &func_graphs, const opt::OptimizerPtr &optimizer,
                                   bool is_top) {
  auto parallel_context = parallel::ParallelContext::GetInstance();
  MS_EXCEPTION_IF_NULL(parallel_context);
  auto parallel_mode = parallel_context->parallel_mode();
  const bool is_parallel_mode =
    parallel_mode == parallel::kSemiAutoParallel || parallel_mode == parallel::kAutoParallel;
  BpropAutoMonadLevel bprop_auto_monad_level = is_parallel_mode ? kLevelTop : kLevelWhole;
  FuncGraphVector grad_fgs;
  if (func_graphs.size() == 1) {
    auto grad_fg = Grad(func_graphs[0], optimizer, is_top, bprop_auto_monad_level);
    grad_fgs.push_back(grad_fg);
    return grad_fgs;
  }
  const auto &resources = optimizer->resource();
  auto manager_ptr = resources->manager();
  MS_EXCEPTION_IF_NULL(manager_ptr);
  for (const auto &func_graph : func_graphs) {
    manager_ptr->AddFuncGraph(func_graph);
  }
  FuncGraphVector before_grad_fgs;
  if (optimizer->is_first_order_j()) {
    lift_fv_before_grad = true;
    before_grad_fgs = LiftFvMulti(resources, func_graphs);
  } else {
    before_grad_fgs = func_graphs;
    lift_fv_before_grad = false;
  }
  for (const auto &func_graph : before_grad_fgs) {
    auto grad_fg = GradOneFuncGraph(func_graph, optimizer, is_top, bprop_auto_monad_level);
    grad_fgs.push_back(grad_fg);
  }
  return grad_fgs;
}

FuncGraphPtr Kprim(const ValueNodePtr &value_node, const pipeline::ResourceBasePtr &resources) {
  auto fg = g_k_prims.KPrimitive(nullptr, value_node, resources);
  if (fg == nullptr) {
    return nullptr;
  }
  return BasicClone(fg);
}

MetaFuncGraphPtr Kmeta(const PrimitivePtr &prim, const pipeline::ResourceBasePtr &) {
  MetaFuncGraphPtr fg = g_k_prims.KMetaFuncGraph(prim);
  return fg;
}

void CleanRes() { DFunctor::Clear(); }

bool MergeForward(const FuncGraphPtr &root, const opt::OptimizerPtr &opt) {
  auto manager = opt->manager();
  MS_EXCEPTION_IF_NULL(manager);
  std::unordered_map<FuncGraphPtr, std::vector<AnfNodePtr>> forward_fg_to_j_nodes;
  auto all_nodes = TopoSort(root->get_return(), SuccDeeperSimple, AlwaysInclude);
  for (const auto &node : all_nodes) {
    if (!IsPrimitiveCNode(node, prim::kPrimJ)) {
      continue;
    }
    auto cnode = node->cast<CNodePtr>();
    auto merge_forward = cnode->user_data<bool>("merge_forward");
    if (merge_forward == nullptr || !(*merge_forward)) {
      continue;
    }
    auto forward_fg = GetValueNode<FuncGraphPtr>(cnode->input(1));
    if (forward_fg == nullptr) {
      continue;
    }
    (void)forward_fg_to_j_nodes[forward_fg].emplace_back(node);
  }
  bool change = false;
  for (const auto &iter : forward_fg_to_j_nodes) {
    auto &j_nodes = iter.second;
    MS_LOG(DEBUG) << "J nodes size is " << j_nodes.size();
    if (j_nodes.size() <= 1) {
      continue;
    }
    auto first_j_user = GetJUser(manager, j_nodes[0]);
    if (first_j_user == nullptr) {
      continue;
    }
    const auto &first_forward_inputs = first_j_user->cast<CNodePtr>()->weak_inputs();
    for (size_t i = 1; i < j_nodes.size(); ++i) {
      auto j_user = GetJUser(manager, j_nodes[i]);
      const auto &forward_inputs = j_user->cast<CNodePtr>()->weak_inputs();
      if (!ForwardInputsEqual(first_forward_inputs, forward_inputs)) {
        continue;
      }
      manager->Replace(j_user, first_j_user);
      MS_LOG(DEBUG) << "Replace J user " << j_user->DebugString() << " with the first J user "
                    << first_j_user->DebugString();
      change = true;
    }
  }
  return change;
}
}  // namespace ad
}  // namespace mindspore
