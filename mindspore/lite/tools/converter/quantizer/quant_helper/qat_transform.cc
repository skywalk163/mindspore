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
#include "tools/converter/quantizer/quant_helper/qat_transform.h"
#include <memory>
#include <set>
#include "mindspore/core/ops/lite_ops.h"
#include "tools/converter/quantizer/quantize_util.h"
#include "tools/converter/quantizer/quant_helper/remove_unused_quant_param.h"
#include "tools/converter/quantizer/quant_helper/quant_type_determiner.h"
#include "tools/converter/quantizer/quant_helper/propagate_quant_param_pass.h"
#include "tools/converter/quantizer/quant_helper/transform_uint8_pass.h"
#include "tools/converter/quantizer/quant_helper/quant_node_pass.h"
#include "tools/converter/quantizer/insert_quant_node_manager.h"
#include "tools/converter/quantizer/fixed_bit_weight_quantization_with_holder.h"
#include "tools/converter/quantizer/quant_strategy.h"
#include "include/backend/optimizer/graph_optimizer.h"
#include "tools/optimizer/fusion/quant_dtype_cast_fusion.h"
#include "tools/optimizer/graph/infershape_pass.h"
#include "tools/optimizer/common/gllo_utils.h"
#include "tools/optimizer/common/pass_manager_extends.h"
#include "tools/common/node_util.h"
#include "include/errorcode.h"
namespace mindspore::lite::quant {
STATUS QATTransform::DoSingleGraphQATTransform(const FuncGraphPtr &func_graph) {
  if (param_->transformQuantParam.precision_mode == quant::PrecisionMode::FLOAT32) {
    auto remove_quant_param_pass = RemoveQuantParam(func_graph);
    auto ret = remove_quant_param_pass.Remove();
    if (ret != RET_OK) {
      MS_LOG(ERROR) << "remove quant param failed.";
      return RET_ERROR;
    }
  }
  auto propogate_pass = PropagateQuantParamPass(func_graph);
  auto ret = propogate_pass.Propagate();
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Propagate quant param failed.";
    return ret;
  }
  auto determiner = QuantTypeDeterminer(func_graph);
  ret = determiner.Determine();
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Run quant type determine failed.";
    return ret;
  }
  auto per_channel_primitive_types = {prim::kPrimConv2DFusion, prim::kPrimConv2dTransposeFusion};
  ret = StaticWeightQuantInfo(func_graph, per_channel_primitive_types);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Quant Weight failed.";
    return RET_ERROR;
  }
  auto transform_uint8_pass = TransformUint8Pass(func_graph);
  ret = transform_uint8_pass.Transform();
  if (ret != RET_OK && ret != RET_NO_CHANGE) {
    MS_LOG(ERROR) << "Run dtype transform pass failed.";
    return ret;
  }
  auto quant_node_pass = QuantNodePass(func_graph);
  ret = quant_node_pass.Quant();
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Run quant node pass failed.";
    return ret;
  }
  InsertQuantNodeManager inset_quant_node_pass;
  ret = inset_quant_node_pass.InsertDequantNode(func_graph);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << "Add QuantCast failed";
    return RET_ERROR;
  }

  auto optimizer = std::make_shared<opt::GraphOptimizer>();
  CHECK_NULL_RETURN(optimizer);
  auto fusion_pm = std::make_shared<opt::LitePassManager>("fusion pass manager after quant", false);
  CHECK_NULL_RETURN(fusion_pm);
  fusion_pm->AddPass(std::make_shared<opt::QuantDtypeCastFusion>());
  fusion_pm->AddPass(std::make_shared<opt::InferShapePass>(param_->fmk_type, param_->train_model));
  optimizer->AddPassManager(fusion_pm);
  if (optimizer->Optimize(func_graph) == nullptr) {
    MS_LOG(ERROR) << "run cast node fusion failed.";
    return RET_ERROR;
  }

  return RET_OK;
}

int QATTransform::Transform() {
  std::set<FuncGraphPtr> all_func_graphs{};
  GetFuncGraphs(func_graph_, &all_func_graphs);
  // Support for multi-subgraph models
  for (auto &item : all_func_graphs) {
    auto status = DoSingleGraphQATTransform(item);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "Do QATTransform failed.";
      return status;
    }
  }
  return RET_OK;
}

bool QATTransform::CheckWeightQuantExist(const CNodePtr &cnode) {
  auto quant_param_holder = GetCNodeQuantHolder(cnode);
  MS_CHECK_TRUE_RET(quant_param_holder != nullptr, false);
  for (size_t index = kPrimOffset; index < cnode->size(); index++) {
    auto input_node = cnode->input(index);
    if (IsGraphInput(input_node)) {
      continue;
    }
    if (input_node->isa<mindspore::Parameter>()) {
      if (index == THIRD_INPUT + kPrimOffset && quant::CheckNodeInSet(cnode, quant::kHasBiasOperator)) {
        continue;
      }
      // Constants have quantization parameters
      if (quant_param_holder->CheckInit(index - quant::kPrimOffset, true)) {
        return true;
      }
    }
  }
  return false;
}

int QATTransform::StaticWeightQuantInfo(const FuncGraphPtr &func_graph,
                                        const std::set<PrimitivePtr> &per_channel_primitive_types) {
  for (auto &cnode : func_graph->GetOrderedCnodes()) {
    auto quant_param_holder = GetCNodeQuantHolder(cnode);
    if (quant_param_holder == nullptr) {
      continue;
    }
    auto quant_type = quant_param_holder->quant_type();
    if (quant_type != quant::QUANT_WEIGHT && quant_type != quant::QUANT_ALL) {
      MS_LOG(DEBUG) << "Invalid quant type, dont need weight quant.";
      continue;
    }
    if (CheckWeightQuantExist(cnode)) {
      MS_LOG(INFO) << "Weight quant param exist, cnode name: " << cnode->fullname_with_scope();
      continue;
    }
    auto ret = ConvertCNodeFp16ToFp32(cnode);
    if (ret != RET_OK) {
      MS_LOG(ERROR) << "Fail to convert cnode fp16 to fp32";
      return ret;
    }
    for (size_t i = 1; i < cnode->size(); ++i) {
      auto input = cnode->input(i);
      ParameterPtr parameter;
      tensor::TensorPtr tensor_info;
      GetParameterAndTensor(input, &parameter, &tensor_info);

      if (parameter == nullptr || tensor_info == nullptr ||
          tensor_info->compression_type() != mindspore::kNoCompression ||
          tensor_info->data_type() != TypeId::kNumberTypeFloat32) {
        MS_LOG(INFO) << "This op " << cnode->fullname_with_scope() << "'input[" << i - 1
                     << "] is not parameter, dont need quant weight";
        continue;
      }
      int preferred_dim = GetPreferredDim(cnode, i - 1, ConvertShapeVectorToInt32(tensor_info->shape()));
      QuantStrategy quant_strategy(0, 0, {});
      if (!quant_strategy.CanTensorQuantized(cnode, input, preferred_dim)) {
        MS_LOG(INFO) << "This op " << cnode->fullname_with_scope() << "'input[" << i - 1 << "] dont need quant weight";
        continue;
      }
      FixedBitWeightQuantizationWithHolder fixed_bit_quant;
      auto primitive = GetValueNode<PrimitivePtr>(cnode->input(0));
      CHECK_NULL_RETURN(primitive);
      auto weight_quant_type = WeightQuantType::FIXED_BIT_PER_LAYER;
      if (CheckNodeInSet(cnode, per_channel_primitive_types)) {
        weight_quant_type = WeightQuantType::FIXED_BIT_PER_CHANNEL;
      }
      ret = fixed_bit_quant.StatisticsFilter(tensor_info, primitive, quant::QUANT_ALL, INT8_MAX, -INT8_MAX, k8Bit,
                                             weight_quant_type, kNumberTypeInt8, i - 1, preferred_dim, true);
      if (ret != RET_OK) {
        MS_LOG(ERROR) << "Statistics failed.";
        return ret;
      }
    }
  }
  return RET_OK;
}
}  // namespace mindspore::lite::quant
