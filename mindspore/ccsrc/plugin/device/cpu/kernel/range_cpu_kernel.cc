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

#include "plugin/device/cpu/kernel/range_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kRangeInputsNum = 4;
constexpr size_t kRangeOutputsNum = 1;

template <typename T>
T Sign(T num) {
  if (num > static_cast<T>(0.0)) {
    return static_cast<T>(1.0);
  } else if (num == static_cast<T>(0.0)) {
    return static_cast<T>(0.0);
  } else {
    return static_cast<T>(-1.0);
  }
}
}  // namespace

bool RangeCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kRangeInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kRangeOutputsNum, kernel_name_);
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

template <typename T>
bool RangeCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                     const std::vector<KernelTensor *> &outputs) {
  auto start = reinterpret_cast<T *>(inputs[0]->device_ptr())[0];
  auto limit = reinterpret_cast<T *>(inputs[1]->device_ptr())[0];
  auto delta = reinterpret_cast<T *>(inputs[2]->device_ptr())[0];
  if (delta == static_cast<T>(0)) {
    MS_LOG(ERROR) << "For " << kernel_name_ << ", the delta can not be 0.";
    return false;
  }

  auto output = reinterpret_cast<T *>(outputs[0]->device_ptr());
  size_t output_size = outputs[0]->size() / sizeof(T);
  if (Sign(delta) * Sign(limit - start) >= 0) {
    for (int index = 0; index < SizeToInt(output_size); index++) {
      output[index] = delta * index + start;
    }
  } else {
    MS_LOG(ERROR) << "For " << kernel_name_ << ", upper bound and larger bound inconsistent with step sign.";
    return false;
  }
  return true;
}

const std::vector<std::pair<KernelAttr, RangeCpuKernelMod::KernelRunFunc>> &RangeCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, RangeCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr()
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat32),
     &RangeCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat64),
     &RangeCpuKernelMod::LaunchKernel<double>},
    {KernelAttr()
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt32),
     &RangeCpuKernelMod::LaunchKernel<int32_t>},
    {KernelAttr()
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64),
     &RangeCpuKernelMod::LaunchKernel<int64_t>},
  };
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Range, RangeCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
