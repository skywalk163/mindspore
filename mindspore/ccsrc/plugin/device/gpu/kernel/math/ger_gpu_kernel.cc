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

#include "plugin/device/gpu/kernel/math/ger_gpu_kernel.h"
#include <functional>
#include <utility>
#include <string>
#include <algorithm>

namespace mindspore {
namespace kernel {
bool GerGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' got empty inputs or outputs, which is invalid.";
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' does not support this kernel type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  unit_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex0).dtype);
  return true;
}

int GerGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  for (const auto &input : inputs) {
    // If any input shape contains -1, means input shape is dynamic, so just return do nothing.
    auto input_shape = input->GetShapeVector();
    if (!IsValidShape(input_shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  ResetResource();
  std::vector<int64_t> output_shape = std::vector<int64_t>(outputs.at(kIndex0)->GetDeviceShapeVector().begin(),
                                                           outputs.at(kIndex0)->GetDeviceShapeVector().end());
  output_elements_ = std::accumulate(output_shape.begin(), output_shape.end(), 1, std::multiplies<int64_t>());
  if (output_elements_ == 0) {
    is_null_input_ = true;
  }
  std::vector<int64_t> clo_shape = std::vector<int64_t>(inputs.at(kIndex0)->GetDeviceShapeVector().begin(),
                                                        inputs.at(kIndex0)->GetDeviceShapeVector().end());
  std::vector<int64_t> row_shape = std::vector<int64_t>(inputs.at(kIndex1)->GetDeviceShapeVector().begin(),
                                                        inputs.at(kIndex1)->GetDeviceShapeVector().end());
  if (clo_shape.size() != 1 || row_shape.size() != 1) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'x1' and 'x2' should be 1-D.";
    return KRET_RESIZE_FAILED;
  }
  input_elements_ = clo_shape[0] + row_shape[0];
  output_elements_ = clo_shape[0] * row_shape[0];
  matrix_row_ = clo_shape[0];
  matrix_col_ = row_shape[0];
  int64_t output_size = output_elements_ * unit_size_;
  output_size_list_.emplace_back(output_size);
  return KRET_OK;
}

void GerGpuKernelMod::ResetResource() noexcept {
  matrix_row_ = 0;
  matrix_col_ = 0;
  input_elements_ = 0;
  output_elements_ = 0;
  output_size_list_.clear();
  workspace_size_list_.clear();
}

template <typename T>
bool GerGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &workspace,
                                   const std::vector<KernelTensor *> &outputs) {
  T *col_input = GetDeviceAddress<T>(inputs, 0);
  T *row_input = GetDeviceAddress<T>(inputs, 1);
  T *output = GetDeviceAddress<T>(outputs, 0);
  auto status = CalGer(output_elements_, row_input, col_input, matrix_row_, matrix_col_, output, device_id_,
                       reinterpret_cast<cudaStream_t>(cuda_stream_));
  CHECK_CUDA_STATUS(status, kernel_name_);
  return true;
}

std::vector<std::pair<KernelAttr, GerGpuKernelMod::GerFunc>> GerGpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &GerGpuKernelMod::LaunchKernel<half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &GerGpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &GerGpuKernelMod::LaunchKernel<double>}};

std::vector<KernelAttr> GerGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, GerFunc> &pair) { return pair.first; });
  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, Ger, GerGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
