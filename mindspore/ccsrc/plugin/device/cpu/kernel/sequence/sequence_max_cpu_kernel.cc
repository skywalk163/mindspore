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

#include "plugin/device/cpu/kernel/sequence/sequence_max_cpu_kernel.h"
#include <algorithm>
#include <complex>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "include/common/thread_pool.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr int kInputsNum = 1;
constexpr int kOutputsNum = 1;
}  // namespace
bool SequenceMaxCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputsNum, kernel_name_);
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int SequenceMaxCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  return KRET_OK;
}

template <typename T>
bool SequenceMaxCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &,
                                           const std::vector<KernelTensor *> &outputs) {
  T *input_addr = GetDeviceAddress<T>(inputs, 0);
  T *output_addr = GetDeviceAddress<T>(outputs, 0);
  auto input_size = inputs[0]->size();
  output_addr[0] = *std::max_element(input_addr, input_addr + input_size / sizeof(T));
  return true;
}

const std::vector<std::pair<KernelAttr, SequenceMaxCpuKernelMod::KernelRunFunc>> &SequenceMaxCpuKernelMod::GetFuncList()
  const {
  static const std::vector<std::pair<KernelAttr, SequenceMaxCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr()
       .AddInputAttr(kObjectTypeTuple, kNumberTypeFloat32)
       .AddOutputAttr(kObjectTypeNumber, kNumberTypeFloat32),
     &SequenceMaxCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kObjectTypeTuple, kNumberTypeFloat64)
       .AddOutputAttr(kObjectTypeNumber, kNumberTypeFloat64),
     &SequenceMaxCpuKernelMod::LaunchKernel<double>},
    {KernelAttr().AddInputAttr(kObjectTypeTuple, kNumberTypeInt32).AddOutputAttr(kObjectTypeNumber, kNumberTypeInt32),
     &SequenceMaxCpuKernelMod::LaunchKernel<int>},
    {KernelAttr().AddInputAttr(kObjectTypeTuple, kNumberTypeInt64).AddOutputAttr(kObjectTypeNumber, kNumberTypeInt64),
     &SequenceMaxCpuKernelMod::LaunchKernel<int64_t>}};
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, SequenceMax, SequenceMaxCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
