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

#include "plugin/device/cpu/kernel/sequence/shape_mul_cpu_kernel.h"
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
bool ShapeMulCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputsNum, kernel_name_);
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int ShapeMulCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  input_shape_ = inputs.at(kIndex0)->GetShapeVector();
  if (input_shape_.size() != 1) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', input_shape size must be 1, but got " << GetShapes(inputs);
  }
  return KRET_OK;
}

template <typename T>
bool ShapeMulCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                        const std::vector<KernelTensor *> &outputs) {
  int64_t *input_addr = GetDeviceAddress<int64_t>(inputs, 0);
  int64_t *output_addr = GetDeviceAddress<int64_t>(outputs, 0);
  MS_EXCEPTION_IF_NULL(output_addr);
  *output_addr = 1;
  for (int64_t i = 0; i < input_shape_[0]; i++) {
    *output_addr *= input_addr[i];
  }

  return true;
}  // namespace kernel

const std::vector<std::pair<KernelAttr, ShapeMulCpuKernelMod::KernelRunFunc>> &ShapeMulCpuKernelMod::GetFuncList()
  const {
  static const std::vector<std::pair<KernelAttr, ShapeMulCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr().AddInputAttr(kObjectTypeTuple, kNumberTypeInt64).AddOutputAttr(kObjectTypeNumber, kNumberTypeInt64),
     &ShapeMulCpuKernelMod::LaunchKernel<int64_t>}};
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, shape_mul, ShapeMulCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
