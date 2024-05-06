/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/math/assign_sub_gpu_kernel.h"

namespace mindspore {
namespace kernel {
bool AssignSubFwdGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  constexpr int INPUT_NUM = 2;
  if (inputs.size() != INPUT_NUM) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs should be 2, but got " << inputs.size();
  }
  constexpr int OUTPUT_NUM = 1;
  if (outputs.size() != OUTPUT_NUM) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of outputs should be 1, but got " << outputs.size();
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' dose not support this kernel type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  input_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex0).dtype);
  std::vector<int64_t> input_shape_ = std::vector<int64_t>(inputs.at(kIndex0)->GetDeviceShapeVector().begin(),
                                                           inputs.at(kIndex0)->GetDeviceShapeVector().end());
  input_elements_ = std::accumulate(input_shape_.begin(), input_shape_.end(), 1, std::multiplies<int64_t>());
  is_null_input_ = (input_elements_ == 0);
  if (is_null_input_) {
    InitSizeLists();
    return true;
  }
  for (int64_t i = 0; i < static_cast<int64_t>(input_shape_.size()); i++) {
    input_size_ *= input_shape_[i];
  }
  InitSizeLists();
  return true;
}

int AssignSubFwdGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  std::vector<int64_t> input_shape_ = std::vector<int64_t>(inputs.at(kIndex0)->GetDeviceShapeVector().begin(),
                                                           inputs.at(kIndex0)->GetDeviceShapeVector().end());
  ResetResource();
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  input_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex0).dtype);
  for (int64_t i = 0; i < static_cast<int64_t>(input_shape_.size()); i++) {
    input_size_ *= input_shape_[i];
  }
  InitSizeLists();
  return KRET_OK;
}

template <typename T>
bool AssignSubFwdGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &workspace,
                                            const std::vector<KernelTensor *> &outputs) {
  T *ref = GetDeviceAddress<T>(inputs, kIndex0);
  T *value = GetDeviceAddress<T>(inputs, kIndex1);
  T *output = GetDeviceAddress<T>(outputs, kIndex0);
  auto status =
    CalAssignSub(input_size_ / sizeof(T), ref, value, output, device_id_, reinterpret_cast<cudaStream_t>(stream_ptr_));
  CHECK_CUDA_STATUS(status, kernel_name_);
  return true;
}

std::vector<std::pair<KernelAttr, AssignSubFwdGpuKernelMod::AssignSubFunc>> AssignSubFwdGpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
   &AssignSubFwdGpuKernelMod::LaunchKernel<uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &AssignSubFwdGpuKernelMod::LaunchKernel<int>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
   &AssignSubFwdGpuKernelMod::LaunchKernel<int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &AssignSubFwdGpuKernelMod::LaunchKernel<int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &AssignSubFwdGpuKernelMod::LaunchKernel<double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &AssignSubFwdGpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &AssignSubFwdGpuKernelMod::LaunchKernel<half>}};

std::vector<KernelAttr> AssignSubFwdGpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, AssignSubFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, AssignSub, AssignSubFwdGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
