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

#ifndef MINDSPORE_CCSRC_FRONTEND_PARALLEL_GRAPH_UTIL_GRAPH_UTILS_H_
#define MINDSPORE_CCSRC_FRONTEND_PARALLEL_GRAPH_UTIL_GRAPH_UTILS_H_
#include <set>
#include <vector>
#include <string>
#include "mindspore/core/base/base.h"
#include "mindspore/core/ir/anf.h"
#include "frontend/parallel/status.h"
#include "frontend/parallel/tensor_layout/tensor_redistribution.h"
#include "frontend/parallel/ops_info/operator_info.h"

namespace mindspore::parallel {
void InsertNode(const Operator &op, const CNodePtr &node, size_t index, const AnfNodePtr &pre_node,
                const FuncGraphPtr &func_graph, const std::string &instance_name, const std::string &param_name = "",
                const FuncGraphPtr &root = nullptr, const TensorRedistributionPtr &tensor_redistribution = nullptr);
std::set<FuncGraphPtr> FindForwardGraphByRootNodes(const std::vector<AnfNodePtr> &root_all_nodes);
std::vector<AnfNodePtr> ReplaceOpInput(const Operator &replace_op, const std::string &instance_name,
                                       const CNodePtr &node);
std::vector<AnfNodePtr> CreateInput(const Operator &op, const AnfNodePtr &pre_node, const std::string &instance_name,
                                    const CNodePtr &cur_cnode = nullptr);
std::vector<AnfNodePtr> CreateMirrorInput(const FuncGraphPtr &root, const Operator &op, const AnfNodePtr &node,
                                          const std::string &instance_name, const std::string &weight_name);
CNodePtr CreateShape(const AnfNodePtr &pre_cnode, const FuncGraphPtr &func_graph, const std::string &inst_name = "");
AnfNodePtr GetAccuGrad(const std::vector<AnfNodePtr> &parameters, const std::string &weight_name);
AnfNodePtr ConvertConstParamToDynamic(const TensorRedistributionPtr &tensor_redistribution, const Param &param,
                                      const FuncGraphPtr &func_graph);
AnfNodePtr CreateDiv(const AnfNodePtr &input_node, int64_t divisor, const FuncGraphPtr &func_graph,
                     bool to_long = false, const std::string &inst_name = "");
CNodePtr CreateSplit(const std::vector<AnfNodePtr> &inputs, const FuncGraphPtr &func_graph,
                     const std::string &inst_name = "");
bool IsToBeInsertedSplitOp(const Operator &op);
Status MergeEntireShapeForDynamic(const FuncGraphPtr &func_graph);
}  // namespace mindspore::parallel
#endif  // MINDSPORE_CCSRC_FRONTEND_PARALLEL_GRAPH_UTIL_GRAPH_UTILS_H_
