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

#include "plugin/device/gpu/optimizer/bce_with_logits_loss_fusion.h"
#include <memory>
#include <vector>
#include <string>
#include "mindspore/core/ops/nn_ops.h"
#include "mindspore/core/ops/math_ops.h"
#include "mindspore/core/ops/op_utils.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "ir/primitive.h"
#include "include/common/utils/utils.h"
#include "include/backend/optimizer/helper.h"
#include "plugin/device/gpu/hal/device/kernel_info_setter.h"

namespace mindspore {
namespace opt {
std::vector<AnfNodePtr> GetReduceInputs(const FuncGraphPtr &func_graph, const CNodePtr &new_cnode,
                                        const std::string &reduction) {
  auto kernel_graph = func_graph->cast<std::shared_ptr<session::KernelGraph>>();
  MS_EXCEPTION_IF_NULL(kernel_graph);

  std::vector<AnfNodePtr> reduce_inputs;
  std::vector<int64_t> empty_axis;
  auto axis_node = AnfAlgo::ConvertValueToNode(kernel_graph, MakeValue(empty_axis));

  // keepdims node
  auto keepdims_node = AnfAlgo::ConvertValueToNode(kernel_graph, MakeValue(false));
  // skipmode node
  auto skipmode_node = AnfAlgo::ConvertValueToNode(kernel_graph, MakeValue(false));

  common::AnfAlgo::SetNodeAttr(kAttrReduction, MakeValue("none"), new_cnode);
  // set reduction to None.
  if (reduction == "sum") {
    reduce_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReduceSum->name())), new_cnode, axis_node,
                     keepdims_node, skipmode_node};  // ReduceSum(input, axis, keepdims=false, skip_mode=false)
  } else if (reduction == "mean") {
    reduce_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimReduceMean->name())), new_cnode, axis_node,
                     keepdims_node};  // ReduceMean(input, axis, keepdims=false)
  } else {
    MS_LOG(INFO) << "Reduction is none, no optimization on current BCEWithLogitsLoss.";
  }
  return reduce_inputs;
}

AnfNodePtr AddReduceNode(const FuncGraphPtr &func_graph, const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(node);
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  std::vector<AnfNodePtr> node_inputs = {
    NewValueNode(std::make_shared<Primitive>(prim::kPrimBCEWithLogitsLoss->name()))};
  (void)node_inputs.insert(node_inputs.end(), cnode->inputs().begin() + 1, cnode->inputs().end());
  CNodePtr new_cnode = func_graph->NewCNode(node_inputs);
  MS_EXCEPTION_IF_NULL(new_cnode);
  auto predict_inputs_list = cnode->inputs();
  if (predict_inputs_list.size() <= 1) {
    MS_LOG(EXCEPTION) << "Node must have more than 2 inputs, but get " << predict_inputs_list.size();
  }
  auto predict_input = predict_inputs_list[1];
  auto new_node_dtype = {common::AnfAlgo::GetOutputInferDataType(predict_input, 0)};
  auto new_node_shape = {AnfAlgo::GetOutputDetailShape(predict_input, 0)};
  common::AnfAlgo::SetOutputTypeAndDetailShape(new_node_dtype, new_node_shape, new_cnode.get());

  string reduction = common::AnfAlgo::GetNodeAttr<std::string>(node, kAttrReduction);
  MS_LOG(INFO) << "Create reduce node for BCEWithLogitsLoss, reduction attr is: " << reduction;

  auto reduce_inputs = GetReduceInputs(func_graph, new_cnode, reduction);
  if (reduce_inputs.size() == 0) {
    return nullptr;
  }
  auto reduce_node = func_graph->NewCNode(reduce_inputs);
  MS_EXCEPTION_IF_NULL(reduce_node);
  auto type = common::AnfAlgo::GetOutputInferDataType(node, 0);
  auto shape = {AnfAlgo::GetOutputDetailShape(node, 0)};
  common::AnfAlgo::SetOutputTypeAndDetailShape({type}, shape, reduce_node.get());
  common::AnfAlgo::SetNodeAttr("keep_dims", MakeValue(false), reduce_node);
  reduce_node->set_scope(cnode->scope());
  return reduce_node;
}

const BaseRef BCEWithLogitsLossFusion::DefinePattern() const {
  VarPtr Xs = std::make_shared<SeqVar>();
  MS_EXCEPTION_IF_NULL(Xs);
  return VectorRef({prim::kPrimBCEWithLogitsLoss, Xs});
}

const AnfNodePtr BCEWithLogitsLossFusion::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                                  const EquivPtr &) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(node);
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  if (GetBoolAttr(cnode, kAttrVisited)) {
    return nullptr;
  }
  common::AnfAlgo::SetNodeAttr(kAttrVisited, MakeValue(true), node);
  if (cnode->size() == 0) {
    return nullptr;
  }
  return AddReduceNode(func_graph, node);
}
}  // namespace opt
}  // namespace mindspore
