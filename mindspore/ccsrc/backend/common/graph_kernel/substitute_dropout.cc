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
#include "backend/common/graph_kernel/substitute_dropout.h"

#include "ops/array_ops.h"
#include "mindspore/core/ops/nn_ops.h"
#include "mindspore/core/ops/op_utils.h"
#include "include/common/utils/utils.h"
#include "include/backend/optimizer/helper.h"
#include "backend/common/graph_kernel/graph_kernel_helper.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "ir/tensor.h"
#include "kernel/kernel_build_info.h"
#include "include/backend/kernel_info.h"

namespace mindspore {
namespace graphkernel {
using opt::CheckCNodeInputSize;
using opt::kDropoutInputTensorNum;

int64_t DropoutExpanderDeco::seed_ = time(nullptr);

AnfNodePtr DropoutExpanderDeco::Run(const AnfNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  CNodePtr cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  auto func_graph = node->func_graph();
  CheckCNodeInputSize(cnode, kDropoutInputTensorNum);
  auto shape = AnfAlgo::GetInputDeviceShape(cnode, 0);
  // Get seed from original dropout's attrs, rather than set seed by time.
  // Only seed0 and seed1 are all equal to 0, then set seed = time.
  int64_t seed = ops::GetScalarValue<int64_t>(cnode->input(kIndex3)->cast<ValueNodePtr>()->value()).value();
  if (seed == 0) {
    seed = ops::GetScalarValue<int64_t>(cnode->input(kIndex4)->cast<ValueNodePtr>()->value()).value();
    if (seed == 0) {
      seed = seed_++;
    }
  }
  // Create a uniform_real kernel to generate random value.
  AnfNodePtr uniform_real_shape;
  if (IsDynamic(shape)) {
    uniform_real_shape = func_graph->NewCNode(prim::kPrimTensorShape, {cnode->input(1)});
    int64_t rank = IsDynamicRank(shape) ? -1 : static_cast<int64_t>(shape.size());
    uniform_real_shape->set_abstract(std::make_shared<abstract::AbstractTensor>(kInt64, ShapeVector{rank}));
    Callback::Instance()->ResetKernelInfo(uniform_real_shape);
  } else {
    auto tensor = std::make_shared<tensor::Tensor>(kNumberTypeInt64, ShapeVector(1, SizeToLong(shape.size())),
                                                   static_cast<void *>(&shape[0]), kNumberTypeInt64);
    uniform_real_shape = NewValueNode(tensor);
    uniform_real_shape->set_abstract(tensor->ToAbstract());
    uniform_real_shape->set_kernel_info(std::make_shared<device::KernelInfo>());
  }
  AnfNodePtrList uniform_real_input = {NewValueNode(prim::kPrimCudnnUniformReal), uniform_real_shape};
  auto uniform_real_node = func_graph->NewCNode(uniform_real_input);
  SetNodeAttrSafely("seed", MakeValue(seed), uniform_real_node);
  common::AnfAlgo::SetNodeAttr("seed2", MakeValue(static_cast<int64_t>(0)), uniform_real_node);
  uniform_real_node->set_abstract(std::make_shared<abstract::AbstractTensor>(kFloat32, shape));
  Callback::Instance()->ResetKernelInfo(uniform_real_node);

  // Create a GKDropout node with uniform_real as its second input.
  AnfNodePtrList gkdropout_inputs = {NewValueNode(std::make_shared<Primitive>("GkDropout")), cnode->input(1),
                                     uniform_real_node};
  auto new_dropout_node = func_graph->NewCNode(gkdropout_inputs);
  SetNodeAttrSafely("keep_prob", cnode->input(kIndex2)->cast<ValueNodePtr>()->value(), new_dropout_node);
  // the output info is unchanged.
  new_dropout_node->set_abstract(node->abstract());
  auto old_kernel_info = AnfAlgo::GetSelectKernelBuildInfo(node);
  auto dropout_kernel_info_builder = std::make_shared<kernel::KernelBuildInfo::KernelBuildInfoBuilder>(old_kernel_info);
  dropout_kernel_info_builder->SetInputsFormat({old_kernel_info->GetInputFormat(0), kOpFormat_DEFAULT});
  dropout_kernel_info_builder->SetInputsDeviceType({old_kernel_info->GetInputDeviceType(0), kNumberTypeFloat32});
  AnfAlgo::SetSelectKernelBuildInfo(dropout_kernel_info_builder->Build(), new_dropout_node.get());
  return decorated_->Run(new_dropout_node);
}
}  // namespace graphkernel
}  // namespace mindspore
