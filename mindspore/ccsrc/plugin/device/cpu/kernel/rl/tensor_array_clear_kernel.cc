/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/rl/tensor_array_clear_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_tensor_array.h"
#include "runtime/device/tensor_array_manager.h"

namespace mindspore {
namespace kernel {
using mindspore::device::TensorArrayMgr;
using mindspore::device::TensorArrayPtr;
TensorArrayClearCpuKernelMod::TensorArrayClearCpuKernelMod() {}

int TensorArrayClearCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  output_size_list_.clear();
  output_size_list_.push_back(sizeof(int64_t));
  return KRET_OK;
}

bool TensorArrayClearCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &) {
  auto handle_addr = GetDeviceAddress<int64_t>(inputs, kIndex0);
  MS_EXCEPTION_IF_NULL(handle_addr);
  TensorArrayPtr tensors_ = TensorArrayMgr::GetInstance().GetTensorArray(handle_addr[0]);
  MS_ERROR_IF_NULL(tensors_);
  // Clear TensorArray valid size, but keep the memory.
  tensors_->Clear();
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, TensorArrayClear, TensorArrayClearCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
