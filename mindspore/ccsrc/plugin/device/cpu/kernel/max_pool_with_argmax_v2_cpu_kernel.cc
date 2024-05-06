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

#include "plugin/device/cpu/kernel/max_pool_with_argmax_v2_cpu_kernel.h"
#include "mindspore/core/ops/max_pool_with_argmax_v2.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kMaxPoolWithArgmaxV2InputNum = 1;
constexpr size_t kMaxPoolWithArgmaxV2OutputsNum = 2;
constexpr int64_t kIndexBatch = 0;
constexpr int64_t kIndexChannel = 1;
constexpr int64_t kIndexHeight = 2;
constexpr int64_t kIndexWidth = 3;
}  // namespace

bool MaxPoolWithArgmaxV2CpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &outputs) {
  x_dtype_ = inputs[kIndex0]->dtype_id();
  argmax_dtype_ = outputs[kIndex1]->dtype_id();
  ksize_list_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kKernelSize));
  strides_list_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kStrides));
  pads_list_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kPads));
  dilation_list_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kDilation));

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For " << kernel_name_ << ", it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int MaxPoolWithArgmaxV2CpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }

  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMaxPoolWithArgmaxV2InputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMaxPoolWithArgmaxV2OutputsNum, kernel_name_);
  x_shape_ = inputs[kIndex0]->GetShapeVector();
  y_shape_ = outputs[kIndex0]->GetShapeVector();
  argmax_shape_ = outputs[kIndex1]->GetShapeVector();
  input_size_ = SizeOf(x_shape_);
  return KRET_OK;
}

std::vector<int64_t> MaxPoolWithArgmaxV2CpuKernelMod::GetValidAttr(const std::vector<int64_t> &src_attr) const {
  if (src_attr.size() == kShape1dDims) {
    return {src_attr[kDim0], src_attr[kDim0]};
  } else if (src_attr.size() == kShape4dDims) {
    return {src_attr[kDim2], src_attr[kDim3]};
  } else {
    return {src_attr[kDim0], src_attr[kDim1]};
  }
}

template <typename DATA_T, typename INDICES_T>
bool MaxPoolWithArgmaxV2CpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                                   const std::vector<KernelTensor *> &,
                                                   const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMaxPoolWithArgmaxV2InputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMaxPoolWithArgmaxV2OutputsNum, kernel_name_);
  auto input_x = static_cast<DATA_T *>(inputs[kIndex0]->device_ptr());
  auto output_y = static_cast<DATA_T *>(outputs[kIndex0]->device_ptr());
  auto output_argmax = static_cast<INDICES_T *>(outputs[kIndex1]->device_ptr());
  const int64_t in_width = x_shape_[kIndexWidth];
  const int64_t in_height = x_shape_[kIndexHeight];
  const int64_t in_channel = x_shape_[kIndexChannel];
  const int64_t in_batch = x_shape_[kIndexBatch];
  const int64_t out_width = y_shape_[kIndexWidth];
  const int64_t out_height = y_shape_[kIndexHeight];

  auto valid_ksize_list = GetValidAttr(ksize_list_);
  auto valid_strides_list = GetValidAttr(strides_list_);
  auto valid_pads_list = GetValidAttr(pads_list_);
  auto valid_dilation_list = GetValidAttr(dilation_list_);
  const int64_t k_width = valid_ksize_list[kDim1];
  const int64_t k_height = valid_ksize_list[kDim0];
  const int64_t s_width = valid_strides_list[kDim1];
  const int64_t s_height = valid_strides_list[kDim0];
  const int64_t p_width = valid_pads_list[kDim1];
  const int64_t p_height = valid_pads_list[kDim0];
  const int64_t d_width = valid_dilation_list[kDim1];
  const int64_t d_height = valid_dilation_list[kDim0];
  // parallel task
  auto task = [input_x, output_y, output_argmax, &in_channel, &in_height, &in_width, &out_height, &out_width, &k_height,
               &k_width, &s_height, &s_width, &p_height, &p_width, &d_height, &d_width,
               this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      MaxPoolWithArgmaxV2SingleCompute(input_x, output_y, output_argmax, i, in_channel, in_height, in_width, out_height,
                                       out_width, k_height, k_width, s_height, s_width, p_height, p_width, d_height,
                                       d_width);
    }
  };
  ParallelLaunchAutoSearch(task, in_batch * in_channel * out_width * out_height, this, &parallel_search_info_);
  return true;
}

template <typename DATA_T, typename INDICES_T>
void MaxPoolWithArgmaxV2CpuKernelMod::MaxPoolWithArgmaxV2SingleCompute(DATA_T *input, DATA_T *output_y,
                                                                       INDICES_T *output_argmax, int64_t i,
                                                                       int64_t in_channel, int64_t iH, int64_t iW,
                                                                       int64_t oH, int64_t oW, int64_t kH, int64_t kW,
                                                                       int64_t sH, int64_t sW, int64_t pH, int64_t pW,
                                                                       int64_t dH, int64_t dW) const {
  const int pos_n = i / (in_channel * oH * oW);
  const int pos_c = i / (oH * oW) % in_channel;
  const int pos_h = i / oW % oH;
  const int pos_w = i % oW;
  int start_h = pos_h * sH - pH;
  int start_w = pos_w * sW - pW;
  int end_h = std::min<int>(start_h + (kH - 1) * dH + 1, iH);
  int end_w = std::min<int>(start_w + (kW - 1) * dW + 1, iW);
  if (start_h < 0) {
    start_h += ceil(-start_h / static_cast<double>(dH)) * dH;
  }
  if (start_w < 0) {
    start_w += ceil(-start_w / static_cast<double>(dW)) * dW;
  }
  INDICES_T input_start = pos_n * in_channel * iH * iW;
  INDICES_T stride = pos_c * iH * iW;
  INDICES_T max_idx = stride + start_h * iW + start_w;
  DATA_T max_data = static_cast<DATA_T>(-std::numeric_limits<DATA_T>::max());
  for (int cur_h = start_h; cur_h < end_h; cur_h += dH) {
    for (int cur_w = start_w; cur_w < end_w; cur_w += dW) {
      INDICES_T input_idx = stride + cur_h * iW + cur_w;
      int index = input_start + input_idx;
      DATA_T input_data = static_cast<DATA_T>(0);
      if (index >= 0 && index < input_size_) {
        input_data = input[index];
      }
      if (input_data > max_data) {
        max_idx = input_idx - stride;
        max_data = input_data;
      }
    }
  }
  output_y[i] = max_data;
  output_argmax[i] = max_idx;
}

#define ADD_KERNEL(x_dtype, shape_dtype, x_type, shape_type)             \
  {                                                                      \
    KernelAttr()                                                         \
      .AddInputAttr(kNumberType##x_dtype)                                \
      .AddOutputAttr(kNumberType##x_dtype)                               \
      .AddOutputAttr(kNumberType##shape_dtype),                          \
      &MaxPoolWithArgmaxV2CpuKernelMod::LaunchKernel<x_type, shape_type> \
  }

std::vector<std::pair<KernelAttr, MaxPoolWithArgmaxV2CpuKernelMod::MaxPoolWithArgmaxV2Func>>
  MaxPoolWithArgmaxV2CpuKernelMod::func_list_ = {
    ADD_KERNEL(Float16, Int32, float16, int32_t), ADD_KERNEL(Float32, Int32, float, int32_t),
    ADD_KERNEL(Float64, Int32, double, int32_t),  ADD_KERNEL(Int8, Int32, int8_t, int32_t),
    ADD_KERNEL(Int16, Int32, int16_t, int32_t),   ADD_KERNEL(Int32, Int32, int32_t, int32_t),
    ADD_KERNEL(Int64, Int32, int64_t, int32_t),   ADD_KERNEL(UInt8, Int32, uint8_t, int32_t),
    ADD_KERNEL(UInt16, Int32, uint16_t, int32_t), ADD_KERNEL(UInt32, Int32, uint32_t, int32_t),
    ADD_KERNEL(UInt64, Int32, uint64_t, int32_t), ADD_KERNEL(Float16, Int64, float16, int64_t),
    ADD_KERNEL(Float32, Int64, float, int64_t),   ADD_KERNEL(Float64, Int64, double, int64_t),
    ADD_KERNEL(Int8, Int64, int8_t, int64_t),     ADD_KERNEL(Int16, Int64, int16_t, int64_t),
    ADD_KERNEL(Int32, Int64, int32_t, int64_t),   ADD_KERNEL(Int64, Int64, int64_t, int64_t),
    ADD_KERNEL(UInt8, Int64, uint8_t, int64_t),   ADD_KERNEL(UInt16, Int64, uint16_t, int64_t),
    ADD_KERNEL(UInt32, Int64, uint32_t, int64_t), ADD_KERNEL(UInt64, Int64, uint64_t, int64_t)};

std::vector<KernelAttr> MaxPoolWithArgmaxV2CpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MaxPoolWithArgmaxV2Func> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, MaxPoolWithArgmaxV2, MaxPoolWithArgmaxV2CpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
