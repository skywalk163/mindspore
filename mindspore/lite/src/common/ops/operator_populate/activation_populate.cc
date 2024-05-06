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
#include "src/common/ops/operator_populate/operator_populate_register.h"
#include "nnacl/fp32/activation_fp32.h"
#include "ops/auto_generate/gen_lite_ops.h"
#include "ops/fusion/activation.h"
#include "ops/leaky_relu.h"
#include "ops/tanh.h"
#include "ops/ops_func_impl/hswish.h"
#include "ops/softplus.h"
using mindspore::ops::kActivationType;
using mindspore::ops::kAlpha;
using mindspore::ops::kApproximate;
using mindspore::ops::kMaxVal;
using mindspore::ops::kMinVal;
using mindspore::ops::kNameActivation;
using mindspore::ops::kNameElu;
using mindspore::ops::kNameGeLU;
using mindspore::ops::kNameHSigmoid;
using mindspore::ops::kNameHSwish;
using mindspore::ops::kNameLeakyRelu;
using mindspore::ops::kNameReLU;
using mindspore::ops::kNameReLU6;
using mindspore::ops::kNameSigmoid;
using mindspore::ops::kNameSoftplus;
using mindspore::ops::kNameTanh;
using mindspore::schema::PrimitiveType_Activation;

namespace mindspore {
namespace lite {
OpParameter *PopulateActivationOpParameter(const BaseOperatorPtr &base_operator) {
  auto param = reinterpret_cast<ActivationParameter *>(PopulateOpParameter<ActivationParameter>());
  if (param == nullptr) {
    MS_LOG(ERROR) << "new ActivationParameter failed.";
    return nullptr;
  }
  mindspore::ValuePtr attr = base_operator->GetPrim()->GetAttr(kActivationType);
  if (attr != nullptr) {
    auto activation_type = ActivationType(GetValue<int64_t>(attr));
    static const std::set<ActivationType> activation_types = {RELU,   RELU6,    LEAKY_RELU, SIGMOID, TANH,     SWISH,
                                                              HSWISH, HSIGMOID, HARD_TANH,  GELU,    SOFTPLUS, ELU};
    if (activation_types.find(activation_type) == activation_types.end()) {
      MS_LOG(ERROR) << "invalid activation type: " << activation_type;
      free(param);
      return nullptr;
    }
    param->type_ = static_cast<int>(activation_type);
  } else {
    auto type_name = base_operator->name();
    static const std::map<std::string, ActivationType> op_type_map = {
      {kNameReLU, RELU},         {kNameReLU6, RELU6},   {kNameLeakyRelu, LEAKY_RELU}, {kNameSigmoid, SIGMOID},
      {kNameTanh, TANH},         {kNameHSwish, HSWISH}, {kNameHSigmoid, HSIGMOID},    {kNameGeLU, GELU},
      {kNameSoftplus, SOFTPLUS}, {kNameElu, ELU}};
    auto iter = op_type_map.find(type_name);
    if (iter == op_type_map.end()) {
      MS_LOG(ERROR) << "invalid activation type: " << type_name;
      free(param);
      return nullptr;
    }
    param->type_ = static_cast<int>(iter->second);
  }

  mindspore::ValuePtr alpha = base_operator->GetPrim()->GetAttr(kAlpha);
  if (alpha != nullptr) {
    param->alpha_ = GetValue<float>(alpha);
  }
  mindspore::ValuePtr min_val = base_operator->GetPrim()->GetAttr(kMinVal);
  if (min_val != nullptr) {
    param->min_val_ = GetValue<float>(min_val);
  }
  mindspore::ValuePtr max_val = base_operator->GetPrim()->GetAttr(kMaxVal);
  if (max_val != nullptr) {
    param->max_val_ = GetValue<float>(max_val);
  }
  mindspore::ValuePtr approximate = base_operator->GetPrim()->GetAttr(kApproximate);
  if (approximate != nullptr) {
    param->approximate_ = GetValue<bool>(approximate);
  }

  return reinterpret_cast<OpParameter *>(param);
}

REG_OPERATOR_POPULATE(kNameActivation, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameReLU, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameReLU6, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameLeakyRelu, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameSigmoid, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameTanh, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameHSwish, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameHSigmoid, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameGeLU, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameSoftplus, PrimitiveType_Activation, PopulateActivationOpParameter)
REG_OPERATOR_POPULATE(kNameElu, PrimitiveType_Activation, PopulateActivationOpParameter)
}  // namespace lite
}  // namespace mindspore
