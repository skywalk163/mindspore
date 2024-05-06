/**
 * Copyright 2019-2024 Huawei Technologies Co., Ltd
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

#include "pipeline/jit/ps/pass.h"

#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include "mindspore/core/ops/other_ops.h"
#include "mindspore/core/ops/framework_ops.h"
#include "utils/hash_map.h"
#include "ir/func_graph_cloner.h"
#include "pipeline/jit/ps/parse/parse_base.h"
#include "pipeline/jit/ps/resource.h"
#include "pipeline/jit/ps/validator.h"
#include "pipeline/jit/ps/remove_value_node_dup.h"
#include "frontend/optimizer/opt.h"
#include "frontend/optimizer/optimizer.h"
#include "frontend/optimizer/cse_pass.h"
#include "frontend/optimizer/fallback_rewriter.h"
#include "frontend/optimizer/irpass.h"
#include "frontend/optimizer/graph_transform.h"
#include "frontend/optimizer/auto_monad_eliminate.h"
#include "include/common/fallback.h"
#include "include/common/utils/parallel_context.h"
#include "frontend/parallel/dynamic_shape/dynamic_shape.h"
#include "frontend/parallel/step_parallel.h"
#include "frontend/parallel/step_auto_parallel.h"
#include "frontend/parallel/graph_util/pipeline_split_utils.h"
#include "frontend/parallel/pipeline_transformer/pipeline_scheduler.h"
#include "frontend/parallel/pipeline_transformer/pipeline_interleave.h"
#include "frontend/parallel/pipeline_transformer/gpipe_interleave_scheduler.h"
#include "frontend/parallel/pass/merge_comm.h"
#include "frontend/parallel/cache_embedding/cache_embedding.h"
#include "frontend/parallel/cache_embedding/ps_embedding_cache_inserter.h"
#include "frontend/parallel/allreduce_fusion/step_allreduce_fusion.h"
#include "frontend/parallel/pynative_shard/pynative_shard.h"
#include "frontend/parallel/pass/label_micro_interleaved_index.h"
#include "frontend/parallel/pass/label_fine_grained_interleaved_index.h"
#include "frontend/parallel/pass/reorder_send_recv_between_fp_bp.h"
#include "frontend/parallel/pass/micro_interleaved_order_control.h"
#include "frontend/parallel/pass/full_micro_interleaved_order_control.h"
#include "frontend/parallel/pass/assign_add_opt.h"
#include "frontend/parallel/pass/float32_redistribution.h"
#include "frontend/parallel/pass/merge_cast_opt.h"
#include "frontend/parallel/pass/remove_cast_before_assign_add.h"
#include "frontend/parallel/pass/comp_comm_scheduling.h"
#include "frontend/parallel/pass/overlap_opt_shard_in_pipeline.h"
#include "frontend/parallel/pass/slice_activation_in_cell_share_recompute.h"
#include "frontend/parallel/pass/handle_group_info.h"
#include "frontend/parallel/pass/overlap_recompute_and_grad_model_parallel.h"
#include "frontend/parallel/pass/overlap_gradmatmul_and_gradallreduce.h"
#include "frontend/parallel/pass/begin_end_overlap_inline.h"
#include "frontend/parallel/pass/split_matmul_comm_elementwise_fp.h"
#include "frontend/parallel/pass/split_layernorm_comm_fp.h"
#include "frontend/parallel/pipeline_transformer/pipeline_transformer.h"
#include "frontend/parallel/pass/overlap_grad_comm.h"
#include "frontend/optimizer/recompute.h"
#include "frontend/optimizer/irpass/recompute.h"
#include "frontend/optimizer/slice_activation_in_recompute.h"
#include "frontend/optimizer/grouped_pairwise_exchange_alltoall.h"
#include "frontend/optimizer/comm_op_attrs.h"
#include "frontend/optimizer/process_send_recv_for_ge.h"
#include "frontend/optimizer/environ_conversion.h"
#include "frontend/optimizer/comm_op_reuse_tag.h"
#include "frontend/optimizer/py_interpret_to_execute.h"
#include "utils/log_adapter.h"
#include "utils/compile_config.h"
#include "pipeline/jit/ps/pipeline_split.h"
#include "pipeline/pynative/pynative_execute.h"
#include "pipeline/jit/ps/static_analysis/auto_monad.h"
#include "frontend/optimizer/irpass/branch_culling.h"
#include "frontend/optimizer/irpass/meta_fg_eliminate.h"
#include "frontend/optimizer/irpass/gradient_eliminate.h"
#include "frontend/optimizer/irpass/shard_eliminate.h"
#include "frontend/optimizer/irpass/taylor_eliminate.h"
#include "frontend/optimizer/irpass/parameter_eliminate.h"
#include "frontend/optimizer/irpass/updatestate_eliminate.h"
#include "frontend/optimizer/irpass/expand_dump_flag.h"
#include "frontend/optimizer/irpass/symbol_engine_optimizer.h"
#include "frontend/optimizer/irpass/add_forward_monad_depend.h"
#if defined(__linux__) && defined(WITH_BACKEND)
#include "include/backend/distributed/ps/util.h"
#include "include/backend/distributed/ps/ps_context.h"
#endif

namespace mindspore {
namespace pipeline {
using OptPassGroupMap = opt::OptPassGroupMap;
using Optimizer = opt::Optimizer;
using CompileGraphs = compile::CompileGraphs;
using abstract::AnalysisResult;
using mindspore::abstract::AnalysisContextPtr;
using mindspore::validator::Validate;
void UpdateArgsSpec(const FuncGraphPtr &func_graph, const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(resource);
  abstract::AbstractBasePtrList args_abs;
  const auto &parameters = func_graph->parameters();
  args_abs.reserve(parameters.size());
  (void)std::transform(parameters.begin(), parameters.end(), std::back_inserter(args_abs),
                       [](const AnfNodePtr &p) { return p->abstract(); });
  resource->set_args_abs(args_abs);
}

bool PyInterpretToExecutePass(const ResourcePtr &resource) {
  const auto allow_fallback_runtime = (fallback::GetJitSyntaxLevel() == kLax);
  if (!allow_fallback_runtime) {
    return true;
  }
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  (void)opt::PyInterpretToExecute(resource);
  UpdateArgsSpec(func_graph, resource);
  return true;
}

bool RewriterBeforeOptAPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  (void)opt::RewriterBeforeOptA(func_graph, resource->manager());
  UpdateArgsSpec(func_graph, resource);
  return true;
}

bool TransformTopGraphPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  if (resource->func_graph() == nullptr) {
    MS_LOG(INTERNAL_EXCEPTION) << "Transform top graph error.";
  }
  FuncGraphPtr func_graph = resource->func_graph();
  if (opt::FuncGraphHasSequenceInput(func_graph)) {
    opt::GraphSequenceParamTransform graph_trans;
    func_graph = graph_trans(func_graph, resource->manager());
    resource->set_func_graph(func_graph);
    AbstractBasePtrList abs_spec_list;
    auto &params = func_graph->parameters();
    (void)std::transform(params.begin(), params.end(), std::back_inserter(abs_spec_list),
                         [](const AnfNodePtr &node) { return node->abstract(); });
    resource->set_args_abs(abs_spec_list);
  }
  return true;
}

bool RewriterAfterOptAPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  (void)opt::RewriterAfterOptA(func_graph, resource);
  UpdateArgsSpec(func_graph, resource);
  return true;
}

bool ConvertAfterRewriterPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  (void)opt::ConvertAfterRewriter(func_graph, resource);
  UpdateArgsSpec(func_graph, resource);
  return true;
}

bool OrderPyExecuteAfterRewriterPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  (void)opt::OrderPyExecuteAfterRewriter(func_graph, resource);
  UpdateArgsSpec(func_graph, resource);
  return true;
}

FuncGraphPtr PrimBpOptPassStep1(const opt::irpass::OptimizeIRPassLib &irpass, const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  MS_EXCEPTION_IF_NULL(resource->func_graph());
  opt::OptPassConfig pynative_eliminate = opt::OptPassConfig({
    irpass.pynative_eliminate_,
  });

  opt::OptPassConfig switch_simplify = opt::OptPassConfig({
    irpass.switch_simplify_,
  });

  opt::OptPassConfig inline_opt = opt::OptPassConfig({
    irpass.inline_,
  });

  OptPassGroupMap map(
    {{"ad_eliminate", pynative_eliminate}, {"ad_inline", inline_opt}, {"ad_switch_simplify", switch_simplify}});

  auto prim_bprop_opt_step_1 = opt::Optimizer::MakeOptimizer("prim_bprop_opt_step_1", resource, map);
  FuncGraphPtr func_graph = resource->func_graph();
  ProfileExecute(MsProfile::GetProfile()->Step("prim_bprop_opt_step_1"), [&prim_bprop_opt_step_1, &func_graph]() {
    func_graph = prim_bprop_opt_step_1->step(func_graph, true);
  });
  return func_graph;
}

FuncGraphPtr PrimBpOptPassStep2(const opt::irpass::OptimizeIRPassLib &irpass, const ResourcePtr &resource,
                                const std::vector<bool> &need_grad_flags) {
  MS_EXCEPTION_IF_NULL(resource);
  MS_EXCEPTION_IF_NULL(resource->func_graph());
  OptPassGroupMap map;

  opt::OptPassConfig special_op_simplify = opt::OptPassConfig({
    irpass.switch_simplify_,
    irpass.reduce_eliminate_,
    irpass.tile_eliminate_,
    irpass.arithmetic_simplify_,
  });

  opt::OptPassConfig inline_opt = opt::OptPassConfig({
    irpass.inline_,
  });

  auto re_auto_monadwrapper = [](const FuncGraphPtr &root, const opt::OptimizerPtr &) -> bool {
    return ReAutoMonad(root);
  };

  map.push_back({"ad_renormalize", opt::OptPassConfig::Renormalize()});
  map.push_back({"ad_inline", inline_opt});
  map.push_back({"ad_special_op_simplify", special_op_simplify});
  map.push_back({"auto_monad_grad", opt::OptPassConfig(re_auto_monadwrapper)});
  if (!need_grad_flags.empty()) {
    // If func graph has not need_grad_flag_of_inputs attr, this graph has no need do this pass.
    opt::OptPassConfig pynative_no_grad_eliminate = opt::OptPassConfig({
      irpass.pynative_no_grad_eliminate_,
    });

    map.push_back({"pynative_no_grad_eliminate", pynative_no_grad_eliminate});
  }

  auto prim_bprop_opt_step_2 = opt::Optimizer::MakeOptimizer("prim_bprop_opt_step_2", resource, map);
  FuncGraphPtr func_graph = resource->func_graph();
  ProfileExecute(MsProfile::GetProfile()->Step("prim_bprop_opt_step_2"), [&prim_bprop_opt_step_2, &func_graph]() {
    func_graph = prim_bprop_opt_step_2->step(func_graph, true);
  });
  return func_graph;
}

FuncGraphPtr JitBpropGraphPass(const ResourcePtr &resource, bool need_renormalize) {
  opt::irpass::OptimizeIRPassLib irpass;
  opt::OptPassConfig grad_graph_opt = opt::OptPassConfig({
    irpass.inline_,
    irpass.list_to_tuple_eliminator_,
    irpass.tuple_to_list_eliminator_,
    irpass.tuple_list_get_set_item_eliminator_,
    irpass.tuple_list_get_item_eliminator_,
    irpass.tuple_list_set_item_eliminator_,
    irpass.depend_value_elim_,
    irpass.reshape_eliminate_,
    irpass.switch_simplify_,
    irpass.addn_zero_filter_,
    irpass.ad_related_special_op_eliminate_,
  });
  opt::OptPassConfig fill_zeros_like = opt::OptPassConfig{irpass.zero_like_fill_zero_};
  OptPassGroupMap map({
    {"grad_graph_opt", grad_graph_opt},
    {"zeros_like", fill_zeros_like},
  });
  if (need_renormalize) {
    (void)map.emplace_back(std::make_pair("renormalize", opt::OptPassConfig::Renormalize()));
    opt::OptPassConfig real_op_eliminate = opt::OptPassConfig{irpass.real_op_eliminate_};
    (void)map.emplace_back(std::make_pair("real_op_eliminate", real_op_eliminate));
  }
  MS_EXCEPTION_IF_NULL(resource);
  auto func_graph = resource->func_graph();
  auto graph_opt = opt::Optimizer::MakeOptimizer("jit_bprop_graph_opt", resource, map);
  return graph_opt->step(func_graph, false);
}

FuncGraphPtr FinalBpropGraphPass(const ResourcePtr &resource, bool has_control_flow) {
  MS_EXCEPTION_IF_NULL(resource);
  auto func_graph = resource->func_graph();

  opt::irpass::OptimizeIRPassLib irpass;
  OptPassGroupMap map;
  opt::OptPassConfig inline_opt = opt::OptPassConfig({
    irpass.inline_,
  });
  map.emplace_back("ad_inline", inline_opt);

  opt::OptPassConfig grad_graph_opt = opt::OptPassConfig({
    irpass.tuple_list_get_item_eliminator_,
    irpass.zero_like_fill_zero_,
  });
  (void)map.emplace_back("grad_graph_opt", grad_graph_opt);

  if (has_control_flow) {
    opt::OptPassConfig env_eliminate = opt::OptPassConfig({
      irpass.environ_get_eliminate_,
      irpass.environ_get_add_eliminate_,
      irpass.environ_get_set_eliminate_,
      irpass.environ_get_depend_swap_,
      irpass.environ_add_const_eliminate_,
    });
    (void)map.emplace_back(std::make_pair("env_eliminate", env_eliminate));
  }
  auto graph_opt = opt::Optimizer::MakeOptimizer("final_bprop_graph_opt", resource, map);
  return graph_opt->step(func_graph, false);
}

namespace {
bool ReAutoMonadWrapper(const FuncGraphPtr &root, const opt::OptimizerPtr &) { return ReAutoMonad(root); }

bool parallel_mode() {
#if defined(__linux__) && defined(WITH_BACKEND)
  if (ps::PSContext::instance()->is_server() || ps::PSContext::instance()->is_scheduler()) {
    return false;
  }
#endif
  std::string parallel_mode = parallel::ParallelContext::GetInstance()->parallel_mode();
  return (parallel_mode == parallel::kAutoParallel) || (parallel_mode == parallel::kSemiAutoParallel);
}

void AddParallelRenormalize(OptPassGroupMap *map_a) {
  if (parallel_mode()) {
    auto parallel_end_opt =
      find_if(map_a->begin(), map_a->end(), [](auto opt_pair) { return opt_pair.first == "meta_fg_expand"; });
    if (parallel_end_opt != map_a->end()) {
      opt::irpass::OptimizeIRPassLib irpass;
      opt::OptPassConfig cast_eliminate_pass = opt::OptPassConfig({irpass.cast_eliminate_});
      auto iter = map_a->insert(parallel_end_opt, {"cast_eliminate", cast_eliminate_pass});
      (void)map_a->insert(iter, {"parallel_renormalize", opt::OptPassConfig::Renormalize()});
    }
  }
}

opt::OptPassConfig GetOptPassA1(const opt::irpass::OptimizeIRPassLib &irpass) {
  return opt::OptPassConfig({
    irpass.partial_defer_inline_,
    irpass.switch_defer_inline_,
    irpass.switch_layer_defer_inline_,
    irpass.switch_simplify_,
    irpass.exchange_switch_depend_value_,
    irpass.float_depend_g_call_,

    // Safe inlining
    irpass.inline_,
    irpass.updatestate_useless_node_eliminater_,
    irpass.updatestate_pure_node_eliminater_,
    irpass.load_eliminater_,
    irpass.stopgrad_eliminater_,
    irpass.partial_eliminate_,
    irpass.replace_applicator_,
    irpass.convert_tensor_eliminate_,

    // Miscellaneous
    irpass.list_to_tuple_eliminator_,
    irpass.tuple_to_list_eliminator_,
    irpass.tuple_list_get_item_eliminator_,
    irpass.make_slice_get_slice_eliminator_,
    irpass.tuple_list_get_item_const_eliminator_,
    irpass.tuple_list_set_item_eliminator_,
    irpass.tuple_list_get_set_item_eliminator_,
    irpass.tuple_list_get_item_depend_reorder_,
    irpass.tuple_list_convert_item_index_to_positive_,
    irpass.dict_get_item_eliminator_,
    irpass.dict_get_item_const_eliminator_,
    irpass.dict_set_item_eliminator_,

    irpass.environ_get_eliminate_,
    irpass.environ_get_add_eliminate_,
    irpass.environ_get_set_eliminate_,
    irpass.environ_get_depend_swap_,
    irpass.environ_add_const_eliminate_,

    irpass.cast_eliminate_,
    irpass.reshape_eliminate_,
    irpass.reduce_eliminate_,
    irpass.tile_eliminate_,
    irpass.transpose_eliminate_,
    irpass.minmaximum_grad_,

    // Arithmetic simplifications
    irpass.arithmetic_simplify_,
    irpass.addn_zero_filter_,
    irpass.adjust_all_reduce_mul_add_,
    irpass.accumulaten_eliminater_,

    // Safe inlining
    irpass.inline_,
    irpass.updatestate_useless_node_eliminater_,
    irpass.updatestate_pure_node_eliminater_,
    irpass.load_eliminater_,
    irpass.stopgrad_eliminater_,
    irpass.print_const_string_wrapper_,
  });
}

OptPassGroupMap GetOptPassesA(const opt::irpass::OptimizeIRPassLib &irpass) {
  opt::OptPassConfig a_1 = GetOptPassA1(irpass);
  opt::OptPassConfig a_2 = opt::OptPassConfig(
    {
      irpass.switch_simplify_,
      irpass.specialize_transform_,
      irpass.merge_addn_,
      irpass.compare_switch_simplify_,
      irpass.addn_check_dump_,
      irpass.float_tuple_getitem_switch_,
      irpass.float_environ_get_switch_,
      irpass.inline_,
      irpass.updatestate_useless_node_eliminater_,
      irpass.arithmetic_simplify_,
      irpass.tuple_list_set_item_eliminator_,
      irpass.tuple_list_get_item_eliminator_,
      irpass.incorporate_call_,
      irpass.incorporate_call_switch_,
      irpass.environ_get_eliminate_,
      irpass.depend_value_elim_,
      irpass.all_reduce_const_elim_,
    },
    false, true);

  opt::OptPassConfig a_after_grad = opt::OptPassConfig({irpass.inline_without_move_, irpass.stack_unstack_eliminate_});

  opt::OptPassConfig a_3 = opt::OptPassConfig(
    {
      irpass.same_eliminate_,
      irpass.check_bprop_eliminate_,
      irpass.switch_layer_defer_inline_,
      irpass.replace_applicator_,
      irpass.row_tensor_add_zeros_like_,
      irpass.mini_step_allgather_replace_,
      irpass.micro_step_allgather_replace_,
      irpass.split_environ_get_set_with_tuple_value_,
    },
    false, true);
  opt::OptPassConfig accelerated_algorithm = opt::OptPassConfig({irpass.less_batch_normalization_});
  opt::OptPassConfig virtual_dataset = opt::OptPassConfig({irpass.virtual_dataset_eliminate_});
  opt::OptPassConfig after_resolve_pass = opt::OptPassConfig({irpass.replace_old_param_});
  // Disable after_resolve_pass if Pre-Lift is enabled.
  static const bool enable_pre_lift = (common::GetCompileConfig("PRE_LIFT") == "1");
  if (enable_pre_lift) {
    after_resolve_pass.set_disabled(true);
  }
  opt::OptPassConfig updatestate_depend_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateDependEliminater());
  opt::OptPassConfig updatestate_assign_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateAssignEliminater());
  opt::OptPassConfig updatestate_loads_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateLoadsEliminater());
  opt::OptPassConfig recompute_prepare = opt::OptPassConfig({irpass.set_cell_output_no_recompute_});
  opt::OptPassConfig get_grad = opt::OptPassConfig({irpass.get_grad_eliminate_});
  opt::OptPassConfig cell_reuse_handle_not_recompute_node_pass =
    opt::OptPassConfig({irpass.remove_not_recompute_node_}, false, true);

  opt::OptPassConfig c_1 = opt::OptPassConfig({
    irpass.switch_call_monad_eliminater_,
    irpass.partial_eliminate_,
  });
  // Disable c_1 if Pre-Lift is not enabled.
  if (!enable_pre_lift) {
    c_1.set_disabled(true);
  }
  // Before adjusting map_a, check GetA1A2() and GetOptPynativeGradEpiloguePhases().
  OptPassGroupMap map_a({{"expand_dump_flag", opt::OptPassConfig(opt::irpass::ExpandDumpFlag())},
                         {"switch_simplify", opt::OptPassConfig({irpass.switch_simplify_})},
                         {"a_1", a_1},
                         {"recompute_prepare", recompute_prepare},
                         {"updatestate_depend_eliminate", updatestate_depend_eliminate},
                         {"updatestate_assign_eliminate", updatestate_assign_eliminate},
                         {"updatestate_loads_eliminate", updatestate_loads_eliminate},
                         {"c_1", c_1},
                         {"parameter_eliminate", opt::OptPassConfig(opt::irpass::ParameterEliminator())},
                         {"a_2", a_2},
                         {"accelerated_algorithm", accelerated_algorithm},
                         {"pynative_shard", opt::OptPassConfig(parallel::PynativeShard)},
                         {"auto_parallel", opt::OptPassConfig(parallel::StepAutoParallel)},
                         {"parallel", opt::OptPassConfig(parallel::StepParallel)},
                         {"merge_comm", opt::OptPassConfig(parallel::MergeComm)},
                         {"allreduce_fusion", opt::OptPassConfig(parallel::StepAllreduceFusion)},
                         {"virtual_dataset", virtual_dataset},
                         {"get_grad_eliminate_", get_grad},
                         {"virtual_output", opt::OptPassConfig({irpass.virtual_output_eliminate_})},
                         {"merge_forward", opt::OptPassConfig(ad::MergeForward)},
                         {"cell_reuse_recompute_pass", opt::OptPassConfig(opt::irpass::AddRecomputeNodes)},
                         {"cell_reuse_handle_not_recompute_node_pass", cell_reuse_handle_not_recompute_node_pass},
                         {"meta_fg_expand", opt::OptPassConfig(opt::irpass::ExpandMetaFg())},
                         {"receive_attached", opt::OptPassConfig(parallel::IsolatedNodeAttach)},
                         {"after_resolve", after_resolve_pass},
                         {"a_after_grad", a_after_grad},
                         {"renormalize", opt::OptPassConfig::Renormalize()},
                         {"real_op_eliminate", opt::OptPassConfig({irpass.real_op_eliminate_})},
                         {"add_forward_monad_depend", opt::OptPassConfig(opt::irpass::AddForwardMonadDepend)},
                         {"auto_monad_grad", opt::OptPassConfig(ReAutoMonadWrapper)},
                         {"auto_monad_eliminator", opt::OptPassConfig(opt::AutoMonadEliminator())},
                         {"cse", opt::OptPassConfig(opt::CSEPass(false))},
                         {"a_3", a_3}});
  AddParallelRenormalize(&map_a);
  return map_a;
}

OptPassGroupMap GetA1A2(const opt::irpass::OptimizeIRPassLib &irpass) {
  auto opt_a = GetOptPassesA(irpass);
  constexpr auto a1_a2_len = 10;
  OptPassGroupMap a1_a2(opt_a.begin(), opt_a.begin() + a1_a2_len);
  return a1_a2;
}

OptPassGroupMap GetOptPassesAfterCconv(const opt::irpass::OptimizeIRPassLib &irpass) {
  opt::OptPassConfig c_1 = opt::OptPassConfig({
    // Safe inlining,
    irpass.inline_,
    irpass.updatestate_useless_node_eliminater_,
    irpass.updatestate_pure_node_eliminater_,
    irpass.load_eliminater_,
    irpass.switch_call_monad_eliminater_,
    irpass.stopgrad_eliminater_,
    irpass.partial_eliminate_,
  });
  opt::OptPassConfig updatestate_depend_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateDependEliminater());
  opt::OptPassConfig updatestate_assign_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateAssignEliminater());
  opt::OptPassConfig updatestate_loads_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateLoadsEliminater());

  OptPassGroupMap map_a({{"c_1", c_1},
                         {"parameter_eliminate", opt::OptPassConfig(opt::irpass::ParameterEliminator())},
                         {"updatestate_depend_eliminate", updatestate_depend_eliminate},
                         {"updatestate_assign_eliminate", updatestate_assign_eliminate},
                         {"updatestate_loads_eliminate", updatestate_loads_eliminate},
                         {"cse", opt::OptPassConfig(opt::CSEPass(false))},
                         {"renormalize", opt::OptPassConfig::Renormalize()}});

  return map_a;
}

OptPassGroupMap GetOptPassesTransformGraph(const opt::irpass::OptimizeIRPassLib &irpass) {
  opt::OptPassConfig d_1 = opt::OptPassConfig({
    irpass.call_graph_tuple_transform_,
    irpass.list_to_tuple_eliminator_,
    irpass.tuple_to_list_eliminator_,
    irpass.tuple_list_get_item_eliminator_,
    irpass.tuple_list_get_item_const_eliminator_,
    irpass.tuple_list_set_item_eliminator_,
    irpass.tuple_list_get_set_item_eliminator_,
    irpass.tuple_list_get_item_depend_reorder_,
    irpass.tuple_list_convert_item_index_to_positive_,
  });

  opt::OptPassConfig d_2 = opt::OptPassConfig({irpass.partial_unused_args_eliminate_});

  OptPassGroupMap map_a({{"d_1", d_1}, {"d_2", d_2}, {"renormalize", opt::OptPassConfig::Renormalize()}});

  return map_a;
}

OptPassGroupMap GetOptPassesB(const opt::irpass::OptimizeIRPassLib &irpass) {
  opt::OptPassConfig b_1 = opt::OptPassConfig({irpass.zero_like_fill_zero_,
                                               irpass.list_to_tuple_eliminator_,
                                               irpass.tuple_to_list_eliminator_,
                                               irpass.tuple_list_get_item_eliminator_,
                                               irpass.tuple_list_get_item_const_eliminator_,
                                               irpass.tuple_list_set_item_eliminator_,
                                               irpass.tuple_list_get_set_item_eliminator_,
                                               irpass.tuple_list_get_item_depend_reorder_,
                                               irpass.tuple_list_convert_item_index_to_positive_,
                                               irpass.make_slice_get_slice_eliminator_,
                                               irpass.float_tuple_getitem_switch_,
                                               irpass.reset_defer_inline_,
                                               irpass.inline_,
                                               irpass.updatestate_useless_node_eliminater_,
                                               irpass.updatestate_pure_node_eliminater_,
                                               irpass.load_eliminater_,
                                               irpass.stopgrad_eliminater_,
                                               irpass.special_op_eliminate_,
                                               irpass.environ_get_eliminate_,
                                               irpass.environ_get_add_eliminate_,
                                               irpass.environ_get_set_eliminate_,
                                               irpass.environ_get_depend_swap_,
                                               irpass.environ_add_const_eliminate_,
                                               irpass.value_based_eliminate_,
                                               irpass.parallel_virtual_node_,
                                               irpass.const_output_eliminate_},
                                              false, true);
  opt::OptPassConfig b_2 = opt::OptPassConfig({
    irpass.row_tensor_eliminate_,
  });
  opt::OptPassConfig updatestate_depend_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateDependEliminater());
  opt::OptPassConfig updatestate_assign_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateAssignEliminater());
  opt::OptPassConfig updatestate_loads_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateLoadsEliminater());
  OptPassGroupMap map({
    {"b_1", b_1},
    {"b_2", b_2},
    {"updatestate_depend_eliminate", updatestate_depend_eliminate},
    {"updatestate_assign_eliminate", updatestate_assign_eliminate},
    {"updatestate_loads_eliminate", updatestate_loads_eliminate},
    {"renormalize", opt::OptPassConfig::Renormalize()},
    {"cse", opt::OptPassConfig(opt::CSEPass(false))},
  });
  return map;
}

OptPassGroupMap GetOptPassesPynativeElim(const opt::irpass::OptimizeIRPassLib &irpass) {
  opt::OptPassConfig pynative_eliminate = opt::OptPassConfig({
    irpass.pynative_eliminate_,
  });

  OptPassGroupMap map({
    {"pynative_eliminate", pynative_eliminate},
  });
  return map;
}

OptPassGroupMap GetOptPassesC(const opt::irpass::OptimizeIRPassLib &) {
  return OptPassGroupMap({{"renormalize", opt::OptPassConfig::Renormalize()}});
}

OptPassGroupMap GetOptPynativeGradEpiloguePhases(const opt::irpass::OptimizeIRPassLib &irpass) {
  auto opt_a = GetOptPassesA(irpass);
  auto a3 = opt_a[opt_a.size() - 1];
  OptPassGroupMap map({
    {"renormalize", opt::OptPassConfig::Renormalize()},
    {"cse", opt::OptPassConfig(opt::CSEPass(false))},
    {a3},
  });
  return map;
}

OptPassGroupMap GetMetaUnpackPreparePhases() {
  opt::irpass::MetaUnpackPrepareLib irpass;
  auto meta_unpack_prepare = opt::OptPassConfig({irpass.meta_unpack_prepare_});
  opt::OptPassGroupMap prepare_map({{"meta_unpack_prepare", meta_unpack_prepare}});
  return prepare_map;
}

OptPassGroupMap GetGradPartialTransformPhases() {
  opt::irpass::GradPartialPassLib irpass;
  auto grad_partial_transform = opt::OptPassConfig({irpass.grad_partial_transform_});
  opt::OptPassGroupMap grad_partial_transform_map({{"grad_partial_transform", grad_partial_transform}});
  return grad_partial_transform_map;
}

OptPassGroupMap GetPreparePhases(const opt::irpass::OptimizeIRPassLib &irpass) {
  opt::OptPassConfig prepare_group = opt::OptPassConfig({irpass.print_tuple_wrapper_});
  OptPassGroupMap map({{"prepare_group", prepare_group}});
  return map;
}

OptPassGroupMap GetAfterRecomputePass(const opt::irpass::OptimizeIRPassLib &) {
  OptPassGroupMap map({{"cse", opt::OptPassConfig(opt::CSEPass(false))}});
  return map;
}

OptPassGroupMap GetSymbolEngineOptPass(const opt::irpass::OptimizeIRPassLib &irpass) {
  if (common::GetEnv("MS_SYMBOL_ENGINE_OPTIMIZE") == "off") {
    MS_LOG(INFO) << "SymbolEngineOptimizer is disabled.";
    return OptPassGroupMap();
  }
  OptPassGroupMap map({{"build", opt::OptPassConfig(opt::irpass::SymbolEngineBuilder())},
                       {"elim_shapecalc", opt::OptPassConfig({irpass.elim_shapecalc_of_broadcastargs_})},
                       {"elim_not_effective", opt::OptPassConfig({irpass.elim_not_effective_node_})},
                       {"opt_reshape", opt::OptPassConfig({irpass.opt_reshape_})},
                       {"fold_const_symbol", opt::OptPassConfig({irpass.fold_const_symbol_})},
                       {"shape_op_cse", opt::OptPassConfig(opt::irpass::ShapeOpCse())},
                       {"renormalize", opt::OptPassConfig::Renormalize()}});
  return map;
}

static mindspore::HashMap<std::string, std::shared_ptr<Optimizer>> g_pass_opts = {};

void InitOpt(const ResourcePtr &resource) {
  if (g_pass_opts.size() == 0) {
    opt::irpass::OptimizeIRPassLib irpass;
    g_pass_opts["a1a2"] = Optimizer::MakeOptimizer("a1a2", resource, GetA1A2(irpass));
    g_pass_opts["opt_a"] = Optimizer::MakeOptimizer("opt_a", resource, GetOptPassesA(irpass));
    g_pass_opts["opt_b"] = Optimizer::MakeOptimizer("opt_b", resource, GetOptPassesB(irpass), false, true);
    g_pass_opts["opt_after_cconv"] =
      Optimizer::MakeOptimizer("opt_after_cconv", resource, GetOptPassesAfterCconv(irpass), false, true);
    g_pass_opts["opt_trans_graph"] =
      Optimizer::MakeOptimizer("opt_trans_graph", resource, GetOptPassesTransformGraph(irpass), true, true);
    g_pass_opts["renormal"] = Optimizer::MakeOptimizer("renormal", resource, GetOptPassesC(irpass));
    g_pass_opts["opt_grad_epilogue"] =
      Optimizer::MakeOptimizer("opt_grad_epilogue", resource, GetOptPynativeGradEpiloguePhases(irpass), true, false);
    g_pass_opts["opt_prepare"] = Optimizer::MakeOptimizer("opt_prepare", resource, GetPreparePhases(irpass));
    g_pass_opts["opt_after_recompute"] =
      Optimizer::MakeOptimizer("opt_after_recompute", resource, GetAfterRecomputePass(irpass));
    g_pass_opts["symbol_engine_opt"] =
      Optimizer::MakeOptimizer("symbol_engine_opt", resource, GetSymbolEngineOptPass(irpass), true, true);
  }
}
}  // namespace

void ReclaimOptimizer() {
  for (auto &opt : g_pass_opts) {
    opt.second = nullptr;
  }
  g_pass_opts.clear();
}

bool OptPassGroup(const ResourcePtr &resource, const std::string &name) {
  MS_EXCEPTION_IF_NULL(resource);
  if (resource->func_graph() == nullptr) {
    MS_LOG(ERROR) << "Opt passes int64_t error";
    return false;
  }

  FuncGraphPtr func_graph = resource->func_graph();
  MS_LOG(DEBUG) << "Start " << name << " func graph:" << func_graph->ToString() << ", "
                << func_graph->get_return()->DebugString(true);
  InitOpt(resource);
  if (g_pass_opts.find(name) != g_pass_opts.end()) {
    resource->set_func_graph(g_pass_opts[name]->step(func_graph));
  }
  // Note: StepParallel may modify the AbstractValue of the parameters of func_graph, but they are not updated to
  // resource->args_abs_ yet. So if any later pass or action want to use that variable, it should be set here.
  return true;
}

bool OptPassA1A2(const ResourcePtr &resource) { return OptPassGroup(resource, "a1a2"); }
bool OptPassAGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "opt_a"); }
bool OptPassBGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "opt_b"); }
bool OptPassAfterCconvGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "opt_after_cconv"); }
bool OptPassTransformGraphGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "opt_trans_graph"); }
bool ControlGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "opt_control"); }
bool PrepareGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "opt_prepare"); }
bool OptAfterRecomputeGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "opt_after_recompute"); }

bool OptPassRNGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "renormal"); }
bool SymEngOptGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "symbol_engine_opt"); }

bool OptPassGradEpilogueGroup(const ResourcePtr &resource) { return OptPassGroup(resource, "opt_grad_epilogue"); }

bool AddRecomputationPass(const ResourcePtr &resource) {
  auto context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(context);
  if (context->CellReuseLevel() != CellReuseLevel::kNoCellReuse) {
    return true;
  }
  MS_EXCEPTION_IF_NULL(resource);
  opt::InsertRecomputedNodes(resource->func_graph());
  return true;
}

bool SliceRecomputeActivationPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  opt::SliceRecomputedActivationNodes(resource->func_graph());
  return true;
}

bool GroupedPairwiseExchangeAllToAllPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  opt::SetGroupedPairwiseExchangeAllToAll(resource);
  return true;
}

bool SliceReuseRecomputedActivationPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::SliceReuseRecomputedActivationNodes(resource->func_graph());
  return true;
}

bool LabelMicroInterleavedIndexPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::LabelMicroInterleavedIndex(resource->func_graph());
  return true;
}

bool LabelFineGrainedInterleavedIndexPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::LabelFineGrainedInterleavedIndex(resource->func_graph());
  return true;
}

bool AssignAddOpt(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  parallel::AssignAddOpt(func_graph);
  auto ms_context = MsContext::GetInstance();
  auto enable_concat_eliminate = ms_context->get_param<bool>(MS_CTX_ENABLE_CONCAT_ELIMINATE_OPT);
  if (!enable_concat_eliminate) {
    return true;
  }
  OptPassGroupMap map({{"renormalize", opt::OptPassConfig({opt::OptPassConfig::Renormalize()})}});
  auto renormalize = opt::Optimizer::MakeOptimizer("renormalize", resource, map);
  (void)renormalize->step(func_graph, false);
  return true;
}

bool MergeCastOpt(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::MergeCastOpt(resource->func_graph());
  return true;
}

bool ForceFp32Comm(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::Float32Redistribution(resource->func_graph());
  return true;
}

bool RemoveCastBeforeAssignAdd(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::RemoveCastBeforeAssignAdd(resource->func_graph());
  return true;
}

bool ReorderSendRecvBetweenFpBpPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::ReorderSendRecvBetweenFpBp(resource->func_graph());
  return true;
}

bool CompCommSchedulingPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  opt::CompCommScheduling(resource->func_graph());
  return true;
}

bool MicroInterLeavedOrderControlPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::MicroInterleavedOrderControl(resource->func_graph());
  return true;
}

bool OverlapGradCommPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::OverlapGradComm(resource->func_graph());
  return true;
}

bool FullMicroInterLeavedOrderControlPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::FullMicroInterleavedOrderControl(resource->func_graph());
  return true;
}

bool SplitMatmulCommElementwiseOpFpPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::SplitMatmulCommElementwiseFp(resource->func_graph());
  return true;
}

bool SplitLayerNormCommFpPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::SplitLayerNormCommFp(resource->func_graph());
  return true;
}

bool CommOpAddAttrs(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  opt::CommOpAttrs(resource->func_graph());
  return true;
}

bool ProcessSendRecvForGE(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  opt::ProcessSendRecvForGE(resource->func_graph());
  return true;
}

bool AddCommOpReusePass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  opt::AddCommOpReuseTag(resource->func_graph());
  return true;
}

bool OverlapOptShardInPipelinePass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::OverlapOptShardInPipeline(resource->func_graph());
  return true;
}

bool BeginEndOverlapInlinePass(const ResourcePtr &resource) {
  auto ms_context = MsContext::GetInstance();
  auto is_enable = ms_context->get_param<bool>(MS_CTX_ENABLE_BEGIN_END_INLINE_OPT);
  if (!is_enable) {
    return true;
  }
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  parallel::BeginEndOverlapInlineOpt(resource->func_graph());
  opt::irpass::OptimizeIRPassLib irpass;
  opt::OptPassConfig get_item_eliminator_pass = opt::OptPassConfig({irpass.tuple_list_get_item_eliminator_});
  OptPassGroupMap map({{"get_item_eliminator", get_item_eliminator_pass}});
  auto get_item_eliminator = opt::Optimizer::MakeOptimizer("get_item_eliminator", resource, map);
  (void)get_item_eliminator->step(func_graph, false);
  return true;
}

bool OverlapGradMatmulAndGradAllreduce(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::OverlapGradMatmulAndGradAllreduce(resource->func_graph());
  return true;
}

bool HandleGroupInfoPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::HandleGroupInfo();
  return true;
}

bool OverlapRecomputeAndGradModelParallel(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  parallel::OverlapRecomputeAndGradModelParallel(resource->func_graph());
  return true;
}

bool AddCacheEmbeddingPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
#if defined(__linux__) && defined(WITH_BACKEND)
  if (ps::PSContext::instance()->is_ps_mode()) {
    return true;
  }
#endif
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);

  parallel::AddCacheEmbedding(func_graph);
  if (func_graph->has_flag(GRAPH_FLAG_CACHE_ENABLE)) {
    auto params = func_graph->parameters();
    AbstractBasePtrList args_abs_list;
    (void)std::for_each(params.begin(), params.end(),
                        [&args_abs_list](const AnfNodePtr &node) { args_abs_list.push_back(node->abstract()); });
    func_graph = pipeline::Renormalize(resource, func_graph, args_abs_list);
  }
  return true;
}

bool RemoveValueNodeDuplicationsPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  if (resource->func_graph() == nullptr) {
    MS_LOG(INTERNAL_EXCEPTION) << "Remove value node duplications error.";
  }
  auto manager = resource->manager();
  HashCache hash_cache;
  HashValue hashes;
  // Remove duplicated value nodes across all graphs in manager
  const auto &node_user_map = manager->node_users();
  for (auto &fg : manager->func_graphs()) {
    auto value_nodes = fg->value_nodes();
    for (const auto &value_pair : value_nodes) {
      auto &users = node_user_map.at(value_pair.first);
      auto prim = GetValueNode<PrimitivePtr>(value_pair.first);
      if (IsPrimitiveEquals(prim, prim::kPrimUpdateState)) {
        continue;
      }
      // For data parallel with some parameters redundant, the allreduce will share the same value node
      // which will raise an error when do allreduce fusion, so the solution is to make the allreduce's value node
      // not be removed, if we found the fusion tag.
      if (users.size() == 1) {
        auto cnode = users.front().first->cast<CNodePtr>();
        if (IsPrimitiveCNode(cnode, prim::kPrimAllReduce) && cnode->size() > 1 && cnode->input(1)->isa<ValueNode>()) {
          auto allreduce_prim = GetCNodePrimitive(users.front().first);
          auto attrs = allreduce_prim->attrs();
          auto fusion_id = attrs.find(mindspore::parallel::FUSION);
          if (fusion_id != attrs.end() && GetValue<int64_t>(fusion_id->second) > 0) {
            continue;
          }
        }
      }
      TryToDoReplace(manager.get(), value_pair.first, &hash_cache, &hashes);
    }
  }
  return true;
}

bool CconvPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  MS_EXCEPTION_IF_NULL(resource->func_graph());
  FuncGraphPtr func_graph = resource->func_graph();
  FuncGraphPtr new_fg = LiftingClone(func_graph);
  resource->set_func_graph(new_fg);
  return true;
}

bool PipelineSplitPass(const ResourcePtr &resource) { return PipelineSplit(resource); }

bool ParallelVirtualDatasetPass(const ResourcePtr &resource) { return ParallelVirtualDataset(resource); }

bool PipelineParallelScheduler(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  auto root = resource->func_graph();
  auto parallel_context = parallel::ParallelContext::GetInstance();
  MS_EXCEPTION_IF_NULL(parallel_context);
  auto parallel_mode = parallel_context->parallel_mode();
  if (parallel_mode != parallel::kSemiAutoParallel && parallel_mode != parallel::kAutoParallel) {
    MS_LOG(INFO) << "Only auto_parallel and semi_auto_parallel support pipeline split.";
    return true;
  }
  auto is_pp_interleave = parallel_context->pipeline_interleave();
  auto stage_num = parallel_context->pipeline_stage_split_num();
  if (is_pp_interleave && stage_num > 1) {
    auto manager = resource->manager();
    auto stage = parallel::InferStage();
    auto pp_scheduler = parallel_context->pipeline_scheduler();
    std::shared_ptr<parallel::PipelineScheduler> scheduler = nullptr;
    if (pp_scheduler == parallel::kPipeline1F1B) {
      scheduler = std::make_shared<parallel::InterleavedScheduler>(manager, root, stage, stage_num);
    } else if (pp_scheduler == parallel::kPipelineGpipe) {
      scheduler = std::make_shared<parallel::GpipeInterleavedScheduler>(manager, root, stage, stage_num);
    } else {
      MS_LOG(EXCEPTION) << "Unsupported pipeline parallel scheduler: " << pp_scheduler;
    }
    scheduler->GetBorderNode();
    scheduler->Reorder();
  }
  opt::ProcessSendRecvForGE(root);
  return true;
}

bool AutoParallelPass(const ResourcePtr &resource) {
  auto func_graph = resource->func_graph();
  auto opt = opt::Optimizer::MakeEmptyOptimizer(resource);
  return parallel::StepAutoParallel(func_graph, opt);
}

bool AutoParallelSymbolPassWithReNormalize(const ResourcePtr &resource) {
  // 1, auto parallel; 2, dynamic shape
  auto func_graph = resource->func_graph();
  if (!parallel::IsParallelDynamicShape(func_graph)) {
    return true;
  }
  MS_LOG(INFO) << "symbol pass for parallel begin";
  // must be bind with renormalize
  OptPassGroupMap opt_map({{"renormalize", opt::OptPassConfig::Renormalize()},
                           {"build", opt::OptPassConfig(opt::irpass::SymbolEngineBuilder())}});
  auto opt = opt::Optimizer::MakeOptimizer("parallel-infer-symbol", resource, opt_map, true);
  (void)opt->step(func_graph, false);
  MS_LOG(INFO) << "symbol pass for parallel end";
  return true;
}

bool ValidatePass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  MS_EXCEPTION_IF_NULL(resource->func_graph());
  FuncGraphPtr func_graph = resource->func_graph();
  Validate(func_graph);
  return true;
}

bool MetaUnpackPreparePass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  auto prepare_map = GetMetaUnpackPreparePhases();
  auto infer_opt_prepare = opt::Optimizer::MakeOptimizer("meta_unpack_prepare", resource, prepare_map);
  (void)infer_opt_prepare->step(func_graph, false);
  return true;
}

bool GradPartialTransformPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  auto grad_partial_transform_map = GetGradPartialTransformPhases();
  auto grad_partial_transform =
    opt::Optimizer::MakeOptimizer("grad_partial_transform", resource, grad_partial_transform_map);
  (void)grad_partial_transform->step(func_graph, false);
  return true;
}

bool PynativeOptPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  opt::irpass::OptimizeIRPassLib irpass;
  auto pynative_opt = GetOptPassesPynativeElim(irpass);
  auto pynative_opt_opt = opt::Optimizer::MakeOptimizer("pynative_opt", resource, pynative_opt);
  (void)pynative_opt_opt->step(func_graph, false);
  return true;
}

bool EliminateSpecialOpOptPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  auto func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  opt::irpass::OptimizeIRPassLib irpass;
  opt::OptPassConfig ad_related_special_op_eliminate = opt::OptPassConfig({
    irpass.ad_related_special_op_eliminate_,
  });
  opt::OptPassConfig mutable_op_eliminate = opt::OptPassConfig({
    irpass.mutable_op_eliminate_,
  });
  opt::OptPassConfig convert_tensor_op_eliminate = opt::OptPassConfig({
    irpass.convert_tensor_all_eliminate_,
  });
  OptPassGroupMap map({
    {"ad_related_special_op_eliminate", ad_related_special_op_eliminate},
    {"mutable_op_eliminate", mutable_op_eliminate},
    {"convert_tensor_op_eliminate", convert_tensor_op_eliminate},
  });
  auto special_op_eliminate_opt = opt::Optimizer::MakeOptimizer("special_op_eliminate", resource, map);
  (void)special_op_eliminate_opt->step(func_graph, false);
  return true;
}

bool AutoMonadElimOptPass(const FuncGraphPtr &func_graph) {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(func_graph->manager());
  auto resource = std::make_shared<pipeline::Resource>();
  resource->set_func_graph(func_graph);
  resource->set_manager(func_graph->manager());

  // opt::irpass::OptimizeIRPassLib is not used here to avoid double free problems in external calls.
  opt::SubstitutionPtr updatestate_useless_node_eliminater =
    opt::MakeSubstitution(std::make_shared<opt::irpass::UpdatestateUselessNodeEliminater>(),
                          "updatestate_useless_node_eliminater", prim::kPrimUpdateState);
  opt::SubstitutionPtr updatestate_pure_node_eliminater =
    opt::MakeSubstitution(std::make_shared<opt::irpass::UpdatestatePureNodeEliminater>(),
                          "updatestate_pure_node_eliminater", prim::kPrimUpdateState);

  opt::OptPassConfig updatestate_eliminater = opt::OptPassConfig({
    updatestate_useless_node_eliminater,
    updatestate_pure_node_eliminater,
  });
  opt::OptPassConfig updatestate_depend_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateDependEliminater());
  opt::OptPassConfig updatestate_assign_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateAssignEliminater());
  opt::OptPassConfig updatestate_loads_eliminate = opt::OptPassConfig(opt::irpass::UpdatestateLoadsEliminater());
  opt::OptPassGroupMap elim_map({
    {"updatestate_eliminater", updatestate_eliminater},
    {"updatestate_depend_eliminate", updatestate_depend_eliminate},
    {"updatestate_assign_eliminate", updatestate_assign_eliminate},
    {"updatestate_loads_eliminate", updatestate_loads_eliminate},
    {"auto_monad_eliminator", opt::OptPassConfig(opt::AutoMonadEliminator())},
  });

  auto auto_monad_elim_opt = opt::Optimizer::MakeOptimizer("auto_monad_elim", resource, elim_map);
  (void)auto_monad_elim_opt->step(func_graph, false);
  return true;
}

bool EnvironConversionPass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
  (void)opt::EnvironConversion(resource);
  return true;
}

// Build service-side graph for embedding distributed cache based on Parameter Server.
bool AddEmbeddingCachePass(const ResourcePtr &resource) {
  MS_EXCEPTION_IF_NULL(resource);
#if ((defined ENABLE_CPU) && (!defined _WIN32) && !defined(__APPLE__))
  if (!ps::PSContext::instance()->cache_enable() || !distributed::cluster::ClusterContext::instance()->initialized() ||
      !ps::PSContext::instance()->is_server()) {
    return true;
  }

  FuncGraphPtr func_graph = resource->func_graph();
  MS_EXCEPTION_IF_NULL(func_graph);
  auto node = distributed::cluster::ClusterContext::instance()->node();
  MS_EXCEPTION_IF_NULL(node);

  // 1. Build service-size graph.
  auto node_role = distributed::cluster::ClusterContext::instance()->node_role();
  uint32_t worker_num = ps::PSContext::instance()->worker_num();
  std::shared_ptr<parallel::PsEmbeddingCacheInserter> embedding_cache_inserter =
    std::make_shared<parallel::PsEmbeddingCacheInserter>(func_graph, static_cast<int64_t>(node->rank_id()), node_role,
                                                         worker_num);
  if (!embedding_cache_inserter->Run()) {
    MS_LOG(ERROR) << "Insert ps embedding cache failed.";
    return false;
  }

  // 2. Renomalize: Infer shape and Set abstract for all nodes in graph.
  abstract::AbstractBasePtrList args_abs;
  auto parameters = func_graph->parameters();
  (void)std::transform(parameters.begin(), parameters.end(), std::back_inserter(args_abs),
                       [](const AnfNodePtr &p) -> AbstractBasePtr { return p->abstract(); });
  FuncGraphPtr new_fg = Renormalize(resource, func_graph, args_abs);
  resource->set_func_graph(new_fg);
  resource->set_args_abs(args_abs);
#endif

  return true;
}

std::vector<PassItem> kVmPasses = {{"py_interpret_to_execute", PyInterpretToExecutePass},
                                   {"rewriter_before_opt_a", RewriterBeforeOptAPass},
                                   {"opt_a", OptPassAGroup},
                                   {"py_interpret_to_execute_after_opt_a", PyInterpretToExecutePass},
                                   {"slice_cell_reuse_recomputed_activation", SliceReuseRecomputedActivationPass},
                                   {"rewriter_after_opt_a", RewriterAfterOptAPass},
                                   {"convert_after_rewriter", ConvertAfterRewriterPass},
                                   {"order_py_execute_after_rewriter", OrderPyExecuteAfterRewriterPass},
                                   {"opt_b", OptPassBGroup},
                                   {"cconv", CconvPass},
                                   {"opt_after_cconv", OptPassAfterCconvGroup},
                                   {"remove_dup_value", RemoveValueNodeDuplicationsPass},
                                   {"tuple_transform", OptPassTransformGraphGroup},
                                   {"add_cache_embedding", AddCacheEmbeddingPass},
                                   {"add_recomputation", AddRecomputationPass},
                                   {"cse_after_recomputation", OptAfterRecomputeGroup},
                                   {"environ_conv", EnvironConversionPass},
                                   {"label_micro_interleaved_index", LabelMicroInterleavedIndexPass},
                                   {"label_fine_grained_interleaved_index", LabelFineGrainedInterleavedIndexPass},
                                   {"merge_cast_opt", MergeCastOpt},
                                   {"slice_recompute_activation", SliceRecomputeActivationPass},
                                   {"micro_interleaved_order_control", MicroInterLeavedOrderControlPass},
                                   {"assign_add_opt", AssignAddOpt},
                                   {"ForceFp32Comm", ForceFp32Comm},
                                   {"remove_cast_before_assign_add", RemoveCastBeforeAssignAdd},
                                   {"full_micro_interleaved_order_control", FullMicroInterLeavedOrderControlPass},
                                   {"comp_comm_scheduling", CompCommSchedulingPass},
                                   {"reorder_send_recv_between_fp_bp", ReorderSendRecvBetweenFpBpPass},
                                   {"comm_op_add_attrs", CommOpAddAttrs},
                                   {"add_comm_op_reuse_tag", AddCommOpReusePass},
                                   {"overlap_opt_shard_in_pipeline", OverlapOptShardInPipelinePass},
                                   {"grouped_pairwise_exchange_alltoall", GroupedPairwiseExchangeAllToAllPass},
                                   {"overlap_recompute_and_grad_model_parallel", OverlapRecomputeAndGradModelParallel},
                                   {"overlap_grad_matmul_and_grad_allreduce", OverlapGradMatmulAndGradAllreduce},
                                   {"begin_end_overlap_inline", BeginEndOverlapInlinePass},
                                   {"overlap_grad_comm", OverlapGradCommPass},
                                   {"split_matmul_comm_elemetwise", SplitMatmulCommElementwiseOpFpPass},
                                   {"split_layernorm_comm", SplitLayerNormCommFpPass},
                                   // The pass cache hccl group, so the hccl group should be created before the pass
                                   {"handle_group_info", HandleGroupInfoPass},
                                   {"symbol_engine_optimizer", SymEngOptGroup}};

std::vector<PassItem> kPynativePasses = {{"opt_a", OptPassAGroup},
                                         {"opt_b", OptPassBGroup},
                                         {"cconv", CconvPass},
                                         {"transform_top", TransformTopGraphPass},
                                         {"transform_graph", OptPassTransformGraphGroup}};

std::vector<PassItem> kInlinePasses = {{"rewriter_before_opt_a", RewriterBeforeOptAPass}, {"a1a2", OptPassA1A2}};
}  // namespace pipeline
}  // namespace mindspore
