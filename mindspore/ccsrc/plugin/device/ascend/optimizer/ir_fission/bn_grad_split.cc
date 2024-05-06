/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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
#include "plugin/device/ascend/optimizer/ir_fission/bn_grad_split.h"

#include <memory>
#include <vector>

#include "include/backend/anf_runtime_algorithm.h"
#include "include/backend/kernel_info.h"
#include "include/backend/optimizer/helper.h"
#include "include/common/utils/anfalgo.h"
#include "include/common/utils/utils.h"
#include "mindapi/base/format.h"
#include "ops/nn_ops.h"
#include "ops/op_utils.h"
#include "ops/sequence_ops.h"
#include "plugin/device/ascend/optimizer/ir_fission/bn_split.h"
#include "plugin/device/ascend/optimizer/get_value_helper.h"
#include "utils/ms_context.h"
#include "utils/trace_base.h"

namespace mindspore {
namespace opt {
namespace {
constexpr size_t kIdxIsTrain = 7;
constexpr size_t kIdxEpsilon = 8;
constexpr size_t kIdxFormat = 9;
}  // namespace
void BnGradSplit::CreateOutputsOfUpdateGrad(const FuncGraphPtr &graph, const CNodePtr &bn_grad_node,
                                            std::vector<AnfNodePtr> *bn_update_grad_outputs, bool is_dynamic) const {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(bn_grad_node);
  auto bn_grad_inputs = bn_grad_node->inputs();
  CheckCNodeInputSize(bn_grad_node, kBNGradInputTensorNum);
  std::vector<AnfNodePtr> bn_update_grad_inputs = {
    NewValueNode(std::make_shared<Primitive>(kBNTrainingUpdateGradOpName)), bn_grad_inputs[kIndex1],
    bn_grad_inputs[kIndex2], bn_grad_inputs[kIndex4], bn_grad_inputs[kIndex5]};
  auto bn_update_grad = NewCNode(bn_update_grad_inputs, graph);
  MS_EXCEPTION_IF_NULL(bn_update_grad);
  bn_update_grad->set_kernel_info(std::make_shared<device::KernelInfo>());
  bn_update_grad->set_scope(bn_grad_node->scope());

  auto types = {common::AnfAlgo::GetOutputInferDataType(bn_grad_node, 1),
                common::AnfAlgo::GetOutputInferDataType(bn_grad_node, 2)};
  auto shapes = {AnfAlgo::GetOutputDetailShape(bn_grad_node, 1), AnfAlgo::GetOutputDetailShape(bn_grad_node, 2)};
  common::AnfAlgo::SetOutputTypeAndDetailShape(types, shapes, bn_update_grad.get());

  auto epsilon = GetNodeScalarValue<float>(bn_grad_inputs[kIdxEpsilon]);
  common::AnfAlgo::SetNodeAttr(kAttrEpsilon, MakeValue(epsilon), bn_update_grad);

  auto format = GetNodeFormatValue(bn_grad_inputs[kIdxFormat]);
  common::AnfAlgo::SetNodeAttr(kAttrFormat, MakeValue(format), bn_update_grad);

  if (is_dynamic) {
    common::AnfAlgo::SetNodeAttr(kAttrInputIsDynamicShape, MakeValue(true), bn_update_grad);
  }
  CreateMultipleOutputsOfAnfNode(graph, bn_update_grad, kBNTrainingUpdateGradOutputNum, bn_update_grad_outputs);
}

void BnGradSplit::CreateOutputsOfReduceGrad(const FuncGraphPtr &graph, const CNodePtr &bn_grad_node,
                                            const std::vector<AnfNodePtr> &bn_update_grad_outputs,
                                            std::vector<AnfNodePtr> *bn_reduce_grad_outputs, bool is_dynamic) const {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(bn_grad_node);
  auto bn_grad_inputs = bn_grad_node->inputs();
  CheckCNodeInputSize(bn_grad_node, kBNGradInputTensorNum);
  if (bn_update_grad_outputs.size() != kBNTrainingUpdateGradOutputNum) {
    MS_LOG(INTERNAL_EXCEPTION) << "Outputs of bn_update_grad has wrong size, should be "
                               << kBNTrainingUpdateGradOutputNum << ", but got " << bn_update_grad_outputs.size()
                               << trace::DumpSourceLines(bn_grad_node);
  }
  std::vector<AnfNodePtr> bn_reduce_grad_inputs = {
    NewValueNode(std::make_shared<Primitive>(kBNTrainingReduceGradOpName)),
    bn_grad_inputs[kIndex1],
    bn_grad_inputs[kIndex2],
    bn_update_grad_outputs[kIndex0],
    bn_update_grad_outputs[kIndex1],
    bn_grad_inputs[kIndex3],
    bn_grad_inputs[kIndex4],
    bn_grad_inputs[kIndex5]};
  auto bn_reduce_grad = NewCNode(bn_reduce_grad_inputs, graph);
  MS_EXCEPTION_IF_NULL(bn_reduce_grad);
  bn_reduce_grad->set_kernel_info(std::make_shared<device::KernelInfo>());
  bn_reduce_grad->set_scope(bn_grad_node->scope());

  auto types = {common::AnfAlgo::GetOutputInferDataType(bn_grad_node, 0)};
  auto shapes = {AnfAlgo::GetOutputDetailShape(bn_grad_node, 0)};
  common::AnfAlgo::SetOutputTypeAndDetailShape(types, shapes, bn_reduce_grad.get());

  auto epsilon = GetNodeScalarValue<float>(bn_grad_inputs[kIdxEpsilon]);
  common::AnfAlgo::SetNodeAttr(kAttrEpsilon, MakeValue(epsilon), bn_reduce_grad);

  auto format = GetNodeFormatValue(bn_grad_inputs[kIdxFormat]);
  common::AnfAlgo::SetNodeAttr(kAttrFormat, MakeValue(format), bn_reduce_grad);

  if (is_dynamic) {
    common::AnfAlgo::SetNodeAttr(kAttrInputIsDynamicShape, MakeValue(true), bn_reduce_grad);
    common::AnfAlgo::SetNodeAttr(kAttrOutputIsDynamicShape, MakeValue(true), bn_reduce_grad);
  }
  (*bn_reduce_grad_outputs).push_back(bn_reduce_grad);
}

CNodePtr BnGradSplit::BNGradSplitForTBE(const FuncGraphPtr &func_graph, const CNodePtr &cnode) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  bool is_dynamic = common::AnfAlgo::IsDynamicShape(cnode);
  std::vector<AnfNodePtr> bn_update_grad_outputs;
  CreateOutputsOfUpdateGrad(func_graph, cnode, &bn_update_grad_outputs, is_dynamic);
  if (bn_update_grad_outputs.size() != kBNTrainingUpdateGradOutputNum) {
    MS_LOG(INTERNAL_EXCEPTION) << "Outputs of bn_update_grad has wrong size, should be "
                               << kBNTrainingUpdateGradOutputNum << ", but got " << bn_update_grad_outputs.size()
                               << trace::DumpSourceLines(cnode);
  }

  std::vector<AnfNodePtr> bn_reduce_grad_outputs;
  CreateOutputsOfReduceGrad(func_graph, cnode, bn_update_grad_outputs, &bn_reduce_grad_outputs, is_dynamic);
  if (bn_reduce_grad_outputs.size() != 1) {
    MS_LOG(INTERNAL_EXCEPTION) << "Outputs of bn_reduce_grad has wrong size, should be " << 1 << ", but got "
                               << bn_reduce_grad_outputs.size() << trace::DumpSourceLines(cnode);
  }

  std::vector<AnfNodePtr> make_tuple_inputs = {NewValueNode(prim::kPrimMakeTuple), bn_reduce_grad_outputs[0],
                                               bn_update_grad_outputs[0], bn_update_grad_outputs[1]};
  AbstractBasePtrList make_tuple_abstract = {bn_reduce_grad_outputs[0]->abstract(),
                                             bn_update_grad_outputs[0]->abstract(),
                                             bn_update_grad_outputs[1]->abstract()};
  if (func_graph->has_flag(kAttrMutableKernel)) {
    // The output number of the BatchNormGrad op is 5, so if it runs
    // in a single op graph, the output number needs to be 5.
    auto none1 = NewValueNode(std::make_shared<None>());
    MS_EXCEPTION_IF_NULL(none1);
    auto none2 = NewValueNode(std::make_shared<None>());
    MS_EXCEPTION_IF_NULL(none2);
    none1->set_abstract(std::make_shared<abstract::AbstractNone>());
    none2->set_abstract(std::make_shared<abstract::AbstractNone>());
    make_tuple_inputs.push_back(none1);
    make_tuple_inputs.push_back(none2);
    make_tuple_abstract.push_back(none1->abstract());
    make_tuple_abstract.push_back(none2->abstract());
  }
  auto make_tuple = func_graph->NewCNode(make_tuple_inputs);
  MS_EXCEPTION_IF_NULL(make_tuple);
  make_tuple->set_scope(cnode->scope());
  make_tuple->set_abstract(std::make_shared<abstract::AbstractTuple>(make_tuple_abstract));
  return make_tuple;
}

void SyncBnGradSplit::CreateOutputsOfUpdateGrad(const FuncGraphPtr &graph, const CNodePtr &bn_grad_node,
                                                std::vector<AnfNodePtr> *bn_update_grad_outputs,
                                                bool is_dynamic) const {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(bn_grad_node);
  auto bn_grad_inputs = bn_grad_node->inputs();
  CheckCNodeInputSize(bn_grad_node, kSyncBNGradInputTensorNum);
  std::vector<AnfNodePtr> bn_update_grad_inputs = {
    NewValueNode(std::make_shared<Primitive>(kBNTrainingUpdateGradOpName)), bn_grad_inputs[kIndex1],
    bn_grad_inputs[kIndex2], bn_grad_inputs[kIndex4], bn_grad_inputs[kIndex5]};
  auto bn_update_grad = NewCNode(bn_update_grad_inputs, graph);
  MS_EXCEPTION_IF_NULL(bn_update_grad);
  bn_update_grad->set_kernel_info(std::make_shared<device::KernelInfo>());
  bn_update_grad->set_scope(bn_grad_node->scope());

  auto types = {common::AnfAlgo::GetOutputInferDataType(bn_grad_node, 1),
                common::AnfAlgo::GetOutputInferDataType(bn_grad_node, 2)};
  auto shapes = {AnfAlgo::GetOutputDetailShape(bn_grad_node, 1), AnfAlgo::GetOutputDetailShape(bn_grad_node, 2)};
  common::AnfAlgo::SetOutputTypeAndDetailShape(types, shapes, bn_update_grad.get());
  common::AnfAlgo::CopyNodeAttr(kAttrEpsilon, bn_grad_node, bn_update_grad);
  if (common::AnfAlgo::HasNodeAttr(kAttrFormat, bn_grad_node)) {
    common::AnfAlgo::CopyNodeAttr(kAttrFormat, bn_grad_node, bn_update_grad);
  } else {
    common::AnfAlgo::SetNodeAttr(kAttrFormat, MakeValue(kOpFormat_NCHW), bn_update_grad);
  }
  if (is_dynamic) {
    common::AnfAlgo::SetNodeAttr(kAttrInputIsDynamicShape, MakeValue(true), bn_update_grad);
  }
  CreateMultipleOutputsOfAnfNode(graph, bn_update_grad, kBNTrainingUpdateGradOutputNum, bn_update_grad_outputs);
}

void SyncBnGradSplit::CreateOutputsOfReduceGrad(const FuncGraphPtr &graph, const CNodePtr &bn_grad_node,
                                                const std::vector<AnfNodePtr> &bn_update_grad_outputs,
                                                std::vector<AnfNodePtr> *bn_reduce_grad_outputs,
                                                bool is_dynamic) const {
  MS_EXCEPTION_IF_NULL(graph);
  MS_EXCEPTION_IF_NULL(bn_grad_node);
  auto bn_grad_inputs = bn_grad_node->inputs();
  CheckCNodeInputSize(bn_grad_node, kSyncBNGradInputTensorNum);
  if (bn_update_grad_outputs.size() != kBNTrainingUpdateGradOutputNum) {
    MS_LOG(INTERNAL_EXCEPTION) << "Outputs of bn_update_grad has wrong size, should be "
                               << kBNTrainingUpdateGradOutputNum << ", but got " << bn_update_grad_outputs.size()
                               << trace::DumpSourceLines(bn_grad_node);
  }
  std::vector<AnfNodePtr> bn_reduce_grad_inputs = {
    NewValueNode(std::make_shared<Primitive>(kBNTrainingReduceGradOpName)),
    bn_grad_inputs[kIndex1],
    bn_grad_inputs[kIndex2],
    bn_update_grad_outputs[kIndex0],
    bn_update_grad_outputs[kIndex1],
    bn_grad_inputs[kIndex3],
    bn_grad_inputs[kIndex4],
    bn_grad_inputs[kIndex5]};
  auto bn_reduce_grad = NewCNode(bn_reduce_grad_inputs, graph);
  MS_EXCEPTION_IF_NULL(bn_reduce_grad);
  bn_reduce_grad->set_kernel_info(std::make_shared<device::KernelInfo>());
  bn_reduce_grad->set_scope(bn_grad_node->scope());

  auto types = {common::AnfAlgo::GetOutputInferDataType(bn_grad_node, 0)};
  auto shapes = {AnfAlgo::GetOutputDetailShape(bn_grad_node, 0)};
  common::AnfAlgo::SetOutputTypeAndDetailShape(types, shapes, bn_reduce_grad.get());
  common::AnfAlgo::CopyNodeAttr(kAttrEpsilon, bn_grad_node, bn_reduce_grad);
  if (common::AnfAlgo::HasNodeAttr(kAttrFormat, bn_grad_node)) {
    common::AnfAlgo::CopyNodeAttr(kAttrFormat, bn_grad_node, bn_reduce_grad);
  } else {
    common::AnfAlgo::SetNodeAttr(kAttrFormat, MakeValue(kOpFormat_NCHW), bn_reduce_grad);
  }
  if (is_dynamic) {
    common::AnfAlgo::SetNodeAttr(kAttrInputIsDynamicShape, MakeValue(true), bn_reduce_grad);
    common::AnfAlgo::SetNodeAttr(kAttrOutputIsDynamicShape, MakeValue(true), bn_reduce_grad);
  }
  (*bn_reduce_grad_outputs).push_back(bn_reduce_grad);
}

CNodePtr SyncBnGradSplit::SyncBNGradSplitForTBE(const FuncGraphPtr &func_graph, const CNodePtr &cnode) const {
  MS_EXCEPTION_IF_NULL(func_graph);
  MS_EXCEPTION_IF_NULL(cnode);
  std::vector<AnfNodePtr> bn_update_grad_outputs;

  bool is_dynamic = common::AnfAlgo::IsDynamicShape(cnode);
  CreateOutputsOfUpdateGrad(func_graph, cnode, &bn_update_grad_outputs, is_dynamic);
  if (bn_update_grad_outputs.size() != kBNTrainingUpdateGradOutputNum) {
    MS_LOG(INTERNAL_EXCEPTION) << "Outputs of bn_update_grad has wrong size, should be "
                               << kBNTrainingUpdateGradOutputNum << ", but got " << bn_update_grad_outputs.size()
                               << trace::DumpSourceLines(cnode);
  }

  std::vector<AnfNodePtr> allreduce_mul_outputs =
    CreateAllReduceAndMulForUpdate(func_graph, bn_update_grad_outputs, cnode, *this, is_dynamic);

  std::vector<AnfNodePtr> bn_reduce_grad_outputs;
  CreateOutputsOfReduceGrad(func_graph, cnode, allreduce_mul_outputs, &bn_reduce_grad_outputs, is_dynamic);
  if (bn_reduce_grad_outputs.size() != 1) {
    MS_LOG(INTERNAL_EXCEPTION) << "Outputs of bn_reduce_grad has wrong size, should be " << 1 << ", but got "
                               << bn_reduce_grad_outputs.size() << trace::DumpSourceLines(cnode);
  }

  auto make_tuple = CreateMakeTupleNode(
    func_graph, std::vector<AnfNodePtr>{bn_reduce_grad_outputs[0], allreduce_mul_outputs[0], allreduce_mul_outputs[1]});
  make_tuple->set_scope(cnode->scope());
  return make_tuple;
}

std::vector<std::string> BnGradSplit::MustExistPrimitiveName() const {
  std::vector<std::string> ret;
  ret.emplace_back(prim::kPrimBatchNormGrad->name());
  return ret;
}

const BaseRef BnGradSplit::DefinePattern() const {
  VarPtr Xs = std::make_shared<SeqVar>();
  return VectorRef({prim::kPrimBatchNormGrad, Xs});
}

const AnfNodePtr BnGradSplit::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node, const EquivPtr &) const {
  MS_EXCEPTION_IF_NULL(node);
  auto cnode = node->cast<CNodePtr>();
  MS_EXCEPTION_IF_NULL(cnode);
  auto is_train_input = cnode->input(kIdxIsTrain);
  MS_EXCEPTION_IF_NULL(is_train_input);
  auto is_training = GetNodeScalarValue<bool>(is_train_input);
  if (!is_training) {
    MS_LOG(INFO) << "Attr is_training should be true if do fusion";
    return nullptr;
  }
  return BNGradSplitForTBE(func_graph, cnode);
}

const BaseRef SyncBnGradSplit::DefinePattern() const {
  VarPtr Xs = std::make_shared<SeqVar>();
  return VectorRef({prim::kPrimSyncBatchNormGrad, Xs});
}

const AnfNodePtr SyncBnGradSplit::Process(const FuncGraphPtr &func_graph, const AnfNodePtr &node,
                                          const EquivPtr &) const {
  MS_EXCEPTION_IF_NULL(node);
  auto cnode = node->cast<CNodePtr>();
  return SyncBNGradSplitForTBE(func_graph, cnode);
}
}  // namespace opt
}  // namespace mindspore
