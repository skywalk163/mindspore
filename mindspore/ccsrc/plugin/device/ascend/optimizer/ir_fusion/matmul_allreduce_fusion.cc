/**
 * Copyright 2024 Huawei Technologies Co., Ltd
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

#include "plugin/device/ascend/optimizer/ir_fusion/matmul_allreduce_fusion.h"
#include <set>
#include <vector>
#include "mindspore/core/ops/nn_ops.h"
#include "mindspore/core/ops/math_ops.h"
#include "mindspore/core/ops/other_ops.h"
#include "mindspore/core/ops/lite_ops.h"
#include "mindspore/core/utils/ms_context.h"
#include "include/backend/optimizer/helper.h"
#include "include/backend/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "include/common/utils/utils.h"
#include "utils/trace_base.h"
#include "ops/fusion/matmul_allreduce.h"
#include "mindspore/core/ops/all_reduce.h"
#include "plugin/device/ascend/optimizer/common/gllo_utils.h"
#include "ops/op_utils.h"
#include "mindspore/ccsrc/frontend/parallel/ops_info/ops_utils.h"
#include "mindspore/core/ir/anf.h"
#include "utils/phase.h"

namespace mindspore::opt {
const BaseRef MatMulAllReduceFusion::DefinePattern() const {
  auto matmul_input_1 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(matmul_input_1 != nullptr, {});
  auto matmul_input_2 = std::make_shared<Var>();
  MS_CHECK_TRUE_RET(matmul_input_2 != nullptr, {});
  auto is_matmul = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimMatMul>);
  MS_CHECK_TRUE_RET(is_matmul != nullptr, {});
  VectorRef matmul_ref = VectorRef({is_matmul, matmul_input_1, matmul_input_2});

  auto is_allreduce = std::make_shared<CondVar>(IsSpecifiedNode<&prim::kPrimAllReduce>);
  MS_CHECK_TRUE_RET(is_allreduce != nullptr, {});
  VectorRef pattern_ref = VectorRef({is_allreduce, matmul_ref});
  return pattern_ref;
}

PrimitivePtr MatMulAllReduceFusion::CreateMatMulAllReducePrim(const PrimitivePtr &allreduce_prim,
                                                              const PrimitivePtr &matmul_prim) const {
  // create op
  auto matmul_allreduce_prim = prim::kPrimMatMulAllReduce->Clone();
  MS_CHECK_TRUE_RET(matmul_allreduce_prim, {});
  // add attr
  matmul_allreduce_prim->AddAttr(kAttrNameGroup, allreduce_prim->GetAttr(kAttrNameGroup));
  matmul_allreduce_prim->AddAttr(kAttrNameFusion, allreduce_prim->GetAttr(kAttrNameFusion));
  matmul_allreduce_prim->AddAttr(kAttrNameOp, allreduce_prim->GetAttr(kAttrNameOp));
  matmul_allreduce_prim->AddAttr(kAttrNameTransposeA, matmul_prim->GetAttr(kAttrNameTransposeA));
  matmul_allreduce_prim->AddAttr(kAttrNameTransposeB, matmul_prim->GetAttr(kAttrNameTransposeB));
  return matmul_allreduce_prim;
}

AnfNodePtr MatMulAllReduceFusion::CreateMatMulAllReduceNode(const FuncGraphPtr &func_graph,
                                                            const AnfNodePtr &node) const {
  MS_LOG(DEBUG) << "start create MatMulAllReduce";
  MS_ASSERT(func_graph != nullptr && node != nullptr);
  auto allreduce_cnode = node->cast<CNodePtr>();
  MS_ASSERT(allreduce_cnode != nullptr);
  auto allreduce_prim = GetCNodePrimitive(allreduce_cnode);
  auto matmul_cnode = allreduce_cnode->input(kIndex1)->cast<CNodePtr>();
  MS_ASSERT(matmul_cnode != nullptr);
  auto matmul_prim = GetCNodePrimitive(matmul_cnode);
  auto input_x_node = matmul_cnode->input(kIndex1);
  MS_ASSERT(input_x_node != nullptr);
  auto input_y_node = matmul_cnode->input(kIndex2);
  MS_ASSERT(input_y_node != nullptr);

  const std::set<TypeId> support_dtype = {kNumberTypeFloat16, kNumberTypeFloat32, kNumberTypeBFloat16};
  if (!CheckSupportDataType(input_x_node, support_dtype)) {
    return nullptr;
  }

  auto matmul_allreduce_prim_c = CreateMatMulAllReducePrim(allreduce_prim, matmul_prim);
  std::vector<AnfNodePtr> matmul_allreduce_inputs = {input_x_node, input_y_node};

  auto matmul_allreduce_cnode = func_graph->NewCNode(matmul_allreduce_prim_c, matmul_allreduce_inputs);
  matmul_allreduce_cnode->set_abstract(allreduce_cnode->abstract()->Clone());
  MS_LOG(DEBUG) << "create MatMulAllReduce success.";
  return matmul_allreduce_cnode;
}

const AnfNodePtr MatMulAllReduceFusion::Process(const mindspore::FuncGraphPtr &func_graph,
                                                const mindspore::AnfNodePtr &node,
                                                const mindspore::EquivPtr &equiv) const {
  auto ms_context = MsContext::GetInstance();
  MS_EXCEPTION_IF_NULL(ms_context);
  if (!ms_context->IsEnableInferBoost()) {
    return nullptr;
  }

  auto phase = PhaseManager::GetInstance().phase();
  if (common::GetEnv("DISABLE_MATMULALLREDUCE_FUSION") == "True" || common::GetEnv("MS_ENABLE_LCCL").empty() ||
      phase.rfind(kPhaseNamePrefill) == std::string::npos) {
    return nullptr;
  }

  if (func_graph == nullptr || node == nullptr) {
    return nullptr;
  }
  auto allreduce_cnode = node->cast<CNodePtr>();
  if (allreduce_cnode->size() != 2) {
    return nullptr;
  }

  auto matmul_allreduce_cnode = CreateMatMulAllReduceNode(func_graph, node);
  MS_CHECK_TRUE_RET(matmul_allreduce_cnode != nullptr, nullptr);

  auto manager = func_graph->manager();
  MS_CHECK_TRUE_RET(manager != nullptr, nullptr);

  // replace allreduce to MatMulAllReduce
  (void)manager->Replace(allreduce_cnode, matmul_allreduce_cnode);
  MS_LOG(DEBUG) << "MatMulAllReduce replace success";
  return matmul_allreduce_cnode;
}
}  // namespace mindspore::opt
