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

#include <complex>
#include "plugin/device/cpu/kernel/sinc_cpu_kernel.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kSincInputsNum = 1;
constexpr size_t kSincOutputsNum = 1;
}  // namespace

bool SincCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

template <typename T>
bool SincCpuKernelMod::LaunchSameKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                        const std::vector<kernel::KernelTensor *> &,
                                        const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kSincInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kSincOutputsNum, kernel_name_);
  auto input = static_cast<T *>(inputs[0]->device_ptr());
  auto output = static_cast<T *>(outputs[0]->device_ptr());
  size_t total = inputs[0]->size() / sizeof(T);
  auto task = [&input, &output](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      if (static_cast<T>(input[i]) == static_cast<T>(0.0f)) {
        output[i] = static_cast<T>(1.0f);
      } else {
        T pi = static_cast<T>(3.14159265358979323846L);
        T product = pi * input[i];
        output[i] = sin(product) / product;
      }
    }
  };
  ParallelLaunchAutoSearch(task, total, this, &parallel_search_info_);
  return true;
}

template <typename T>
bool SincCpuKernelMod::LaunchDiffKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                        const std::vector<kernel::KernelTensor *> &,
                                        const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kSincInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kSincOutputsNum, kernel_name_);
  auto input = static_cast<T *>(inputs[0]->device_ptr());
  auto output = static_cast<float *>(outputs[0]->device_ptr());
  size_t total = inputs[0]->size() / sizeof(T);
  auto task = [&input, &output](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      if (input[i] == static_cast<T>(0)) {
        output[i] = static_cast<float>(1.0f);
      } else {
        float pi = static_cast<float>(3.14159265358979323846);
        float product = pi * input[i];
        output[i] = sin(product) / product;
      }
    }
  };
  ParallelLaunchAutoSearch(task, total, this, &parallel_search_info_);
  return true;
}

template <typename T>
bool SincCpuKernelMod::LaunchBoolKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                        const std::vector<kernel::KernelTensor *> &,
                                        const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kSincInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kSincOutputsNum, kernel_name_);
  auto input = static_cast<bool *>(inputs[0]->device_ptr());
  auto output = static_cast<float *>(outputs[0]->device_ptr());
  size_t total = inputs[0]->size() / sizeof(T);
  auto task = [&input, &output](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      float tmp;
      if (input[i] == true) {
        tmp = 1.0f;
      } else {
        tmp = 0.0f;
      }
      float pi = 3.14159265358979323846;
      float product = pi * tmp;
      output[i] = sin(product) / product;
    }
  };
  ParallelLaunchAutoSearch(task, total, this, &parallel_search_info_);
  return true;
}

const std::vector<std::pair<KernelAttr, SincCpuKernelMod::KernelRunFunc>> &SincCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, SincCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchDiffKernel<uint8_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchDiffKernel<int8_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchDiffKernel<uint16_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchDiffKernel<int16_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchDiffKernel<uint32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchDiffKernel<int32_t>},
    {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchDiffKernel<uint64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchDiffKernel<int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
     &SincCpuKernelMod::LaunchSameKernel<float16>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchSameKernel<float>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
     &SincCpuKernelMod::LaunchSameKernel<double>},
    {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex64),
     &SincCpuKernelMod::LaunchSameKernel<std::complex<float>>},
    {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex128),
     &SincCpuKernelMod::LaunchSameKernel<std::complex<double>>},
    {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat32),
     &SincCpuKernelMod::LaunchBoolKernel<bool>}};
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Sinc, SincCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
