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

#include "plugin/device/cpu/kernel/scale_grad_cpu_kernel.h"
#include <algorithm>
#include <functional>
#include "mindspore/core/ops/fusion/scale_grad_fusion.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore::kernel {
template <typename T>
void ScaleGradCpuKernelMod::LaunchScaleGradPerGrad(const std::vector<KernelTensor *> &inputs,
                                                   const std::vector<KernelTensor *> &outputs,
                                                   const float16 *scale_addr_half, const float *scale_addr_float,
                                                   size_t index) {
  T *input_addr = GetDeviceAddress<T>(inputs, index);
  T *output_addr = GetDeviceAddress<T>(outputs, index);
  T x1;
  if (scale_addr_half != nullptr) {
    x1 = static_cast<T>(*scale_addr_half);
  } else {
    MS_EXCEPTION_IF_NULL(scale_addr_float);
    x1 = static_cast<T>(*scale_addr_float);
  }

  size_t lens = outputs[index]->size() > 0 ? static_cast<size_t>(outputs[index]->size() / sizeof(T)) : 1;
  auto task = [input_addr, x1, output_addr](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      output_addr[i] = input_addr[i] * x1;
    }
  };
  ParallelLaunchAutoSearch(task, lens, this, &parallel_search_info_);
}

bool ScaleGradCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &workspace,
                                   const std::vector<KernelTensor *> &outputs) {
  float16 *scale_addr_half = nullptr;
  float *scale_addr_float = nullptr;
  if (input_info_.back() == kNumberTypeFloat16) {
    scale_addr_half = GetDeviceAddress<float16>(inputs, inputs.size() - 1);
  } else {
    scale_addr_float = GetDeviceAddress<float>(inputs, inputs.size() - 1);
  }

  for (size_t i = 0; i < inputs.size() - 1; i++) {
    switch (input_info_[i]) {
      case kNumberTypeFloat16: {
        LaunchScaleGradPerGrad<float16>(inputs, outputs, scale_addr_half, scale_addr_float, i);
        break;
      }
      case kNumberTypeFloat32: {
        LaunchScaleGradPerGrad<float>(inputs, outputs, scale_addr_half, scale_addr_float, i);
        break;
      }
      default:
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the typeid cannot be " << input_info_[i];
    }
  }
  return true;
}

std::vector<KernelAttr> ScaleGradCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  support_list.push_back(KernelAttr().AddSkipCheckAttr(true));
  return support_list;
}

bool ScaleGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  auto input_size = inputs.size();
  for (size_t index = 0; index < input_size; index++) {
    auto type_id = inputs[index]->dtype_id();
    input_info_.push_back(type_id);

    if (index < input_size - 1) {
      output_size_list_.push_back(inputs[index]->size());
    }
  }

  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ScaleGrad, ScaleGradCpuKernelMod);
}  // namespace mindspore::kernel
