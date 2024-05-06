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

#include "plugin/device/cpu/kernel/mkldnn/lstm_cpu_kernel.h"
#include <string>
#include "mindspore/core/ops/lstm.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kLstmInputsNum = 4;
constexpr size_t kLstmOutputsNum = 5;
constexpr int kMaxLSTMLayer = 100;
constexpr int kOutputWorkSpaceIndex = 3;
constexpr int kInputCIndex = 2;
constexpr int kInputWeightIndex = 3;
constexpr int kGateNum = 4;

using tag = dnnl::memory::format_tag;
using dim = dnnl::memory::dims;
using dt = dnnl::memory::data_type;
}  // namespace

void LstmCpuKernelMod::InitOutputSize(const std::vector<KernelTensor *> &outputs) {
  output_size_list_[kOutputWorkSpaceIndex] = reserve_size_;
  size_t len = reserve_size_ / IntToSize(kGateNum);
  outputs[kOutputWorkSpaceIndex]->SetShapeVector({SizeToLong(len), 1});
}

bool LstmCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.size() != kLstmInputsNum || outputs.size() != kLstmOutputsNum) {
    MS_LOG(ERROR) << kernel_name_ << ": input and output size should be " << kLstmInputsNum << " and "
                  << kLstmOutputsNum << ", but get " << inputs.size() << " and " << outputs.size();
    return false;
  }
  bidirectional_ = GetValue<bool>(KernelMod::primitive_->GetAttr(ops::kBidirectional));
  input_size_ = GetValue<int64_t>(KernelMod::primitive_->GetAttr(ops::kInputSize));
  hidden_size_ = GetValue<int64_t>(KernelMod::primitive_->GetAttr(ops::kHidden_size));
  num_layers_ = GetValue<int64_t>(KernelMod::primitive_->GetAttr(ops::kNumLayers));
  has_bias_ = GetValue<bool>(KernelMod::primitive_->GetAttr(ops::kHasBias));
  proj_size_ = GetValue<int64_t>(KernelMod::primitive_->GetAttr(ops::kProjection_size));
  real_hidden_size_ = proj_size_ > 0 ? proj_size_ : hidden_size_;
  constexpr int kBidirectional = 2;
  num_directions_ = 1;
  if (bidirectional_) {
    num_directions_ = kBidirectional;
  }
  const int gate_size = kGateNum * hidden_size_;
  if (num_layers_ <= 0) {
    MS_LOG(EXCEPTION) << "Layers must be greater than zero!";
  }
  if (num_layers_ > kMaxLSTMLayer) {
    MS_LOG(EXCEPTION) << "Layers must be lower than 100!";
  }

  weight_size_ = 0;
  weight_h_size_ = 0;
  weight_r_size_ = 0;
  for (int i = 0; i < num_layers_; ++i) {
    weight_size_ += gate_size * (i == 0 ? input_size_ : hidden_size_ * num_directions_);
    weight_h_size_ += gate_size * real_hidden_size_;
    weight_r_size_ += hidden_size_ * proj_size_;
  }
  weight_size_ = weight_size_ * num_directions_;
  weight_h_size_ = weight_h_size_ * num_directions_;
  weight_r_size_ = weight_r_size_ * num_directions_;
  weights_dims_ = {num_layers_, num_directions_, input_size_, kGateNum, hidden_size_};
  weights_h_dims_ = {num_layers_, num_directions_, real_hidden_size_, kGateNum, hidden_size_};
  weights_r_dims_ = {num_layers_, num_directions_, hidden_size_, proj_size_};
  bias_dims_ = {num_layers_, num_directions_, kGateNum, hidden_size_};
  is_training_ = KernelMod::primitive_->HasAttr(kAttrIsTraining)
                   ? GetValue<bool>(KernelMod::primitive_->GetAttr(kAttrIsTraining))
                   : true;
  return true;
}

int LstmCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  auto src_shape = inputs[kIndex0]->GetShapeVector();
  auto src_h_shape = inputs[kIndex1]->GetShapeVector();
  auto src_c_shape = inputs[kIndex2]->GetShapeVector();
  if (src_shape.size() != 3 || src_h_shape.size() != 3 || src_c_shape.size() != 3) {
    MS_LOG(EXCEPTION) << "Lstm only support 3-D input!";
  }
  batch_size_ = src_shape[1];
  seq_len_ = src_shape[0];

  if (num_directions_ * num_layers_ != src_h_shape[0]) {
    MS_LOG(EXCEPTION) << "Error iteration shape!";
  }
  dnnl::rnn_direction direction = dnnl::rnn_direction::unidirectional;
  if (bidirectional_) {
    direction = dnnl::rnn_direction::bidirectional_concat;
  }
  dim src_dims = {seq_len_, batch_size_, input_size_};
  dim src_h_dims = {num_layers_, num_directions_, batch_size_, real_hidden_size_};
  dim src_c_dims = {num_layers_, num_directions_, batch_size_, hidden_size_};
  dim dst_dims = {seq_len_, batch_size_, real_hidden_size_ * num_directions_};
  dim dst_h_dims = {num_layers_, num_directions_, batch_size_, real_hidden_size_};
  dim dst_c_dims = {num_layers_, num_directions_, batch_size_, hidden_size_};
  dnnl::memory::desc src_desc = formatted_md(src_dims, tag::tnc);
  dnnl::memory::desc src_h_desc = formatted_md(src_h_dims, tag::ldnc);
  dnnl::memory::desc src_c_desc = formatted_md(src_c_dims, tag::ldnc);
  dnnl::memory::desc bias_desc = formatted_md(bias_dims_, tag::ldgo);
  dnnl::memory::desc dst_desc = formatted_md(dst_dims, tag::tnc);
  dnnl::memory::desc dst_h_desc = formatted_md(dst_h_dims, tag::ldnc);
  dnnl::memory::desc dst_c_desc = formatted_md(dst_c_dims, tag::ldnc);

  auto prop_kind = is_training_ ? dnnl::prop_kind::forward_training : dnnl::prop_kind::forward_inference;
  auto weights_desc = formatted_md(weights_dims_, tag::any);
  auto weights_h_desc = formatted_md(weights_h_dims_, tag::any);
  auto weights_r_desc = proj_size_ > 0 ? formatted_md(weights_r_dims_, tag::any) : dnnl::memory::desc();
  auto peephole_desc = dnnl::memory::desc();
  auto desc = CreatePrimitive<dnnl::lstm_forward::desc>(prop_kind, direction, src_desc, src_h_desc, src_c_desc,
                                                        weights_desc, weights_h_desc, peephole_desc, weights_r_desc,
                                                        bias_desc, dst_desc, dst_h_desc, dst_c_desc);
  prim_desc_ = CreateDesc<dnnl::lstm_forward::primitive_desc>(*desc, engine_);
  primitive_ = CreatePrimitive<dnnl::lstm_forward>(prim_desc_);
  auto weights_layer = GetWeightsLayerDesc(prim_desc_);
  auto weights_iter = GetWeightsIterDesc(prim_desc_);
  auto weights_proj = GetWeightsProjectionDesc(prim_desc_);
  bias_desc_ = GetBiasDesc(prim_desc_);
  if (is_training_) {
    auto wksp_desc = GetWorkspaceDesc(prim_desc_);
    reserve_size_ = GetSize(wksp_desc);
    AddArgument(DNNL_ARG_WORKSPACE, wksp_desc);
  }
  AddArgument(DNNL_ARG_SRC_LAYER, src_desc);
  AddArgument(DNNL_ARG_SRC_ITER, src_h_desc);
  AddArgument(DNNL_ARG_SRC_ITER_C, src_c_desc);
  AddArgument(DNNL_ARG_WEIGHTS_LAYER, weights_layer);
  AddArgument(DNNL_ARG_WEIGHTS_ITER, weights_iter);
  AddArgument(DNNL_ARG_WEIGHTS_PROJECTION, weights_proj);
  AddArgument(DNNL_ARG_BIAS, bias_desc);
  AddArgument(DNNL_ARG_DST_LAYER, dst_desc);
  AddArgument(DNNL_ARG_DST_ITER, dst_h_desc);
  AddArgument(DNNL_ARG_DST_ITER_C, dst_c_desc);

  auto weights_dims_desc = CreateDesc<dnnl::memory::desc>(weights_dims_, dt::f32, tag::ldgoi);
  auto weights_h_dims_desc = CreateDesc<dnnl::memory::desc>(weights_h_dims_, dt::f32, tag::ldgoi);
  auto weights_r_dims_desc = CreateDesc<dnnl::memory::desc>(weights_r_dims_, dt::f32, tag::ldoi);
  user_weights_memory_ = CreateDesc<dnnl::memory>(weights_dims_desc, engine_);
  user_weights_h_memory_ = CreateDesc<dnnl::memory>(weights_h_dims_desc, engine_);
  user_weights_r_memory_ = CreateDesc<dnnl::memory>(weights_r_dims_desc, engine_);
  weights_memory_ = CreateDesc<dnnl::memory>(weights_layer, engine_);
  weights_h_memory_ = CreateDesc<dnnl::memory>(weights_iter, engine_);
  weights_r_memory_ = CreateDesc<dnnl::memory>(weights_proj, engine_);
  bias_memory_ = CreateDesc<dnnl::memory>(bias_desc_, engine_);

  InitOutputSize(outputs);
  return KRET_OK;
}

bool LstmCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                              const std::vector<kernel::KernelTensor *> &,
                              const std::vector<kernel::KernelTensor *> &outputs) {
  size_t offset = 0;
  SetDataHandle(user_weights_memory_, inputs[kInputWeightIndex]->device_ptr());
  offset += weight_size_;
  SetDataHandle(user_weights_h_memory_, reinterpret_cast<float *>(inputs[kInputWeightIndex]->device_ptr()) + offset);
  offset += weight_h_size_;
  Reorder(&user_weights_memory_, &weights_memory_);
  Reorder(&user_weights_h_memory_, &weights_h_memory_);
  if (proj_size_ > 0) {
    SetDataHandle(user_weights_r_memory_, reinterpret_cast<float *>(inputs[kInputWeightIndex]->device_ptr()) + offset);
    Reorder(&user_weights_r_memory_, &weights_r_memory_);
    offset += weight_r_size_;
  }
  if (has_bias_) {
    SetDataHandle(bias_memory_, reinterpret_cast<float *>(inputs[kInputWeightIndex]->device_ptr()) + offset);
  } else {
    auto size = GetSize(bias_desc_);
    if (memset_s(GetDataHandle(bias_memory_), size, 0, size) != EOK) {
      MS_LOG(EXCEPTION) << "Bias memset error";
    }
  }
  // set handle
  SetArgumentHandle(DNNL_ARG_SRC_LAYER, inputs[0]->device_ptr());
  SetArgumentHandle(DNNL_ARG_SRC_ITER, inputs[1]->device_ptr());
  SetArgumentHandle(DNNL_ARG_SRC_ITER_C, inputs[kInputCIndex]->device_ptr());
  SetArgumentHandle(DNNL_ARG_WEIGHTS_LAYER, GetDataHandle(weights_memory_));
  SetArgumentHandle(DNNL_ARG_WEIGHTS_ITER, GetDataHandle(weights_h_memory_));
  SetArgumentHandle(DNNL_ARG_WEIGHTS_PROJECTION, GetDataHandle(weights_r_memory_));
  SetArgumentHandle(DNNL_ARG_BIAS, GetDataHandle(bias_memory_));
  SetArgumentHandle(DNNL_ARG_DST_LAYER, outputs[0]->device_ptr());
  SetArgumentHandle(DNNL_ARG_DST_ITER, outputs[1]->device_ptr());
  SetArgumentHandle(DNNL_ARG_DST_ITER_C, outputs[2]->device_ptr());
  if (is_training_) {
    SetArgumentHandle(DNNL_ARG_WORKSPACE, outputs[kOutputWorkSpaceIndex]->device_ptr());
  }
  ExecutePrimitive();
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, LSTM, LstmCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
