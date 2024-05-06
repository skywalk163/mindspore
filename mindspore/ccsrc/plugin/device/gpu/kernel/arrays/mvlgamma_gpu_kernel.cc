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

#include "plugin/device/gpu/kernel/arrays/mvlgamma_gpu_kernel.h"

namespace mindspore {
namespace kernel {
bool MvlgammaGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' got empty inputs or outputs, which is invalid.";
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the kernel type should be in [float32, float64], but got: " << kernel_attr << ".";
    return false;
  }
  kernel_func_ = func_list_[index].second;
  unit_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex0).dtype);
  p_ = GetValue<int64_t>(primitive_->GetAttr(ops::kP));
  if (p_ < 1) {
    MS_LOG(ERROR) << "For " << kernel_name_ << ", the attr 'p' has to be greater than or equal to 1, "
                  << "but got " << p_ << ".";
    return false;
  }
  return true;
}

int MvlgammaGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  for (const auto &input : inputs) {
    // If any input shape contains -1, means input shape is dynamic, so just
    // return do nothing.
    auto input_shape = input->GetShapeVector();
    if (!IsValidShape(input_shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  ResetResource();
  std::vector<int64_t> input_shape = std::vector<int64_t>(inputs.at(kIndex0)->GetDeviceShapeVector().begin(),
                                                          inputs.at(kIndex0)->GetDeviceShapeVector().end());
  input_elements_ = std::accumulate(input_shape.begin(), input_shape.end(), 1, std::multiplies<int64_t>());
  if (input_elements_ == 0) {
    is_null_input_ = true;
  }
  int64_t input_dims = input_shape.size();
  if (input_dims < 1) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'x' should be at least 1-D, but got " << input_dims
                  << "-D.";
    return KRET_RESIZE_FAILED;
  }
  size_t input_size = input_elements_ * unit_size_;
  output_size_list_.push_back(input_size);
  workspace_size_list_.push_back(sizeof(int));
  return KRET_OK;
}

template <typename T>
bool MvlgammaGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &workspace,
                                        const std::vector<KernelTensor *> &outputs) {
  T *input = GetDeviceAddress<T>(inputs, 0);
  T *output = GetDeviceAddress<T>(outputs, 0);
  int *valid_d = GetDeviceAddress<int>(workspace, 0);
  int host_valid = -1;
  auto status = CalMvlgamma(valid_d, input_elements_, input, p_, output, device_id_,
                            reinterpret_cast<cudaStream_t>(cuda_stream_), &host_valid);
  CHECK_CUDA_STATUS(status, kernel_name_);
  if (host_valid >= 0) {
    MS_EXCEPTION(ValueError) << "For " << kernel_name_ << ", all elements of 'x' must be greater than (p-1)/2";
  }
  return true;
}

std::vector<std::pair<KernelAttr, MvlgammaGpuKernelMod::MvlgammaFunc>> MvlgammaGpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &MvlgammaGpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &MvlgammaGpuKernelMod::LaunchKernel<double>}};

std::vector<KernelAttr> MvlgammaGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MvlgammaFunc> &pair) { return pair.first; });
  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, Mvlgamma, MvlgammaGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
