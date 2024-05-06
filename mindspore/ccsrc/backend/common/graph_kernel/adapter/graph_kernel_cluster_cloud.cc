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
#include "backend/common/graph_kernel/adapter/graph_kernel_cluster_cloud.h"
#include <set>
#include "mindspore/core/ops/sequence_ops.h"
#include "mindspore/core/ops/nn_optimizer_ops.h"
#include "mindspore/core/ops/nn_ops.h"
#include "mindspore/core/ops/math_ops.h"
#include "mindspore/core/ops/lite_ops.h"
#include "mindspore/core/ops/comparison_ops.h"
#include "mindspore/core/ops/array_ops.h"
#include "mindspore/core/ops/framework_ops.h"
#include "ir/graph_utils.h"
#include "include/common/utils/anfalgo.h"
#include "utils/anf_utils.h"
#include "utils/ms_context.h"
#include "utils/file_utils.h"
#include "backend/common/graph_kernel/graph_kernel_flags.h"
#include "backend/common/graph_kernel/core/graph_kernel_callback.h"
#include "backend/common/graph_kernel/core/graph_kernel_utils.h"
#include "backend/common/graph_kernel/core/value_depend_op_utils.h"
#include "backend/common/graph_kernel/graph_kernel_helper.h"

namespace mindspore::graphkernel {
namespace {
std::set<TypeId> dvm_float_types{kNumberTypeFloat16, kNumberTypeFloat32, kNumberTypeBFloat16};

bool CheckFormat(const AnfNodePtr &node) {
  if (common::AnfAlgo::IsDynamicShape(node) && !CheckDefaultFormat(node)) {
    // dvm kernel infer shape use inputs device shape, but the output abstract shape inferred from device shape is
    // not unique if some shape value are not a multiple of 16
    MS_LOG(DEBUG) << "skip node: " << node->fullname_with_scope()
                  << " because only default format is supported in dynamic shape";
    return false;
  }
  auto cb = Callback::Instance();
  MS_EXCEPTION_IF_NULL(cb);
  auto input_num = AnfUtils::GetInputTensorNum(node);
  if (input_num > 0) {
    bool has_special_format = false;
    auto base_format = cb->GetInputFormat(node, 0);
    for (size_t i = 0; i < input_num; ++i) {
      auto input_format = cb->GetInputFormat(node, i);
      if (!has_special_format &&
          (input_format.find("FRACTAL") != std::string::npos || input_format.find("C0") != std::string::npos)) {
        has_special_format = true;
      }
      if (has_special_format && input_format != base_format) {
        // mixed special format and default format is not supported, because extra Reshape/TransData is needed
        return false;
      }
    }
  }
  return true;
}

bool DvmSliceSupported(const AnfNodePtr &node, TypeId node_output_type) {
  constexpr size_t input_num = 3;
  if (common::AnfAlgo::IsDynamicRankNode(node) || GetShape(node).size() > input_num) {
    return false;
  }
  if (IsPrimitiveCNode(node, prim::kPrimStridedSlice)) {
    auto cnode = node->cast<CNodePtr>();
    auto step_node = cnode->input(kIndex4)->cast<ValueNodePtr>();
    if (step_node == nullptr) {
      return false;
    }
    auto step_vector = GetValue<std::vector<int64_t>>(step_node->value());
    if (std::any_of(step_vector.begin(), step_vector.end(), [](int i) { return i != 1; })) {
      return false;
    }
  }
  return (dvm_float_types.find(node_output_type) != dvm_float_types.end() || node_output_type == kNumberTypeInt32);
}

bool DvmSupported(const AnfNodePtr &node) {
  // check format
  if (!CheckFormat(node)) {
    return false;
  }
  auto cb = Callback::Instance();
  MS_EXCEPTION_IF_NULL(cb);
  auto node_output_type = cb->GetOutputType(node, 0);
  // cast op
  if (IsPrimitiveCNode(node, prim::kPrimCast)) {
    static std::set<TypeId> supported_types{kNumberTypeFloat16, kNumberTypeFloat32, kNumberTypeBool, kNumberTypeInt32,
                                            kNumberTypeBFloat16};
    auto node_input_type = cb->GetInputType(node, 0);
    return !(supported_types.find(node_input_type) == supported_types.end() ||
             supported_types.find(node_output_type) == supported_types.end());
  }
  // reduceSum op
  if (IsPrimitiveCNode(node, prim::kPrimReduceSum)) {
    auto prim = GetCNodePrimitive(node);
    MS_EXCEPTION_IF_NULL(prim);
    auto skip_mode_attr = prim->GetAttr(kAttrSkipMode);
    MS_EXCEPTION_IF_NULL(skip_mode_attr);
    auto skip_mode = GetValue<bool>(skip_mode_attr);
    if (skip_mode == true) {
      return false;
    }
  }
  // compare op
  static std::vector<PrimitivePtr> compare_ops{prim::kPrimEqual,        prim::kPrimNotEqual, prim::kPrimGreater,
                                               prim::kPrimGreaterEqual, prim::kPrimLess,     prim::kPrimLessEqual};
  if (std::any_of(compare_ops.begin(), compare_ops.end(),
                  [&node](const PrimitivePtr &prim) { return IsPrimitiveCNode(node, prim); })) {
    auto node_input_type = cb->GetInputType(node, 0);
    return (dvm_float_types.find(node_input_type) != dvm_float_types.end() || node_input_type == kNumberTypeInt32);
  }
  // int op
  static std::vector<PrimitivePtr> int_ops{
    prim::kPrimAdd, prim::kPrimSub, prim::kPrimMul,    prim::kPrimMaximum, prim::kPrimMinimum,
    prim::kPrimNeg, prim::kPrimAbs, prim::kPrimSelect, prim::kPrimAssign,  prim::kPrimBroadcastTo};
  if (std::any_of(int_ops.begin(), int_ops.end(),
                  [&node](const PrimitivePtr &prim) { return IsPrimitiveCNode(node, prim); })) {
    return (dvm_float_types.find(node_output_type) != dvm_float_types.end() || node_output_type == kNumberTypeInt32);
  }
  // slice op
  static std::vector<PrimitivePtr> slice_ops{prim::kPrimSlice, prim::kPrimStridedSlice};
  if (std::any_of(slice_ops.begin(), slice_ops.end(),
                  [&node](const PrimitivePtr &prim) { return IsPrimitiveCNode(node, prim); })) {
    return DvmSliceSupported(node, node_output_type);
  }
  // other op
  return dvm_float_types.find(node_output_type) != dvm_float_types.end();
}

const std::vector<OpWithLevel> clusterable_ops_with_level = {
  // all target
  {kAllTarget, OpLevel_0, prim::kPrimAbs},
  {kAllTarget, OpLevel_0, prim::kPrimAdd},
  {kAllTarget, OpLevel_0, prim::kPrimCast},
  {kAllTarget, OpLevel_0, prim::kPrimEqual},
  {kAllTarget, OpLevel_0, prim::kPrimExp},
  {kAllTarget, OpLevel_0, prim::kPrimLog},
  {kAllTarget, OpLevel_0, prim::kPrimMaximum},
  {kAllTarget, OpLevel_0, prim::kPrimMinimum},
  {kAllTarget, OpLevel_0, prim::kPrimMul},
  {kAllTarget, OpLevel_0, prim::kPrimNeg},
  {kAllTarget, OpLevel_0, prim::kPrimPow},
  {kAllTarget, OpLevel_0, prim::kPrimRealDiv},
  {kAllTarget, OpLevel_0, prim::kPrimReciprocal},
  {kAllTarget, OpLevel_1, prim::kPrimReduceSum},
  {kAllTarget, OpLevel_1, prim::kPrimReshape},
  {kAllTarget, OpLevel_0, prim::kPrimRound},
  {kAllTarget, OpLevel_0, prim::kPrimRsqrt},
  {kAllTarget, OpLevel_0, prim::kPrimSqrt},
  {kAllTarget, OpLevel_0, prim::kPrimSub},
  {kAllTarget, OpLevel_0, prim::kPrimTanh},
  {kAllTarget, OpLevel_1, prim::kPrimTranspose},
  // ascend
  {kAscendDevice, OpLevel_1, prim::kPrimMatMul},
  {kAscendDevice, OpLevel_1, prim::kPrimTransData},
  {kAscendDevice, OpLevel_1, prim::kPrimBatchMatMul},
  // gpu
  {kGPUDevice, OpLevel_0, prim::kPrimACos},
  {kGPUDevice, OpLevel_0, prim::kPrimAcosh},
  {kGPUDevice, OpLevel_2, prim::kPrimArgMax},
  {kGPUDevice, OpLevel_2, prim::kPrimArgmin},
  {kGPUDevice, OpLevel_0, prim::kPrimAsin},
  {kGPUDevice, OpLevel_0, prim::kPrimAsinh},
  {kGPUDevice, OpLevel_0, prim::kPrimAssign},
  {kGPUDevice, OpLevel_0, prim::kPrimAtan},
  {kGPUDevice, OpLevel_0, prim::kPrimAtan2},
  {kGPUDevice, OpLevel_0, prim::kPrimCos},
  {kGPUDevice, OpLevel_0, prim::kPrimDiv},
  {kGPUDevice, OpLevel_0, prim::kPrimErf},
  {kGPUDevice, OpLevel_0, prim::kPrimExpm1},
  {kGPUDevice, OpLevel_0, prim::kPrimFloor},
  {kGPUDevice, OpLevel_0, prim::kPrimFloorDiv},
  {kGPUDevice, OpLevel_0, prim::kPrimFloorMod},
  {kGPUDevice, OpLevel_0, prim::kPrimGreater},
  {kGPUDevice, OpLevel_0, prim::kPrimGreaterEqual},
  {kGPUDevice, OpLevel_0, prim::kPrimIsFinite},
  {kGPUDevice, OpLevel_0, prim::kPrimIsInf},
  {kGPUDevice, OpLevel_0, prim::kPrimIsNan},
  {kGPUDevice, OpLevel_0, prim::kPrimLess},
  {kGPUDevice, OpLevel_0, prim::kPrimLessEqual},
  {kGPUDevice, OpLevel_0, prim::kPrimLogicalAnd},
  {kGPUDevice, OpLevel_0, prim::kPrimLogicalOr},
  {kGPUDevice, OpLevel_0, prim::kPrimLogicalNot},
  {kGPUDevice, OpLevel_0, prim::kPrimMod},
  {kGPUDevice, OpLevel_0, prim::kPrimNotEqual},
  {kGPUDevice, OpLevel_1, prim::kPrimReduceMax},
  {kGPUDevice, OpLevel_1, prim::kPrimReduceMin},
  {kGPUDevice, OpLevel_0, prim::kPrimSelect},
  {kGPUDevice, OpLevel_0, prim::kPrimSign},
  {kGPUDevice, OpLevel_0, prim::kPrimSin},
  {kGPUDevice, OpLevel_0, prim::kPrimStridedSlice},
  {kGPUDevice, OpLevel_1, prim::kPrimCumSum},
  {kGPUDevice, OpLevel_1, prim::kPrimOneHot},
  // cpu
  {kCPUDevice, OpLevel_0, prim::kPrimLogicalNot},
  {kCPUDevice, OpLevel_0, prim::kPrimMod},
  {kCPUDevice, OpLevel_1, prim::kPrimReduceMax},
  {kCPUDevice, OpLevel_0, prim::kPrimSelect},
  {kCPUDevice, OpLevel_0, prim::kPrimLess},
  {kCPUDevice, OpLevel_0, prim::kPrimLessEqual},
};

const std::vector<OpWithLevel> clusterable_ops_with_level_v2 = {
  // cpu
  {kCPUDevice, OpLevel_0, prim::kPrimNotEqual},
  {kCPUDevice, OpLevel_0, prim::kPrimGreaterEqual},
  {kCPUDevice, OpLevel_0, prim::kPrimGreater},
  {kCPUDevice, OpLevel_0, prim::kPrimFloor},
  {kCPUDevice, OpLevel_0, prim::kPrimIsNan},
  {kCPUDevice, OpLevel_0, prim::kPrimAssign},
  {kCPUDevice, OpLevel_0, prim::kPrimBroadcastTo},
  {kCPUDevice, OpLevel_0, prim::kPrimTile},
  {kCPUDevice, OpLevel_0, prim::kPrimLogicalAnd},
  {kCPUDevice, OpLevel_0, prim::kPrimCos},
  {kCPUDevice, OpLevel_0, prim::kPrimSin},
  {kCPUDevice, OpLevel_0, prim::kPrimACos},
  {kCPUDevice, OpLevel_0, prim::kPrimAsin},
  {kCPUDevice, OpLevel_0, prim::kPrimTanh},
  {kCPUDevice, OpLevel_0, prim::kPrimAtan2},
  {kCPUDevice, OpLevel_0, prim::kPrimMinimum},
  {kCPUDevice, OpLevel_0, prim::kPrimMaximum},
  {kCPUDevice, OpLevel_0, prim::kPrimReduceAll},
  {kCPUDevice, OpLevel_0, prim::kPrimStridedSlice},
  // gpu
  {kGPUDevice, OpLevel_0, prim::kPrimNotEqual},
  {kGPUDevice, OpLevel_0, prim::kPrimSelect},
  {kGPUDevice, OpLevel_0, prim::kPrimTile},
  {kGPUDevice, OpLevel_0, prim::kPrimLogicalAnd},
  {kGPUDevice, OpLevel_0, prim::kPrimCos},
  {kGPUDevice, OpLevel_0, prim::kPrimSin},
  {kGPUDevice, OpLevel_0, prim::kPrimMinimum},
  {kGPUDevice, OpLevel_0, prim::kPrimMaximum},
  {kGPUDevice, OpLevel_0, prim::kPrimAssign},
};

const std::vector<std::string> disable_cluster_op_list_v2 = {"OneHot", "CumSum",      "Transpose",   "BatchMatMul",
                                                             "MatMul", "BroadcastTo", "StridedSlice"};

const std::vector<OpWithLevel> clusterable_ops_with_level_dvm = {
  {kAscendDevice, OpLevel_0, prim::kPrimAbs},          {kAscendDevice, OpLevel_0, prim::kPrimAdd},
  {kAscendDevice, OpLevel_0, prim::kPrimBroadcastTo},  {kAscendDevice, OpLevel_0, prim::kPrimCast},
  {kAscendDevice, OpLevel_0, prim::kPrimExp},          {kAscendDevice, OpLevel_0, prim::kPrimLog},
  {kAscendDevice, OpLevel_0, prim::kPrimMaximum},      {kAscendDevice, OpLevel_0, prim::kPrimMinimum},
  {kAscendDevice, OpLevel_0, prim::kPrimMul},          {kAscendDevice, OpLevel_0, prim::kPrimNeg},
  {kAscendDevice, OpLevel_0, prim::kPrimPow},          {kAscendDevice, OpLevel_0, prim::kPrimDiv},
  {kAscendDevice, OpLevel_0, prim::kPrimRealDiv},      {kAscendDevice, OpLevel_0, prim::kPrimReciprocal},
  {kAscendDevice, OpLevel_0, prim::kPrimRsqrt},        {kAscendDevice, OpLevel_0, prim::kPrimSqrt},
  {kAscendDevice, OpLevel_0, prim::kPrimSub},          {kAscendDevice, OpLevel_0, prim::kPrimEqual},
  {kAscendDevice, OpLevel_0, prim::kPrimNotEqual},     {kAscendDevice, OpLevel_0, prim::kPrimGreater},
  {kAscendDevice, OpLevel_0, prim::kPrimGreaterEqual}, {kAscendDevice, OpLevel_0, prim::kPrimLess},
  {kAscendDevice, OpLevel_0, prim::kPrimLessEqual},    {kAscendDevice, OpLevel_0, prim::kPrimLogicalAnd},
  {kAscendDevice, OpLevel_0, prim::kPrimLogicalOr},    {kAscendDevice, OpLevel_0, prim::kPrimLogicalNot},
  {kAscendDevice, OpLevel_0, prim::kPrimSelect},       {kAscendDevice, OpLevel_0, prim::kPrimAssign},
  {kAscendDevice, OpLevel_0, prim::kPrimReduceSum},    {kAscendDevice, OpLevel_0, prim::kPrimIsFinite},
  {kAscendDevice, OpLevel_1, prim::kPrimReshape},
};
}  // namespace

std::vector<PrimitivePtr> StaticShapeCluster::GetClusterOps() {
  const auto &flags = GraphKernelFlags::GetInstance();
  std::vector<std::string> disable_cluster_ops = flags.disable_cluster_ops;
  auto cb = Callback::Instance();

  std::vector<OpWithLevel> clusterable_ops;
  if (flags.kernel_generator == "AKG_V2") {
    clusterable_ops = clusterable_ops_with_level;
    clusterable_ops.insert(clusterable_ops.end(), clusterable_ops_with_level_v2.begin(),
                           clusterable_ops_with_level_v2.end());
    if (cb->GetTargetFromContext() == kCPUDevice &&
        std::find(flags.enable_cluster_ops.begin(), flags.enable_cluster_ops.end(), "Reshape") ==
          flags.enable_cluster_ops.end()) {
      disable_cluster_ops.push_back("Reshape");
    }
    if (cb->GetTargetFromContext() == kGPUDevice) {
      for (const std::string &item : disable_cluster_op_list_v2) {
        if (std::find(flags.enable_cluster_ops.begin(), flags.enable_cluster_ops.end(), item) ==
            flags.enable_cluster_ops.end()) {
          disable_cluster_ops.push_back(item);
        }
      }
    }
  } else if (flags.kernel_generator == "DVM") {
    clusterable_ops = clusterable_ops_with_level_dvm;
  } else {
    clusterable_ops = clusterable_ops_with_level;
  }
  auto ops = GkUtils::GetValidOps(clusterable_ops, flags.fusion_ops_level, flags.enable_cluster_ops_only,
                                  flags.enable_cluster_ops, disable_cluster_ops);
  return GkUtils::FilterExcludedOps(ops);
}

std::vector<PrimitivePtr> StaticShapeCluster::GetClusterableOpList() { return StaticShapeCluster::GetClusterOps(); }

bool StaticShapeCluster::IsClusterableOp(const AnfNodePtr &node) {
  if (AnfUtils::IsGraphKernel(node)) {
    auto sub_graph = GetCNodeFuncGraph(node);
    if (auto type = sub_graph->get_attr("composite_type")) {
      if (GetValue<std::string>(type) == "inplace_assign_builder") {
        return false;
      }
    }
    return true;
  }
  if (GkUtils::IsKeepBasicNode(node)) {
    return false;
  }
  bool is_dvm = (GraphKernelFlags::GetInstance().kernel_generator == "DVM");
  if (!is_dvm && common::AnfAlgo::IsDynamicShape(node)) {
    return false;
  }
  bool node_in_oplist = std::any_of(op_list_.begin(), op_list_.end(),
                                    [&node](const PrimitivePtr &prim) { return IsPrimitiveCNode(node, prim); });
  if (!node_in_oplist) {
    return false;
  }

  auto cb = Callback::Instance();
  MS_EXCEPTION_IF_NULL(cb);
  // if node's output type is complex64 or complex128, cannot be added to the cluster list.
  auto node_output_type = cb->GetOutputType(node, 0);
  if (node_output_type == kNumberTypeComplex64 || node_output_type == kNumberTypeComplex128) {
    return false;
  }
  if (IsPrimitiveCNode(node, prim::kPrimCast)) {
    auto node_input_type = cb->GetInputType(node, 0);
    if ((node_input_type == kNumberTypeComplex64) || (node_input_type == kNumberTypeComplex128)) {
      return false;
    }
  }

  if (is_dvm && !DvmSupported(node)) {
    return false;
  }

  if (IsPrimitiveCNode(node, prim::kPrimReshape)) {
    auto output_format = cb->GetOutputFormat(node, 0);
    if (output_format != kOpFormat_DEFAULT) {
      auto primitive = GetCNodePrimitive(node);
      MS_EXCEPTION_IF_NULL(primitive);
      primitive = primitive->Clone();
      // format attr used by ReshapeOp::InferFormat
      primitive->AddAttr("format", MakeValue(output_format));
      auto cnode = node->cast<CNodePtr>();
      MS_EXCEPTION_IF_NULL(cnode);
      cnode->set_input(kAnfPrimitiveIndex, NewValueNode(primitive));
    }
  }
  if (!ValueDependOpUtils::IsConstInput(node)) {
    return false;
  }
  return true;
}

std::vector<PrimitivePtr> DynamicShapeCluster::GetClusterableOpList() {
  std::vector<PrimitivePtr> dyn_clusterable_ops_list = {
    prim::kPrimAdd, prim::kPrimCast, prim::kPrimMul,  prim::kPrimRealDiv,   prim::kPrimSub,
    prim::kPrimAbs, prim::kPrimExp,  prim::kPrimLog,  prim::kPrimMaximum,   prim::kPrimMinimum,
    prim::kPrimNeg, prim::kPrimPow,  prim::kPrimSqrt, prim::kPrimTranspose, prim::kPrimReduceSum};
  return dyn_clusterable_ops_list;
}

bool DynamicShapeCluster::IsClusterableOp(const AnfNodePtr &node) {
  bool node_in_oplist = std::any_of(op_list_.begin(), op_list_.end(),
                                    [&node](const PrimitivePtr &prim) { return IsPrimitiveCNode(node, prim); });
  if (!node_in_oplist || common::AnfAlgo::IsDynamicRankNode(node)) {
    return false;
  }
  if (GkUtils::IsKeepBasicNode(node)) {
    return false;
  }
  if (!ValueDependOpUtils::IsConstInput(node)) {
    return false;
  }
  return true;
}

bool DynamicShapeCluster::Run(const FuncGraphPtr &func_graph) {
  auto mng = func_graph->manager();
  MS_EXCEPTION_IF_NULL(mng);
  Init(func_graph);
  bool changed = Process(func_graph);
  if (changed) {
    mng->RemoveRoots();
    mng->KeepRoots({func_graph});
  }
  Clean();
  return changed;
}
}  // namespace mindspore::graphkernel
