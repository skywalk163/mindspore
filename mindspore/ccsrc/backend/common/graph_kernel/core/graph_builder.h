/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_CORE_GRAPH_BUILDER_H_
#define MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_CORE_GRAPH_BUILDER_H_

#include <unordered_map>
#include <tuple>
#include <string>
#include "ir/anf.h"

namespace mindspore::graphkernel {
using AnfNodePtrToAnfNodePtrMap = std::unordered_map<AnfNodePtr, AnfNodePtr>;

struct ClusterConfig {
  bool inline_sub_func_graph{true};
  bool only_output_basenode{false};
  bool sort_parameter{false};
  AnfNodePtr base_node{nullptr};
};

std::tuple<FuncGraphPtr, AnfNodePtrList, AnfNodePtrList> BuildGraphFromNodes(
  const AnfNodePtrList &nodes, const ClusterConfig &config = ClusterConfig{});
std::tuple<FuncGraphPtr, AnfNodePtrList, AnfNodePtrList> BuildSingleGraphFromNodes(
  const AnfNodePtrList &nodes, const ClusterConfig &config = ClusterConfig());
CNodePtr CreateNewFuseCNode(const FuncGraphPtr &main_fg, const FuncGraphPtr &sub_fg, const AnfNodePtrList &inputs);
CNodePtr ReplaceNodesWithGraphKernelNode(const AnfNodePtrList &nodes, const FuncGraphPtr &main_graph,
                                         const std::string &postfix = "",
                                         const ClusterConfig &config = ClusterConfig{});
CNodePtr ReplaceNodesWithGraphKernelFuncGraph(const FuncGraphPtr &main_graph, const FuncGraphPtr &sub_graph,
                                              const AnfNodePtrList &inputs, const AnfNodePtrList &outputs);
bool ConvertTensorToParameter(const FuncGraphPtr &fg, AnfNodePtrList *inputs_ptr);
/// \brief Put Monads at end of parameters
bool SortParameters(const FuncGraphPtr &fg, AnfNodePtrList *inputs_ptr);
bool EliminateMaketupleGetitem(const FuncGraphPtr &fg);
void EliminateRedundantParameters(const FuncGraphPtr &func_graph, AnfNodePtrList *inputs);
}  // namespace mindspore::graphkernel
#endif  // MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GRAPH_KERNEL_CORE_GRAPH_BUILDER_H_
