/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#include "src/train/optimizer/fusion/matmul_activation_fusion_pass.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "schema/inner/model_generated.h"
#include "tools/common/meta_graph_utils.h"
#include "src/train/optimizer/common/fusion_utils.h"
namespace {
constexpr std::string_view MatMulName = "MATMUL";
constexpr std::string_view ActName = "ACTIVATION";
}  // namespace
namespace mindspore {
namespace lite {
STATUS MatMulActivationFusionPass::DefinePattern() {
  auto matmul_op = std::make_shared<PatternOp>();
  MS_CHECK_TRUE_RET(matmul_op != nullptr, RET_NULL_PTR);
  matmul_op->id = MatMulName;
  matmul_op->types = {schema::PrimitiveType_MatMulFusion};
  auto act_op = std::make_shared<PatternOp>();
  MS_CHECK_TRUE_RET(act_op != nullptr, RET_NULL_PTR);
  act_op->id = ActName;
  act_op->types = {schema::PrimitiveType_Activation};
  act_op->left = matmul_op;
  auto fusion_pattern = std::make_unique<FusionPattern>("MatMulActivationFusion");
  MS_CHECK_TRUE_MSG(fusion_pattern != nullptr, RET_NULL_PTR, "new fusion_pattern failed");
  fusion_pattern->AddPatternOp(matmul_op);
  fusion_pattern->AddPatternOp(act_op);
  fusion_pattern->Finish();
  this->patterns.emplace_back(fusion_pattern.release());
  return RET_OK;
}

STATUS MatMulActivationFusionPass::DoFusion(
  MetaGraphT *graph, const std::string &pattern_name,
  const std::unordered_map<std::string, std::shared_ptr<Path>> &matched_path) {
  MS_CHECK_TRUE_RET(graph != nullptr, RET_NULL_PTR);
  if (matched_path.size() != opt::kMatchPathLenTwo) {
    MS_LOG(ERROR) << "MatMul-Activation-Fusion should have two NodeIndex in matchedPair";
    return RET_PARAM_INVALID;
  }

  size_t matmul_index = 0;
  auto ret = opt::GetMatchNodeIndex(graph, matched_path, std::string(MatMulName), &matmul_index);
  MS_CHECK_TRUE_MSG(ret == RET_OK, ret, "cannot get matmul_index");
  auto &matmul_node = graph->nodes.at(matmul_index);
  MS_CHECK_TRUE_MSG(matmul_node != nullptr, RET_NULL_PTR, "matmul_node is nullptr");
  size_t act_index = 0;
  ret = opt::GetMatchNodeIndex(graph, matched_path, std::string(ActName), &act_index);
  MS_CHECK_TRUE_MSG(ret == RET_OK, ret, "cannot get act_index");
  auto &act_node = graph->nodes.at(act_index);
  MS_CHECK_TRUE_MSG(act_node != nullptr, RET_NULL_PTR, "act_node is nullptr");

  if (matmul_node->quantType == schema::QuantType_QUANT_ALL ||
      matmul_node->quantType == schema::QuantType_QUANT_DYNAMIC) {
    MS_LOG(DEBUG) << "cannot fusion.";
    return RET_NO_CHANGE;
  }
  MS_CHECK_TRUE_RET(matmul_node->primitive != nullptr, RET_NULL_PTR);
  auto matmul_type = matmul_node->primitive->value.AsMatMulFusion();
  MS_CHECK_TRUE_RET(matmul_type->activation_type == ActivationType::ActivationType_NO_ACTIVATION, RET_NO_CHANGE);
  MS_CHECK_TRUE_RET(act_node->primitive != nullptr, RET_NULL_PTR);
  auto act_type = act_node->primitive->value.AsActivation()->activation_type;
  MS_CHECK_TRUE_RET(act_type == ActivationType::ActivationType_RELU || act_type == ActivationType::ActivationType_RELU6,
                    RET_NO_CHANGE);
  matmul_type->activation_type = act_type;
  matmul_node->outputIndex = {act_node->outputIndex};
  // cannot delete node here, otherwise will destroy order in other pattern's node index
  // make it an isolated node to be removed in IsolatedNodeRemovePass
  act_node->inputIndex.clear();
  act_node->outputIndex.clear();
  return RET_OK;
}

MatMulActivationFusionPass::~MatMulActivationFusionPass() = default;
}  // namespace lite
}  // namespace mindspore
