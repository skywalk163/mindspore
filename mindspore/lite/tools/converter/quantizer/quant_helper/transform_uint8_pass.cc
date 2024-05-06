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

#include "tools/converter/quantizer/quant_helper/transform_uint8_pass.h"
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include "mindspore/core/ops/framework_ops.h"
#include "tools/common/node_util.h"
#include "tools/converter/quantizer/insert_quant_node_manager.h"
#include "tools/converter/quantizer/quantize_util.h"
#include "tools/optimizer/common/format_utils.h"
#include "tools/optimizer/common/gllo_utils.h"

namespace mindspore::lite::quant {
// only enable for uint8
int TransformUint8Pass::Transform() {
  auto cnodes = func_graph_->GetOrderedCnodes();
  for (auto &cnode : cnodes) {
    if (!CheckNeedDTypeTrans(cnode)) {
      MS_LOG(DEBUG) << "CheckNeedDTypeTrans invalid cnode, cnode name: " << cnode->fullname_with_scope();
      continue;
    }
    auto status = DoNodeDTypeTrans(cnode);
    if (status == RET_NO_CHANGE) {
      return status;
    } else if (status != RET_OK) {
      MS_LOG(ERROR) << "DoNodeDTypeTrans failed, cnode name: " << cnode->fullname_with_scope();
      return status;
    }
    quant::QuantType curr_quant_type;
    if (GetQuantType(cnode, &curr_quant_type) != RET_OK) {
      MS_LOG(ERROR) << "Get quant type failed, cnode name: " << cnode->fullname_with_scope();
      return RET_ERROR;
    }
    if (curr_quant_type != quant::QUANT_ALL) {
      MS_LOG(INFO) << "Invalid cnode quant type, cnode name: " << cnode->fullname_with_scope()
                   << " quant type: " << curr_quant_type;
      continue;
    }
    quant::InsertQuantNodeManager insert_node_manager;
    status = insert_node_manager.InsertForwardCastNode(this->func_graph_, cnode, kNumberTypeUInt8, curr_quant_type);
    if (status != RET_OK) {
      MS_LOG(ERROR) << "InsertForwardCastNode failed, cnode name: " << cnode->fullname_with_scope();
      return status;
    }
    // DetectionPostProcess op(Uint8toFp32, not need backward cast node)
    if (!CheckNodeInSet(cnode, kUint8toFP32Operator)) {
      status = insert_node_manager.InsertBackwardCastNode(this->func_graph_, cnode, kNumberTypeUInt8, curr_quant_type);
      if (status != RET_OK) {
        MS_LOG(ERROR) << "InsertBackwardCastNode failed, cnode name: " << cnode->fullname_with_scope();
        return status;
      }
    }
  }  // for
  return RET_OK;
}

int TransformUint8Pass::DoParameterNodeTrans(const CNodePtr &cnode, const ParameterPtr &input_node,
                                             size_t input_index) {
  CHECK_NULL_RETURN(cnode);
  CHECK_NULL_RETURN(input_node);
  MS_CHECK_LT(input_index, cnode->size(), RET_ERROR);
  if (input_index == THIRD_INPUT + 1 && CheckNodeInSet(cnode, kHasBiasOperator)) {
    return RET_NOT_SUPPORT;
  }
  auto tensor_info = input_node->default_param()->cast<tensor::TensorPtr>();
  CHECK_NULL_RETURN(tensor_info);
  bool is_shared_weight = IsSharedWeightParameter(input_node);
  auto weight_name = input_node->fullname_with_scope();

  if (is_shared_weight) {
    auto iter = shared_weight_quant_params_.find(weight_name);
    if (iter != shared_weight_quant_params_.end()) {
      auto quant_param_holder = GetCNodeQuantHolder(cnode);
      CHECK_NULL_RETURN(quant_param_holder);
      quant_param_holder->set_input_quant_param(input_index - 1, iter->second);
      return RET_NO_CHANGE;
    }
  }

  // filter condition: dtype == kNumberTypeUInt8
  if (tensor_info->data_type() != kNumberTypeUInt8) {
    MS_LOG(INFO) << input_node->fullname_with_scope() << " dtype not uint8.";
    return RET_NOT_SUPPORT;
  }

  // transform weight data
  size_t elem_count = tensor_info->DataSize();
  auto ret = Uint8toInt8(static_cast<uint8_t *>(tensor_info->data().data()), elem_count);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << input_node->fullname_with_scope() << " transform data uint8 to int8 failed.";
    return ret;
  }

  // update zp
  auto quant_param_holder = GetCNodeQuantHolder(cnode);
  CHECK_NULL_RETURN(quant_param_holder);
  if (quant_param_holder->get_input_quant_params().size() < input_index) {
    MS_LOG(ERROR) << "Invalid quant params. input node  name: " << input_node->fullname_with_scope();
    return RET_ERROR;
  }
  auto quant_params = quant_param_holder->get_input_quant_params().at(input_index - 1);
  for (auto &quant_param : quant_params) {
    quant_param.zeroPoint -= kU8ZeroPointOffset;
  }
  quant_param_holder->set_input_quant_param(input_index - 1, quant_params);
  if (is_shared_weight && shared_weight_quant_params_.find(weight_name) == shared_weight_quant_params_.end()) {
    shared_weight_quant_params_.insert({weight_name, quant_params});
  }

  // set dtype
  tensor_info->set_data_type(kNumberTypeInt8);
  ret = UpdateDataType(input_node, kNumberTypeInt8);
  if (ret != RET_OK) {
    MS_LOG(ERROR) << input_node->fullname_with_scope() << " set new dtype failed.";
    return ret;
  }
  return RET_OK;
}

int TransformUint8Pass::Uint8toInt8(uint8_t *data, int size) const {
  CHECK_NULL_RETURN(data);

  for (int i = 0; i < size; i++) {
    int temp = static_cast<int>(data[i]) - kU8ZeroPointOffset;
    if (temp > INT8_MAX) {
      data[i] = INT8_MAX;
    } else if (temp < INT8_MIN) {
      data[i] = INT8_MIN;
    } else {
      data[i] = static_cast<int8_t>(temp);
    }
  }
  return RET_OK;
}

/**
 * Transform CNode(dtype,uint8toint8,weigh data)
 * */
int TransformUint8Pass::DoNodeDTypeTrans(const CNodePtr &cnode) {
  auto curr_quant_param_holder = GetCNodeQuantHolder(cnode);
  CHECK_NULL_RETURN(curr_quant_param_holder);
  TypeId cnode_dtype = kTypeUnknown;
  if (opt::GetDataTypeFromAnfNode(cnode, &cnode_dtype) != RET_OK) {
    MS_LOG(INFO) << "Get data type failed, cnode name: " << cnode->fullname_with_scope();
    return RET_NO_CHANGE;
  }
  if (cnode_dtype == kNumberTypeUInt8) {
    MS_LOG(INFO) << "cnode dtype kNumberTypeUInt8, cnode name: " << cnode->fullname_with_scope();
    if (UpdateDataType(cnode, kNumberTypeInt8) != RET_OK) {
      MS_LOG(ERROR) << "Update data type failed, cnode name: " << cnode->fullname_with_scope();
      return RET_ERROR;
    }
    if (opt::CheckPrimitiveType(cnode, prim::kPrimQuantDTypeCast)) {
      auto primitive_c = GetValueNode<std::shared_ptr<mindspore::Primitive>>(cnode->input(0));
      auto primc = api::MakeShared<mindspore::ops::QuantDTypeCast>(primitive_c);
      primc->set_dst_t(kNumberTypeInt8);
    }
    // update output quant param zp
    if (curr_quant_param_holder->get_output_quant_params().empty()) {
      MS_LOG(INFO) << "output quant params empty.";
      return RET_NO_CHANGE;
    }
    auto out_quant_params = curr_quant_param_holder->get_output_quant_params()[0];
    for (auto &quant_param : out_quant_params) {
      quant_param.zeroPoint -= kU8ZeroPointOffset;
    }
    curr_quant_param_holder->set_output_quant_param(0, out_quant_params);
  }

  // DTypeCastNode, set quant type
  if (opt::CheckPrimitiveType(cnode, prim::kPrimQuantDTypeCast)) {
    curr_quant_param_holder->set_quant_type(quant::QUANT_NONE);
  }

  for (size_t index = 1; index < cnode->size(); index++) {
    auto input_node = cnode->input(index);
    CHECK_NULL_RETURN(input_node);
    if (IsGraphInput(input_node) || input_node->isa<mindspore::CNode>()) {
      // updata graph input quant params
      if (curr_quant_param_holder->get_input_quant_params().size() < index) {
        MS_LOG(INFO) << "quant params invalid, input node name: " << input_node->fullname_with_scope();
        continue;
      }
      auto input_quant_params = curr_quant_param_holder->get_input_quant_params()[index - 1];
      if (input_quant_params.empty() || !input_quant_params.front().inited) {
        MS_LOG(INFO) << "input node not quantizied, input node name: " << input_node->fullname_with_scope();
        continue;
      }
      for (auto &quant_param : input_quant_params) {
        quant_param.zeroPoint -= kU8ZeroPointOffset;
      }
      curr_quant_param_holder->set_input_quant_param(index - 1, input_quant_params);
    } else if (input_node->isa<mindspore::Parameter>()) {  // weight data
      auto ret = DoParameterNodeTrans(cnode, input_node->cast<ParameterPtr>(), index);
      bool is_failed = (ret != RET_OK && ret != RET_NOT_SUPPORT && ret != RET_NO_CHANGE);
      if (is_failed) {
        MS_LOG(WARNING) << "DoParameterNodeTrans failed, input node name: " << input_node->fullname_with_scope();
        return ret;
      }
    }
  }
  return RET_OK;
}

// Copy quant param from quant_para_holder into CNode
int TransformUint8Pass::CopyQuantParam(const CNodePtr &cnode) {
  auto cnode_primitve = GetValueNode<PrimitivePtr>(cnode->input(0));
  CHECK_NULL_RETURN(cnode_primitve);
  auto quant_param_holder = GetCNodeQuantHolder(cnode);
  CHECK_NULL_RETURN(quant_param_holder);
  if (opt::CheckPrimitiveType(cnode, prim::kPrimQuantDTypeCast)) {
    cnode_primitve->AddAttr(quant::kQuantType, MakeValue(static_cast<int>(quant::QUANT_NONE)));
  } else {
    auto quant_type = quant_param_holder->quant_type();
    cnode_primitve->AddAttr(quant::kQuantType, MakeValue(static_cast<int>(quant_type)));
  }
  auto input_quant_params = quant_param_holder->get_input_quant_params();
  auto output_quant_params = quant_param_holder->get_output_quant_params();
  if (quant_param_holder->IsOutputExistInited()) {
    std::vector<ValuePtr> quantization_list;
    for (size_t index = 0; index < output_quant_params.size(); index++) {
      auto quantization_ptr = ConvertQuantParamTToQuantizationParam(output_quant_params[index]);
      if (quantization_ptr != nullptr) {
        quantization_list.push_back(quantization_ptr);
      }
    }
    cnode_primitve->AddAttr(quant::kQuantParam, std::make_unique<ValueList>(quantization_list));
  } else {
    MS_LOG(DEBUG) << cnode->fullname_with_scope() << " output quant params empty.";
  }

  if (quant_param_holder->IsInputExistInited()) {
    for (size_t index = 1; index < cnode->size(); index++) {
      auto input_node = cnode->input(index);
      CHECK_NULL_RETURN(input_node);
      // If quant_param not exist, skip
      if ((static_cast<int>(index) - kPrimOffset) >= static_cast<int>(input_quant_params.size())) {
        continue;
      }
      auto input_quant_param = input_quant_params.at(static_cast<int>(index) - kPrimOffset);
      if (input_quant_param.empty()) {
        MS_LOG(DEBUG) << cnode->fullname_with_scope() << " input node index: " << index << " quant param is empty.";
        continue;
      }
      if (IsGraphInput(input_node)) {
        auto quantization_param = quant::ConvertQuantParamTToQuantizationParam(input_quant_param);
        cnode_primitve->AddAttr(quant::kGraphInputQuantParam, quantization_param);
      } else if (input_node->isa<mindspore::CNode>()) {
        // input node has single output
        continue;
      } else if (input_node->isa<mindspore::Parameter>()) {
        auto parameter_node = input_node->cast<ParameterPtr>();
        CHECK_NULL_RETURN(parameter_node);
        auto tensor_info = parameter_node->default_param()->cast<tensor::TensorPtr>();
        CHECK_NULL_RETURN(tensor_info);
        auto quantization_ptr = quant::ConvertQuantParamTToQuantizationParam(input_quant_param);
        CHECK_NULL_RETURN(quantization_ptr);
        tensor_info->set_quant_param(std::vector<QuantizationParamPtr>{quantization_ptr});
      } else if (input_node->isa<mindspore::ValueNode>()) {
        auto value_node = input_node->cast<ValueNodePtr>();
        CHECK_NULL_RETURN(value_node);
        auto tensor_info = value_node->value()->cast<tensor::TensorPtr>();
        CHECK_NULL_RETURN(tensor_info);
        auto quantization_ptr = quant::ConvertQuantParamTToQuantizationParam(input_quant_param);
        CHECK_NULL_RETURN(quantization_ptr);
        tensor_info->set_quant_param(std::vector<QuantizationParamPtr>{quantization_ptr});
      } else {
        MS_LOG(ERROR) << input_node->fullname_with_scope() << ":" << input_node->type_name() << " not supported.";
        return RET_ERROR;
      }
    }
  } else {
    MS_LOG(DEBUG) << cnode->fullname_with_scope() << " input quant params is empty.";
  }
  return RET_OK;
}

bool TransformUint8Pass::CheckNeedDTypeTrans(const CNodePtr &cnode) {
  if (opt::IsSpecialType(cnode) || CheckControlFlowType(cnode)) {
    return false;
  }

  // if CastNode(U8toInt8 or Int8toU8), do nonthing
  if (CheckCastNodeUint8Int8(cnode)) {
    return false;
  }

  // If CastNode(kDeQuant) as graph input node, or CastNode(kQuant) as graph output node, do nothing.
  CastNodeType cast_node_type = kNone;
  auto status = quant::GetCastNodeType(func_graph_, cnode, &cast_node_type);
  if (status == RET_OK) {
    if ((cast_node_type == kDeQuant && IsGraphInDTypeCast(cnode)) ||
        (IsGraphOutDTypeCast(func_graph_, cnode) && cast_node_type == kQuant)) {
      return false;
    }
  } else if (status != RET_NOT_SUPPORT) {
    MS_LOG(ERROR) << "Get cast node type failed, cnode name: " << cnode->fullname_with_scope();
    return false;
  }

  TypeId cnode_dtype = kTypeUnknown;
  if (opt::GetDataTypeFromAnfNode(cnode, &cnode_dtype) != RET_OK) {
    MS_LOG(INFO) << "Get data type failed, cnode name: " << cnode->fullname_with_scope();
    return false;
  }
  bool is_fp32_output =
    opt::CheckPrimitiveType(cnode, prim::kPrimQuantDTypeCast) || CheckNodeInSet(cnode, kUint8toFP32Operator);
  if (cnode_dtype != kNumberTypeUInt8 && !is_fp32_output) {
    MS_LOG(DEBUG) << "dtype not kNumberTypeUInt8, cnode name: " << cnode->fullname_with_scope();
    return false;
  }
  auto curr_quant_param_holder = GetCNodeQuantHolder(cnode);
  if (curr_quant_param_holder->get_output_quant_params().empty()) {
    return false;
  }
  return true;
}

bool TransformUint8Pass::CheckCastNodeUint8Int8(const CNodePtr &cnode) {
  if (opt::CheckPrimitiveType(cnode, prim::kPrimQuantDTypeCast)) {
    auto prim = GetValueNode<std::shared_ptr<mindspore::Primitive>>(cnode->input(kPrimIndex));
    if (prim == nullptr) {
      MS_LOG(ERROR) << "Get prim from value node failed.";
      return false;
    }
    auto primc = api::MakeShared<mindspore::ops::QuantDTypeCast>(prim);
    MS_CHECK_TRUE_MSG(primc != nullptr, false, "cast ptr failed.");
    auto src_type = primc->get_src_t();
    auto dst_type = primc->get_dst_t();
    if ((src_type == kNumberTypeUInt8 && dst_type == kNumberTypeInt8) ||
        (src_type == kNumberTypeInt8 && dst_type == kNumberTypeUInt8)) {
      return true;
    }
  }
  return false;
}

bool TransformUint8Pass::IsSharedWeightParameter(const AnfNodePtr &anf_node) {
  auto manager = this->func_graph_->manager();
  if (manager == nullptr) {
    manager = Manage(this->func_graph_, true);
  }
  MS_CHECK_TRUE_MSG(manager != nullptr, false, "manage is nullptr.");
  auto node_users = manager->node_users()[anf_node];
  return (node_users.size() > 1);
}
}  // namespace mindspore::lite::quant
