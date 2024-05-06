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

#include <algorithm>
#include <memory>
#include "plugin/device/gpu/kernel/nn/maxpool_with_argmax_v2_gpu_kernel.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/maxpool_with_argmax_v2_impl.cuh"
#include "mindspore/core/ops/max_pool_with_argmax_v2.h"

namespace mindspore {
namespace kernel {
constexpr auto kMaxPoolWithArgmaxV2 = "MaxPoolWithArgmaxV2";
constexpr size_t kInputDimLowerLimit = 4;
constexpr size_t kOutputDimLowerLimit = 4;
constexpr size_t kInputNum = 1;
constexpr size_t kOutputNum = 2;
constexpr int64_t kIndexBatch = 0;
constexpr int64_t kIndexChannel = 1;
constexpr int64_t kIndexHeight = 2;
constexpr int64_t kIndexWidth = 3;

template <typename T, typename S>
bool MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                      const std::vector<kernel::KernelTensor *> &outputs) {
  T *input_addr = GetDeviceAddress<T>(inputs, kIndex0);
  T *output_addr = GetDeviceAddress<T>(outputs, kIndex0);
  S *index_addr = GetDeviceAddress<S>(outputs, kIndex1);

  auto status = CalMaxPoolWithArgmaxV2(
    input_addr, in_n_, in_c_, in_h_, in_w_, ksize_h_, ksize_w_, strides_h_, strides_w_, pads_h_, pads_w_, dilation_h_,
    dilation_w_, out_h_, out_w_, output_addr, index_addr, device_id_, reinterpret_cast<cudaStream_t>(cuda_stream_));
  CHECK_CUDA_STATUS(status, kernel_name_);
  return true;
}

std::vector<int> GetAttrFromOpsPrim(const std::vector<int64_t> &attr) {
  if (attr.size() == kShape1dDims) {
    return {LongToInt(attr[kDim0]), LongToInt(attr[kDim0])};
  } else if (attr.size() == kShape4dDims) {
    return {LongToInt(attr[kDim2]), LongToInt(attr[kDim3])};
  } else {
    return {LongToInt(attr[kDim0]), LongToInt(attr[kDim1])};
  }
}

bool MaxPoolWithArgmaxV2FwdGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                              const std::vector<KernelTensor *> &outputs) {
  auto ksize = GetValue<std::vector<int64_t>>(primitive_->GetAttr("kernel_size"));
  auto strides = GetValue<std::vector<int64_t>>(primitive_->GetAttr("strides"));
  auto pads = GetValue<std::vector<int64_t>>(primitive_->GetAttr("pads"));
  auto dilation = GetValue<std::vector<int64_t>>(primitive_->GetAttr("dilation"));

  auto ksize_v = GetAttrFromOpsPrim(ksize);
  ksize_h_ = ksize_v[kDim0];
  ksize_w_ = ksize_v[kDim1];

  auto strides_v = GetAttrFromOpsPrim(strides);
  strides_h_ = strides_v[kDim0];
  strides_w_ = strides_v[kDim1];

  auto pads_v = GetAttrFromOpsPrim(pads);
  pads_h_ = pads_v[kDim0];
  pads_w_ = pads_v[kDim1];

  auto dilation_v = GetAttrFromOpsPrim(dilation);
  dilation_h_ = dilation_v[kDim0];
  dilation_w_ = dilation_v[kDim1];

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' it does not support this kernel type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int MaxPoolWithArgmaxV2FwdGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                               const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }
  if (inputs.size() != kInputNum) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the number of inputs should be " << kInputNum << ", but got "
                  << inputs.size();
    return KRET_RESIZE_FAILED;
  }
  if (outputs.size() != kOutputNum) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the number of outputs should be " << kOutputNum << ", but got "
                  << outputs.size();
    return KRET_RESIZE_FAILED;
  }

  auto input_shape = inputs[kIndex0]->GetShapeVector();
  auto output_shape = outputs[kIndex0]->GetShapeVector();
  is_null_input_ =
    CHECK_SHAPE_NULL(input_shape, kernel_name_, "input") || CHECK_SHAPE_NULL(output_shape, kernel_name_, "output");
  if (is_null_input_) {
    return KRET_RESIZE_FAILED;
  }
  if ((input_shape.size() < kInputDimLowerLimit) || (output_shape.size() < kOutputDimLowerLimit)) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of input and output cannot be less than "
                  << kOutputDimLowerLimit << ", but got the dimension of input: " << input_shape.size()
                  << ", the dimension of output: " << output_shape.size();
    return KRET_RESIZE_FAILED;
  }

  in_n_ = LongToInt(input_shape[kIndexBatch]);
  in_c_ = LongToInt(input_shape[kIndexChannel]);
  in_h_ = LongToInt(input_shape[kIndexHeight]);
  in_w_ = LongToInt(input_shape[kIndexWidth]);

  out_h_ = LongToInt(output_shape[kIndexHeight]);
  out_w_ = LongToInt(output_shape[kIndexWidth]);

  return KRET_OK;
}

std::vector<std::pair<KernelAttr, MaxPoolWithArgmaxV2FwdGpuKernelMod::MaxPoolWithArgmaxV2Func>>
  MaxPoolWithArgmaxV2FwdGpuKernelMod::func_list_ = {
    {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<half, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<float, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<double, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<int8_t, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<int16_t, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<int32_t, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<int64_t, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<uint8_t, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<uint16_t, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<uint32_t, int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt32),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<uint64_t, int32_t>},

    {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<half, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<float, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<double, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<int8_t, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<int16_t, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<int32_t, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<int64_t, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<uint8_t, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<uint16_t, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<uint32_t, int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt64),
     &MaxPoolWithArgmaxV2FwdGpuKernelMod::LaunchKernel<uint64_t, int64_t>},
};

std::vector<KernelAttr> MaxPoolWithArgmaxV2FwdGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MaxPoolWithArgmaxV2Func> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeGpuKernelMod, MaxPoolWithArgmaxV2, []() {
  return std::make_shared<MaxPoolWithArgmaxV2FwdGpuKernelMod>(kMaxPoolWithArgmaxV2);
});
}  // namespace kernel
}  // namespace mindspore
