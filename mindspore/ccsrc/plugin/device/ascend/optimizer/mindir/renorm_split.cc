/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/optimizer/mindir/renorm_split.h"
#include <algorithm>
#include <memory>
#include <vector>
#include "include/common/utils/anfalgo.h"
#include "ops/nn_op_name.h"
#include "ops/array_ops.h"
#include "ops/nn_ops.h"
#include "include/backend/optimizer/helper.h"

namespace mindspore {
namespace opt {
namespace {
constexpr auto kAttrRecomputeShape = "RecomputeShape";
void FreshRenormInferShape(const CNodePtr &node, ShapeVector in_shape, const TypeId &type) {
  MS_EXCEPTION_IF_NULL(node);
  auto dim = common::AnfAlgo::GetNodeAttr<int64_t>(node, "dim");
  if (dim > 0 && dim >= SizeToLong(in_shape.size())) {
    MS_LOG(EXCEPTION) << "Attr dim must be less than the shape size, but got dim:" << dim
                      << ", shape size:" << in_shape.size();
  }
  if (dim < 0) {
    if (std::abs(dim) <= SizeToLong(in_shape.size())) {
      dim += SizeToLong(in_shape.size());
      common::AnfAlgo::SetNodeAttr("dim", MakeValue(dim), node);
    } else {
      MS_LOG(EXCEPTION) << "Attr dim must be less than the shape size, but got dim:" << dim
                        << ", shape size:" << in_shape.size();
    }
  }

  for (size_t i = 0; i < in_shape.size(); i++) {
    if (static_cast<int64_t>(i) != dim) {
      in_shape[i] = 1;
    }
  }
  common::AnfAlgo::SetOutputInferTypeAndShape({type}, {in_shape}, node.get());
}
}  // namespace

const BaseRef RenormSplit::DefinePattern() const {
  std::shared_ptr Xs = std::make_shared<SeqVar>();
  return VectorRef({prim::kPrimRenorm, Xs});
}

/**
 * Renorm split
 *                    operatorA
 *                         \
 *   operatorA           Renorm
 *       |                  \
 *    Renorm     -->    BroadcastTo operatorA
 *                           \         /
 *                               Mul
 * */

const AnfNodePtr RenormSplit::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node, const EquivPtr &) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(node);
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  auto op_name = common::AnfAlgo::GetCNodeName(node);
  if (op_name != kRenormOpName || common::AnfAlgo::HasNodeAttr(kAttrVisited, cnode)) {
    return nullptr;
  }
  common::AnfAlgo::SetNodeAttr(kAttrVisited, MakeValue(true), node);

  CheckCNodeInputSize(cnode, 1);
  auto renorm_input = cnode->input(1);
  auto in_shape = common::AnfAlgo::GetPrevNodeOutputInferShape(node, 0);
  auto type = common::AnfAlgo::GetOutputInferDataType(node, 0);
  FreshRenormInferShape(cnode, in_shape, type);
  if (common::AnfAlgo::IsDynamicShape(cnode)) {
    common::AnfAlgo::SetNodeAttr(kAttrRecomputeShape, MakeValue(true), cnode);
  }
  auto shape_node = opt::CreateValueNodeWithKernelInfo(func_graph, MakeValue(in_shape));
  std::vector<AnfNodePtr> broadcast_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimBroadcastTo->name())),
                                              node, shape_node};
  auto broadcast_node = NewCNode(broadcast_inputs, func_graph);
  MS_EXCEPTION_IF_NULL(broadcast_node);
  common::AnfAlgo::SetOutputInferTypeAndShape({type}, {in_shape}, broadcast_node.get());
  broadcast_node->set_scope(node->scope());

  std::vector<AnfNodePtr> mul_inputs = {NewValueNode(std::make_shared<Primitive>(prim::kPrimMul->name())),
                                        broadcast_node, renorm_input};
  auto mul_node = func_graph->NewCNode(mul_inputs);
  MS_EXCEPTION_IF_NULL(mul_node);
  common::AnfAlgo::SetOutputInferTypeAndShape({type}, {in_shape}, mul_node.get());
  mul_node->set_scope(node->scope());
  return mul_node;
}
}  // namespace opt
}  // namespace mindspore
