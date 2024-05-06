/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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
#include "backend/common/pass/const_input_to_attr.h"

#include <vector>
#include "include/backend/optimizer/helper.h"
#include "include/common/utils/utils.h"
#include "ops/array_ops.h"
#include "ops/ascend_op_name.h"
#include "ops/conv_pool_ops.h"
#include "ops/framework_ops.h"
#include "ops/image_ops.h"
#include "ops/lite_op_name.h"
#include "ops/math_ops.h"
#include "ops/nn_ops.h"
#include "ops/nn_optimizer_op_name.h"
#include "ops/sparse_ops.h"
#include "ops/auto_generate/gen_ops_primitive.h"
#include "ops/op_utils.h"
#include "utils/anf_utils.h"
#include "utils/log_adapter.h"
#include "include/common/utils/anfalgo.h"

namespace mindspore {
namespace opt {
ConstInputToAttrInfoRegistry::ConstInputToAttrInfoRegistry() {
  Register(prim::kPrimCast->name(), {1});
  Register(prim::kPrimAvgPoolGradVm->name(), {0});
  Register(prim::kPrimAvgPool3DGrad->name(), {0});
  Register(kConv2DTransposeOpName, {2});
  Register(prim::kPrimConv3DTranspose->name(), {2});
  Register(prim::kPrimConv2DBackpropInput->name(), {2});
  Register(prim::kPrimParallelResizeBilinearGrad->name(), {2});
  Register(prim::kPrimConv2DBackpropFilter->name(), {2});
  Register(prim::kPrimConv3DBackpropInput->name(), {2});
  Register(prim::kPrimConv3DBackpropFilter->name(), {2});
  Register(prim::kPrimDepthwiseConv2dNativeBackpropFilter->name(), {1});
  Register(prim::kPrimDepthwiseConv2dNativeBackpropInput->name(), {0});
  Register(prim::kPrimReshape->name(), {1});
  Register(prim::kPrimReduceMax->name(), {1});
  Register(prim::kPrimReduceMin->name(), {1});
  Register(prim::kPrimReduceProd->name(), {1});
  Register(prim::kPrimReduceSum->name(), {1});
  Register(prim::kPrimArgminV2->name(), {1});
  Register(prim::kPrimReduceMean->name(), {1});
  Register(prim::kPrimCentralization->name(), {1});
  Register(prim::kPrimGather->name(), {2});
  Register(prim::kPrimGatherD->name(), {1});
  Register(prim::kPrimEmbeddingLookup->name(), {2, 3, 4, 5});
  Register(prim::kPrimEmbeddingLookupCommGrad->name(), {1});
  Register(prim::kPrimSubscalar->name(), {1});
  Register(prim::kPrimTranspose->name(), {1});
  Register(prim::kPrimUnsortedSegmentSum->name(), {2});
  Register(prim::kPrimOneHot->name(), {1});
  Register(prim::kPrimConcat->name(), {0});
  Register(prim::kPrimCumSum->name(), {1});
  Register(prim::kPrimCumProd->name(), {1});
  Register(prim::kPrimReduceAll->name(), {1});
  Register(prim::kPrimReduceAny->name(), {1});
  Register(prim::kPrimUnsortedSegmentMin->name(), {2});
  Register(prim::kPrimUnsortedSegmentMax->name(), {2});
  Register(prim::kPrimCSRReduceSum->name(), {3, 4});
  Register(prim::kPrimCSRMV->name(), {3});
  Register(prim::kPrimCSRMM->name(), {3});
  Register(prim::kPrimCSRMul->name(), {3});
  Register(prim::kPrimCSRDiv->name(), {3});
  Register(prim::kPrimCSRGather->name(), {3});
  Register(prim::kPrimCSR2COO->name(), {1});
  Register(prim::kPrimCOO2CSR->name(), {1});
  Register(prim::kPrimInplaceUpdateV2->name(), {1});
  Register(kSparseGatherV2OpName, {2});
  Register(kUnsortedSegmentProdOpName, {2});
  Register(kSimpleMeanGradOpName, {1});
  Register(kMeanGradOpName, {1});
  Register(kSliceOpName, {1, 2});
  Register(kSliceGradOpName, {2, 3});
  Register(kTileOpName, {1});
  Register(kScatterNdOpName, {2});
  Register(kStridedSliceAssignOpName, {1, 2, 3});
  Register(kStridedSliceOpName, {1, 2, 3});
  Register(kEyeOpName, {0, 1, 2});
  Register(kStridedSliceGradOpName, {1, 2, 3, 4});
  Register(kTensorCopySlicesOpName, {2, 3, 4});
  Register(kFlattenGradOpName, {1});
  Register(kExpandDimsOpName, {1});
  Register(kSplitOpName, {0});
  Register(kErfOpName, {1});
  Register(kSparseApplyAdagradOpName, {2});
  Register(kResizeNearestNeighborGradOpName, {1});
  Register(kApplyRMSPropOpName, {5, 6, 7});
  Register(kReduceProdOpName, {1});
  Register(kCumprodOpName, {1});
  Register(kSpaceToBatchOpName, {1});
  Register(kBatchToSpaceOpName, {1});
  Register(kPadOpName, {1});
  Register(kPushOpName, {1});
  Register(kPullWeightOpName, {1, 2});
  Register(kPushWeightOpName, {1, 2});
}

ConstInputToAttrInfoRegistry &ConstInputToAttrInfoRegistry::Instance() {
  static ConstInputToAttrInfoRegistry instance{};
  return instance;
}

void ConstInputToAttrInfoRegistry::Register(const ConstInputToAttrInfoRegister &reg) {
  auto op_name = reg.GetOpName();
  if (op_input_to_attr_map_.find(op_name) == op_input_to_attr_map_.end()) {
    (void)op_input_to_attr_map_.emplace(op_name, reg);
    MS_LOG(DEBUG) << op_name << " const2attr register successfully!";
  }
}

void ConstInputToAttrInfoRegistry::Register(const std::string &op_name,
                                            const mindspore::HashSet<size_t> &input_attr_set) {
  if (op_input_to_attr_map_.find(op_name) == op_input_to_attr_map_.end()) {
    ConstInputToAttrInfoRegister reg(op_name);
    (void)reg.SetConstInputToAttr(input_attr_set);
    (void)op_input_to_attr_map_.emplace(op_name, reg);
    MS_LOG(DEBUG) << op_name << " const2attr register successfully!";
  }
}

bool ConstInputToAttrInfoRegistry::GetRegisterByOpName(const std::string &op_name,
                                                       ConstInputToAttrInfoRegister *reg) const {
  if (op_input_to_attr_map_.find(op_name) != op_input_to_attr_map_.end()) {
    *reg = op_input_to_attr_map_.at(op_name);
    MS_LOG(DEBUG) << op_name << " const2attr find in registry.";
    return true;
  }
  return false;
}

CNodePtr ConstInputToAttr(const CNodePtr &cnode, const mindspore::HashSet<size_t> &input_attrs) {
  MS_EXCEPTION_IF_NULL(cnode);
  std::vector<AnfNodePtr> new_inputs;
  auto primitive = GetCNodePrimitive(cnode);
  MS_EXCEPTION_IF_NULL(primitive);
  primitive = primitive->Clone();
  auto inputs = cnode->inputs();
  new_inputs.push_back(inputs[0]);
  bool need_update = false;
  auto input_names = primitive->GetAttr(kAttrInputNames);
  std::vector<std::string> input_names_vec;
  if (input_names != nullptr) {
    input_names_vec = GetValue<std::vector<std::string>>(input_names);
  }
  auto op_name = common::AnfAlgo::GetCNodeName(cnode);
  for (size_t i = 0; i < inputs.size() - 1; ++i) {
    auto input_node = inputs[i + 1];
    MS_EXCEPTION_IF_NULL(input_node);
    if (IsPrimitiveCNode(input_node, prim::kPrimDepend)) {
      input_node = AnfUtils::VisitKernel(input_node, 0).first;
    }
    if (input_attrs.find(i) != input_attrs.end() && input_node->isa<ValueNode>() && !HasAbstractMonad(input_node)) {
      auto input_name = ops::GetInputNameByIndex(op_name, i);
      if (input_name == "") {
        // operators that are not developed by yaml
        if (input_names_vec.empty()) {
          MS_LOG(DEBUG) << "input_names are nullptr in cnode[" + cnode->DebugString() + "]";
          return cnode;
        }
        if (i >= input_names_vec.size()) {
          MS_LOG(EXCEPTION) << "Index " << i << " is larger than input names size [" << input_names_vec.size() << "]";
        }
        input_name = input_names_vec[i];
      }
      auto value_node = input_node->cast<ValueNodePtr>();
      MS_EXCEPTION_IF_NULL(value_node);
      MS_LOG(DEBUG) << "start erase input[" << i << "] of cnode[" + cnode->DebugString() + "]";
      auto value = value_node->value();
      if (value->isa<tensor::Tensor>()) {
        auto tensor = value->cast<tensor::TensorPtr>();
        if (tensor->data().const_data() == nullptr && !tensor->has_user_data(kTensorValueIsEmpty)) {
          need_update = false;
          break;
        }
      }
      primitive->set_attr(input_name, value);
      need_update = true;
    } else {
      new_inputs.push_back(inputs[i + 1]);
    }
  }
  if (need_update) {
    // Update cnode's inputs
    new_inputs[0] = NewValueNode(primitive);
    auto graph = cnode->func_graph();
    MS_EXCEPTION_IF_NULL(graph);
    auto new_cnode = NewCNode(new_inputs, graph, {cnode});
    MS_EXCEPTION_IF_NULL(new_cnode);
    new_cnode->set_abstract(cnode->abstract());
    new_cnode->set_scope(cnode->scope());
    new_cnode->set_primal_attrs(cnode->primal_attrs());
    new_cnode->set_attrs(cnode->attrs());
    auto kernel_graph = graph->cast<KernelGraphPtr>();
    if (kernel_graph != nullptr) {
      kernel_graph->FrontBackendlMapUpdate(cnode, new_cnode);
    }
    return new_cnode;
  }
  return cnode;
}
}  // namespace opt
}  // namespace mindspore
