/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GPU_COMBINE_OPTIMIZER_FUSION_H_
#define MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GPU_COMBINE_OPTIMIZER_FUSION_H_

#include <memory>
#include <string>
#include <vector>
#include "include/backend/optimizer/optimizer.h"
#include "ops/ascend_op_name.h"

namespace mindspore {
namespace opt {
class CombineOptimizerFusion : public Pass {
 public:
  explicit CombineOptimizerFusion(const std::string &name) : Pass(kCombineOptimizerOpName) { InitCombineOptimizer(); }
  ~CombineOptimizerFusion() override = default;
  bool Run(const FuncGraphPtr &graph) override;

 private:
  void InitCombineOptimizer();
  bool CheckCondition();
  bool CheckFuncGraph(const FuncGraphPtr &graph);
  bool TransformOptimizerList(const std::vector<AnfNodePtr> &node_list,
                              std::vector<std::vector<AnfNodePtr>> *deal_list);
  AnfNodePtr FindFirstMonadInput(const std::vector<AnfNodePtr> &optimizer_node_list,
                                 const mindspore::HashMap<AnfNodePtr, size_t> &nodes_to_topo_indexes);
};
}  // namespace opt
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_OPTIMIZER_GPU_COMBINE_OPTIMIZER_FUSION_H_
