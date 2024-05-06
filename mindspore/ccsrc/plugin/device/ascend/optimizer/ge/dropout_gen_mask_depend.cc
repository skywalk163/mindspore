/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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
#include "plugin/device/ascend/optimizer/ge/dropout_gen_mask_depend.h"

#include "ops/sequence_ops.h"
#include "ops/framework_ops.h"
#include "mindspore/core/ops/nn_ops.h"
#include "ir/graph_utils.h"
#include "include/backend/optimizer/helper.h"
#include "include/common/utils/utils.h"
#include "include/common/utils/parallel_context.h"
#include "include/common/utils/anfalgo.h"

namespace mindspore {
namespace opt {
constexpr size_t kInputIndex1 = 1;
bool DropoutGenMaskDepend::Run(const FuncGraphPtr &func_graph) {
  MS_EXCEPTION_IF_NULL(func_graph);
  std::vector<AnfNodePtr> genmasks;
  bool changed = false;
  auto manager = func_graph->manager();
  MS_EXCEPTION_IF_NULL(manager);
  std::vector<AnfNodePtr> node_list = TopoSort(func_graph->get_return());

  // Get all GenMasks with fusion attr
  for (auto &node : node_list) {
    MS_EXCEPTION_IF_NULL(node);
    if (IsPrimitiveCNode(node, prim::kPrimDropoutGenMask)) {
      genmasks.push_back(node);
    }
  }

  auto nodes_size = genmasks.size();
  if (nodes_size == 0) {
    MS_LOG(INFO) << "No Dropout Gen Mask.";
    return false;
  }

  auto kernel_graph = func_graph->cast<std::shared_ptr<session::KernelGraph>>();
  for (auto &mask_node : genmasks) {
    auto mask_cnode = mask_node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(mask_cnode);
    auto inputs = mask_cnode->inputs();
    if (inputs.size() - 1 <= kInputIndex1) {
      MS_EXCEPTION(IndexError) << "For DropoutGenMask, the num of inputs is " << inputs.size() - 1 << ", expect 2";
    }
    auto input_node = inputs[kInputIndex1];
    if (IsValueNode<ValueSequence>(input_node)) {
      auto tensor_input = CreateTensorInput(kernel_graph, input_node);
      MS_EXCEPTION_IF_NULL(tensor_input);
      mask_cnode->set_input(kInputIndex1, tensor_input);
    }
  }

  for (size_t i = 0; i < nodes_size - 1; ++i) {
    auto this_node = genmasks[i];
    auto next_node = genmasks[i + 1];
    auto next_cnode = next_node->cast<CNodePtr>();
    MS_EXCEPTION_IF_NULL(next_cnode);
    std::vector<AnfNodePtr> inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimDepend->name())),
                                      common::AnfAlgo::GetInputNode(next_cnode, 0), this_node};
    auto new_input = func_graph->NewCNode(inputs);
    new_input->set_scope(this_node->scope());
    new_input->set_abstract(common::AnfAlgo::GetInputNode(next_cnode, 0)->abstract());
    common::AnfAlgo::SetNodeInput(next_cnode, new_input, 0);
    changed = true;
  }
  return changed;
}
}  // namespace opt
}  // namespace mindspore
