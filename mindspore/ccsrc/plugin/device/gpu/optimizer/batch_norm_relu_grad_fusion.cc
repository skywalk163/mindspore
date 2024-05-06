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
#include "plugin/device/gpu/optimizer/batch_norm_relu_grad_fusion.h"

#include <memory>
#include <vector>
#include <string>

#include "ops/nn_optimizer_ops.h"
#include "ops/nn_ops.h"
#include "ops/op_utils.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "ir/primitive.h"
#include "include/common/utils/utils.h"
#include "include/backend/optimizer/helper.h"
#include "plugin/device/gpu/hal/device/kernel_info_setter.h"
#include "utils/ms_context.h"
#include "kernel/graph_kernel_info.h"
#include "ops/op_name.h"

namespace mindspore {
namespace opt {
const BaseRef BatchNormReluGradFusion::DefinePattern() const {
  VectorRef relu_grad = VectorRef({prim::kPrimReluGrad, dy_, y_});
  VectorRef batch_norm_grad = VectorRef(
    {prim::kPrimBatchNormGrad, relu_grad, x_, scale_, save_mean_, save_var_, reserve_, is_training_, eps_, format_});
  return batch_norm_grad;
}

const AnfNodePtr BatchNormReluGradFusion::Process(const FuncGraphPtr &graph, const AnfNodePtr &node,
                                                  const EquivPtr &) const {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(node);
  auto kernel_name = common::AnfAlgo::GetCNodeName(node);
  size_t is_train_idx = ops::GetInputIndexByName(kernel_name, "is_training");
  size_t format_idx = ops::GetInputIndexByName(kernel_name, "data_format");
  if (is_train_idx == SIZE_MAX || format_idx == SIZE_MAX) {
    return nullptr;
  }
  auto is_train_input_node = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), is_train_idx);
  auto format_input_node = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), format_idx);
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
  if (AnfAlgo::GetInputFormat(node, 0) != kOpFormat_NHWC && format_v.value() != Format::NHWC) {
    return nullptr;
  }
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  if (ms_context->get_param<int>(MS_CTX_EXECUTION_MODE) == kPynativeMode) {
    return nullptr;
  }
  auto shape = AnfAlgo::GetInputDeviceShape(node, 0);
  if (shape.back() % kBNChannelMultipleFactor != 0) {
    return nullptr;
  }

  auto relu_grad = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), 0);
  MS_EXCEPTION_IF_NULL(relu_grad);

  auto outlist = GetRealNodeUsedList(graph, relu_grad);
  const size_t node_user_num_upper_bound = 2;
  if (outlist->size() >= node_user_num_upper_bound) {
    return nullptr;
  }

  auto dy = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(relu_grad), kIndex0);
  MS_EXCEPTION_IF_NULL(dy);
  auto y = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(relu_grad), kIndex1);
  MS_EXCEPTION_IF_NULL(y);
  auto x = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), kIndex1);
  MS_EXCEPTION_IF_NULL(x);
  auto scale = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), kIndex2);
  MS_EXCEPTION_IF_NULL(scale);
  auto save_mean = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), kIndex3);
  MS_EXCEPTION_IF_NULL(save_mean);
  auto save_var = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), kIndex4);
  MS_EXCEPTION_IF_NULL(save_var);
  auto reserve = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), kIndex5);
  MS_EXCEPTION_IF_NULL(reserve);
  auto is_train = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), kIndex6);
  MS_EXCEPTION_IF_NULL(is_train);
  auto eps = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), kIndex7);
  MS_EXCEPTION_IF_NULL(eps);
  auto format = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(node), kIndex8);
  MS_EXCEPTION_IF_NULL(format);
  auto batch_norm = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(save_mean), kIndex0);
  MS_EXCEPTION_IF_NULL(batch_norm);
  auto bias = common::AnfAlgo::GetInputNode(utils::cast<CNodePtr>(batch_norm), kIndex2);
  MS_EXCEPTION_IF_NULL(bias);

  auto prim = std::make_shared<Primitive>(kBatchNormGradWithActivationOpName);
  MS_EXCEPTION_IF_NULL(prim);
  prim->AddAttr(mindspore::ops::kActivationType, MakeValue(static_cast<int64_t>(mindspore::ActivationType::RELU)));
  std::vector<AnfNodePtr> inputs = {NewValueNode(prim), dy,  x,     scale, save_mean, save_var, reserve, bias, y,
                                    is_train,           eps, format};
  auto fused_batch_norm_grad_with_relu = graph->NewCNode(inputs);
  MS_EXCEPTION_IF_NULL(fused_batch_norm_grad_with_relu);

  std::vector<TypeId> outputs_type;
  std::vector<BaseShapePtr> outputs_shape;
  auto output_num = AnfAlgo::GetOutputTensorNum(node);
  for (size_t i = 0; i < output_num; i++) {
    outputs_type.push_back(common::AnfAlgo::GetOutputInferDataType(node, i));
    outputs_shape.push_back(AnfAlgo::GetOutputDetailShape(node, i));
  }
  common::AnfAlgo::SetOutputTypeAndDetailShape(outputs_type, outputs_shape, fused_batch_norm_grad_with_relu.get());
  common::AnfAlgo::CopyNodeAttrs(node, fused_batch_norm_grad_with_relu);
  auto kernel_info_setter = GraphKernelInfoManager::Instance().GetGraphKernelInfo(kGPUDevice);
  kernel_info_setter->SetKernelInfo(fused_batch_norm_grad_with_relu, KernelType::UNKNOWN_KERNEL_TYPE);
  return fused_batch_norm_grad_with_relu;
}
}  // namespace opt
}  // namespace mindspore
