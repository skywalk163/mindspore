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

#include "ops/dynamic_quant.h"
#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(DynamicQuant, BaseOperator);
void DynamicQuant::set_symmetric(const bool symmetric) { (void)AddAttr(kSymmetric, api::MakeValue(symmetric)); }
bool DynamicQuant::get_symmetric() const {
  auto value_ptr = this->GetAttr(kSymmetric);
  return GetValue<bool>(value_ptr);
}
void DynamicQuant::set_dst_type(const int64_t dst_type) { (void)AddAttr(kDstType, api::MakeValue(dst_type)); }
int64_t DynamicQuant::get_dst_type() const { return GetValue<int64_t>(GetAttr(kDstType)); }
void DynamicQuant::set_prefer_axis(const int64_t prefer_axis) {
  (void)AddAttr(kPreferAxis, api::MakeValue(prefer_axis));
}
int64_t DynamicQuant::get_prefer_axis() const { return GetValue<int64_t>(GetAttr(kPreferAxis)); }
void DynamicQuant::set_activation_channel(const bool activation_channel) {
  (void)AddAttr(kActivationChannel, api::MakeValue(activation_channel));
}
bool DynamicQuant::get_activation_channel() const {
  auto value_ptr = this->GetAttr(kActivationChannel);
  return GetValue<bool>(value_ptr);
}
void DynamicQuant::set_transpose(const bool transpose) { (void)AddAttr(kTrans, api::MakeValue(transpose)); }
bool DynamicQuant::get_transpose() const {
  auto value_ptr = this->GetAttr(kTrans);
  return GetValue<bool>(value_ptr);
}

void DynamicQuant::set_prefer_axes(const std::vector<int> &prefer_axes) {
  (void)AddAttr(kPreferAxes, api::MakeValue(prefer_axes));
}

std::vector<int> DynamicQuant::get_prefer_axes() const {
  auto value_ptr = GetAttr(kPreferAxes);
  auto tmp = GetValue<std::vector<int64_t>>(value_ptr);
  std::vector<int> res(tmp.begin(), tmp.end());
  return res;
}

void DynamicQuant::Init(const bool symmetric, const int64_t dst_type) {
  this->set_symmetric(symmetric);
  this->set_dst_type(dst_type);
  this->set_activation_channel(false);
  this->set_prefer_axis(0);
  this->set_transpose(false);
}

REGISTER_PRIMITIVE_C(kNameDynamicQuant, DynamicQuant);
}  // namespace ops
}  // namespace mindspore
