/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/gpu/kernel/random/random_standard_laplace_gpu_kernel.h"
#include <functional>
#include <utility>
#include <memory>
#include <string>
#include <algorithm>
#include <condition_variable>
#include "ir/anf.h"
#include "utils/log_adapter.h"
#include "kernel/common_utils.h"
#include "include/cuda_fp16.h"

namespace mindspore {
namespace kernel {
bool StandardLaplaceGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' got empty inputs or outputs, which is invalid.";
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the kernel type should be in [int32, int64], "
                  << "but got: " << kernel_attr << ".";
    return false;
  }
  kernel_func_ = func_list_[index].second;

  unit_input_size_ = abstract::TypeIdSize(kernel_attr.GetInputAttr(kIndex0).dtype);
  unit_output_size_ = abstract::TypeIdSize(kernel_attr.GetOutputAttr(kIndex0).dtype);

  uint64_t seed = static_cast<uint64_t>(GetValue<int64_t>(primitive_->GetAttr("seed")));
  uint64_t seed2 = static_cast<uint64_t>(GetValue<int64_t>(primitive_->GetAttr("seed2")));
  seed_ = random::GetSeed(seed, seed2);
  return true;
}

int StandardLaplaceGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  for (const auto &input : inputs) {
    // If any input shape contains -1, means input shape is dynamic, so just return do nothing.
    auto input_shape = input->GetShapeVector();
    if (!IsValidShape(input_shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  ResetResource();
  std::vector<int64_t> shape_shape = std::vector<int64_t>(inputs.at(kIndex0)->GetDeviceShapeVector().begin(),
                                                          inputs.at(kIndex0)->GetDeviceShapeVector().end());
  int64_t input_dims = shape_shape.size();
  if (input_dims != 1) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'shape' must be 1-D, but got " << input_dims
                  << "-D.";
    return KRET_RESIZE_FAILED;
  }

  std::vector<int64_t> output_shape = std::vector<int64_t>(outputs.at(kIndex0)->GetDeviceShapeVector().begin(),
                                                           outputs.at(kIndex0)->GetDeviceShapeVector().end());
  output_elements_ = std::accumulate(output_shape.begin(), output_shape.end(), 1, std::multiplies<int64_t>());
  if (output_elements_ == 0) {
    is_null_input_ = true;
  }
  output_size_list_.emplace_back(output_elements_ * unit_output_size_);
  workspace_size_list_.push_back(output_elements_ * sizeof(curandState));
  return KRET_OK;
}

template <typename T>
bool StandardLaplaceGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                               const std::vector<KernelTensor *> &workspace,
                                               const std::vector<KernelTensor *> &outputs) {
  T *output = GetDeviceAddress<T>(outputs, 0);
  curandState *devStates = nullptr;
  void *workspace_addr = GetDeviceAddress<void *>(workspace, 0);
  devStates = reinterpret_cast<curandState *>(workspace_addr);
  auto status = StandardLaplace(seed_, seed_offset_, devStates, output, output_elements_,
                                reinterpret_cast<cudaStream_t>(cuda_stream_));
  CHECK_CUDA_STATUS(status, kernel_name_);
  seed_offset_ += 1;
  return true;
}

std::vector<std::pair<KernelAttr, StandardLaplaceGpuKernelMod::SLFunc>> StandardLaplaceGpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
   &StandardLaplaceGpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &StandardLaplaceGpuKernelMod::LaunchKernel<float>}};

std::vector<KernelAttr> StandardLaplaceGpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, SLFunc> &pair) { return pair.first; });
  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, StandardLaplace, StandardLaplaceGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
