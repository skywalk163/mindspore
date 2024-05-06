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
#include "plugin/device/gpu/optimizer/batch_norm_relu_fusion.h"

#include <memory>
#include <string>
#include <vector>

#include "include/backend/anf_runtime_algorithm.h"
#include "include/backend/optimizer/helper.h"
#include "include/common/utils/anfalgo.h"
#include "include/common/utils/utils.h"
#include "ir/primitive.h"
#include "kernel/graph_kernel_info.h"
#include "mindspore/core/ops/nn_ops.h"
#include "mindspore/core/ops/nn_optimizer_ops.h"
#include "mindspore/core/ops/sequence_ops.h"
#include "ops/op_name.h"
#include "ops/op_utils.h"
#include "plugin/device/gpu/hal/device/kernel_info_setter.h"

namespace mindspore {
namespace opt {
const BaseRef BatchNormReluFusion::DefinePattern() const {
  VectorRef batch_norm =
    VectorRef({prim::kPrimBatchNorm, x_, scale_, bias_, mean_, var_, is_training_, eps_, momentum_, format_, umonad_});
  VectorRef tuple_get = VectorRef({prim::kPrimTupleGetItem, batch_norm, index_});
  VectorRef relu = VectorRef({prim::kPrimReLU, tuple_get});
  return relu;
}

const AnfNodePtr BatchNormReluFusion::Process(const FuncGraphPtr &graph, const AnfNodePtr &node,
                                              const EquivPtr &) const {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(node);

  auto tuple_get_item = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), 0);
  MS_EXCEPTION_IF_NULL(tuple_get_item);

  // Only fuse output[0] of BatchNorm with ReLU
  size_t output_index = common::AnfAlgo::GetTupleGetItemOutIndex(utils::cast<CNodePtr>(tuple_get_item));
  if (output_index != 0) {
    return nullptr;
  }

  auto outlist = GetRealNodeUsedList(graph, tuple_get_item);
  // If output[0] of BatchNorm is used by more than one CNode, fusing BatchNorm+ReLU will affect the result of
  // BatchNorm's user node.
  const size_t node_user_num = 1;
  if (outlist->size() != node_user_num) {
    return nullptr;
  }

  auto batch_norm = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(tuple_get_item), 0);
  MS_EXCEPTION_IF_NULL(batch_norm);

  auto kernel_name = common::AnfAlgo::GetCNodeName(batch_norm);
  size_t is_train_idx = ops::GetInputIndexByName(kernel_name, "is_training");
  size_t format_idx = ops::GetInputIndexByName(kernel_name, "data_format");
  if (is_train_idx == SIZE_MAX || format_idx == SIZE_MAX) {
    return nullptr;
  }
  auto is_train_input_node = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), is_train_idx);
  auto format_input_node = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), format_idx);
  if (!utils::isa<ValueNodePtr>(is_train_input_node) || !utils::isa<ValueNodePtr>(format_input_node)) {
    return nullptr;
  }
  auto is_train_v = ops::GetScalarValue<bool>(is_train_input_node->cast<ValueNodePtr>()->value());
  if (!is_train_v.has_value() || !is_train_v.value()) {
    return nullptr;
  }

  auto format_v = ops::GetScalarValue<int64_t>(format_input_node->cast<ValueNodePtr>()->value());
  if (!format_v.has_value()) {
    return nullptr;
  }
  if (AnfAlgo::GetInputFormat(batch_norm, 0) != kOpFormat_NHWC && format_v.value() != Format::NHWC) {
    return nullptr;
  }
  auto shape = AnfAlgo::GetInputDeviceShape(batch_norm, 0);
  if (shape.back() % kBNChannelMultipleFactor != 0) {
    return nullptr;
  }

  auto x = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex0);
  auto scale = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex1);
  auto bias = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex2);
  auto mean = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex3);
  auto var = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex4);
  auto is_train = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex5);
  auto eps = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex6);
  auto momentum = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex7);
  auto format = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex8);
  auto umonad = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex9);

  MS_EXCEPTION_IF_NULL(x);
  MS_EXCEPTION_IF_NULL(scale);
  MS_EXCEPTION_IF_NULL(bias);
  MS_EXCEPTION_IF_NULL(mean);
  MS_EXCEPTION_IF_NULL(var);
  MS_EXCEPTION_IF_NULL(is_train);
  MS_EXCEPTION_IF_NULL(eps);
  MS_EXCEPTION_IF_NULL(momentum);
  MS_EXCEPTION_IF_NULL(format);
  MS_EXCEPTION_IF_NULL(umonad);

  auto prim = std::make_shared<Primitive>(kBatchNormWithActivationOpName);
  MS_EXCEPTION_IF_NULL(prim);
  prim->AddAttr(mindspore::ops::kActivationType, MakeValue(static_cast<int64_t>(mindspore::ActivationType::RELU)));
  std::vector<AnfNodePtr> inputs = {NewValueNode(prim), x,   scale,    bias,   mean,  var,
                                    is_train,           eps, momentum, format, umonad};
  auto fused_batch_norm_with_relu = graph->NewCNode(inputs);
  MS_EXCEPTION_IF_NULL(fused_batch_norm_with_relu);

  std::vector<TypeId> outputs_type;
  std::vector<BaseShapePtr> outputs_shape;
  auto output_num = AnfAlgo::GetOutputTensorNum(batch_norm);
  for (size_t i = 0; i < output_num; i++) {
    outputs_type.push_back(common::AnfAlgo::GetOutputInferDataType(batch_norm, i));
    outputs_shape.push_back(AnfAlgo::GetOutputDetailShape(batch_norm, i));
  }
  common::AnfAlgo::SetOutputTypeAndDetailShape(outputs_type, outputs_shape, fused_batch_norm_with_relu.get());
  common::AnfAlgo::CopyNodeAttrs(batch_norm, fused_batch_norm_with_relu);

  auto manager = graph->manager();
  MS_EXCEPTION_IF_NULL(manager);
  if (!manager->Replace(batch_norm, fused_batch_norm_with_relu)) {
    MS_LOG(EXCEPTION) << "manager replace node failed in batchnorm relu fusion.";
  }
  auto kernel_info_setter = GraphKernelInfoManager::Instance().GetGraphKernelInfo(kGPUDevice);
  kernel_info_setter->SetKernelInfo(fused_batch_norm_with_relu, KernelType::UNKNOWN_KERNEL_TYPE);
  return tuple_get_item;
}
}  // namespace opt
}  // namespace mindspore
