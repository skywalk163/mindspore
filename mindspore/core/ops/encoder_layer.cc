
/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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

#include "ops/encoder_layer.h"

#include "mindapi/base/shared_ptr.h"
#include "mindapi/ir/value.h"
#include "mindapi/src/helper.h"
#include "mindspore/core/ops/nn_ops.h"
#include "ops/op_name.h"
#include "ops/primitive_c.h"
#include "utils/log_adapter.h"

namespace mindspore::ops {
MIND_API_OPERATOR_IMPL(EncoderLayer, BaseOperator);

void EncoderLayer::set_head_num(int64_t head_num) { (void)this->AddAttr(kNumHeads, api::MakeValue(head_num)); }

void EncoderLayer::set_head_size(int64_t head_size) { (void)this->AddAttr(kSizePerHead, api::MakeValue(head_size)); }

void EncoderLayer::set_post_layernorm(bool post_layernorm) {
  (void)this->AddAttr(kPostLayernorm, api::MakeValue(post_layernorm));
}

void EncoderLayer::set_eps_layernorm1(float eps_layernorm1) {
  (void)this->AddAttr(kEpsLayerNorm1, api::MakeValue(eps_layernorm1));
}

void EncoderLayer::set_eps_layernorm2(float eps_layernorm2) {
  (void)this->AddAttr(kEpsLayerNorm2, api::MakeValue(eps_layernorm2));
}

void EncoderLayer::set_eps_layernorm3(float eps_layernorm3) {
  (void)this->AddAttr(kEpsLayerNorm3, api::MakeValue(eps_layernorm3));
}
void EncoderLayer::set_ffn_hidden_size(int64_t ffn_hidden_size) {
  (void)this->AddAttr(kFfnHiddenSize, api::MakeValue(ffn_hidden_size));
}
void EncoderLayer::set_expert_num(int64_t expert_num) { (void)this->AddAttr(kExpertNum, api::MakeValue(expert_num)); }
void EncoderLayer::set_expert_offset_id(int64_t expert_offset_id) {
  (void)this->AddAttr(kExpertOffsetId, api::MakeValue(expert_offset_id));
}
void EncoderLayer::set_capacity_factor(float capacity_factor) {
  (void)this->AddAttr(kCapacityFactor, api::MakeValue(capacity_factor));
}
void EncoderLayer::set_position_bias(bool position_bias) {
  (void)this->AddAttr(kPositionBias1, api::MakeValue(position_bias));
}
void EncoderLayer::set_scale(float scale) { (void)this->AddAttr(kScale, api::MakeValue(scale)); }
void EncoderLayer::set_layer_norm(bool layer_norm) { (void)this->AddAttr(kLayerNorm, api::MakeValue(layer_norm)); }

void EncoderLayer::set_act_type(ActType act_type) {
  (void)this->AddAttr(kActivationType, api::MakeValue(static_cast<int32_t>(act_type)));
}
void EncoderLayer::set_use_past(bool use_past) { (void)this->AddAttr(kUsePast, api::MakeValue(use_past)); }
void EncoderLayer::set_query_layer(bool query_layer) { (void)this->AddAttr(kQueryLayer, api::MakeValue(query_layer)); }
void EncoderLayer::set_moe(bool moe) { (void)this->AddAttr(kMoe, api::MakeValue(moe)); }
void EncoderLayer::set_embedding_layer(bool embedding_layer) {
  (void)this->AddAttr(kEmbeddingLayer, api::MakeValue(embedding_layer));
}

int64_t EncoderLayer::get_head_num() const {
  auto value_ptr = this->GetAttr(kNumHeads);
  return GetValue<int64_t>(value_ptr);
}

int64_t EncoderLayer::get_head_size() const {
  auto value_ptr = this->GetAttr(kSizePerHead);
  return GetValue<int64_t>(value_ptr);
}

bool EncoderLayer::get_post_layernorm() const {
  auto value_ptr = this->GetAttr(kPostLayernorm);
  return GetValue<bool>(value_ptr);
}

float EncoderLayer::get_eps_layernorm1() const {
  auto value_ptr = this->GetAttr(kEpsLayerNorm1);
  return GetValue<float>(value_ptr);
}

float EncoderLayer::get_eps_layernorm2() const {
  auto value_ptr = this->GetAttr(kEpsLayerNorm2);
  return GetValue<float>(value_ptr);
}

float EncoderLayer::get_eps_layernorm3() const {
  auto value_ptr = this->GetAttr(kEpsLayerNorm3);
  return GetValue<float>(value_ptr);
}

int64_t EncoderLayer::get_ffn_hidden_size() const {
  auto value_ptr = this->GetAttr(kFfnHiddenSize);
  return GetValue<int64_t>(value_ptr);
}

int64_t EncoderLayer::get_expert_num() const {
  auto value_ptr = this->GetAttr(kExpertNum);
  return GetValue<int64_t>(value_ptr);
}

int64_t EncoderLayer::get_expert_offset_id() const {
  auto value_ptr = this->GetAttr(kExpertOffsetId);
  return GetValue<int64_t>(value_ptr);
}

float EncoderLayer::get_capacity_factor() const {
  auto value_ptr = this->GetAttr(kCapacityFactor);
  return GetValue<float>(value_ptr);
}

bool EncoderLayer::get_position_bias() const {
  auto value_ptr = this->GetAttr(kPositionBias1);
  return GetValue<bool>(value_ptr);
}

float EncoderLayer::get_scale() const {
  auto value_ptr = this->GetAttr(kScale);
  return GetValue<float>(value_ptr);
}

ActType EncoderLayer::get_act_type() const {
  auto value_ptr = GetAttr(kActivationType);
  if (value_ptr == nullptr) {
    return ActType::ActType_No;
  }
  return ActType(GetValue<int64_t>(value_ptr));
}

bool EncoderLayer::get_layer_norm() const {
  auto value_ptr = this->GetAttr(kLayerNorm);
  return GetValue<bool>(value_ptr);
}

bool EncoderLayer::get_use_past() const {
  auto value_ptr = this->GetAttr(kUsePast);
  return GetValue<bool>(value_ptr);
}

bool EncoderLayer::get_query_layer() const {
  auto value_ptr = this->GetAttr(kQueryLayer);
  return GetValue<bool>(value_ptr);
}

bool EncoderLayer::get_moe() const {
  auto value_ptr = this->GetAttr(kMoe);
  return GetValue<bool>(value_ptr);
}

bool EncoderLayer::get_embedding_layer() const {
  auto value_ptr = this->GetAttr(kEmbeddingLayer);
  if (value_ptr == nullptr) {
    return false;
  }
  return GetValue<bool>(value_ptr);
}
void EncoderLayer::Init(int64_t head_num, int64_t head_size, float eps_layernorm1, float eps_layernorm2,
                        float eps_layernorm3, int64_t ffn_hidden_size, int64_t expert_num, int64_t expert_offset_id,
                        float capacity_factor, bool position_bias, bool post_layernorm, float scale, ActType act_type,
                        bool layer_norm, bool use_past, bool query_layer, bool moe, bool embedding_layer) {
  this->set_head_num(head_num);
  this->set_head_size(head_size);
  this->set_post_layernorm(post_layernorm);
  this->set_eps_layernorm1(eps_layernorm1);
  this->set_eps_layernorm2(eps_layernorm2);
  this->set_eps_layernorm3(eps_layernorm3);
  this->set_ffn_hidden_size(ffn_hidden_size);
  this->set_expert_num(expert_num);
  this->set_expert_offset_id(expert_offset_id);
  this->set_capacity_factor(capacity_factor);
  this->set_position_bias(position_bias);
  this->set_act_type(act_type);
  this->set_scale(scale);
  this->set_layer_norm(layer_norm);
  this->set_use_past(use_past);
  this->set_query_layer(query_layer);
  this->set_moe(moe);
  this->set_embedding_layer(embedding_layer);
}
REGISTER_PRIMITIVE_C(kNameEncoderLayer, EncoderLayer);
}  // namespace mindspore::ops
