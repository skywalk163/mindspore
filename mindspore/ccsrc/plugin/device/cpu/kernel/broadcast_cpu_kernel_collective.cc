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

#include "plugin/device/cpu/kernel/broadcast_cpu_kernel_collective.h"

#include <set>
#include <string>
#include <functional>
#include <memory>

#if defined(__linux__) && defined(WITH_BACKEND)
#include "plugin/device/cpu/hal/hardware/ms_collective_comm_lib.h"
#endif

namespace mindspore {
namespace kernel {
#if defined(__linux__) && defined(WITH_BACKEND)
using device::cpu::kMCCLGlobalGroupName;
using device::cpu::MsCollectiveCommLib;
#endif

bool BroadcastCPUKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
#if defined(__linux__) && defined(WITH_BACKEND)
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto is_match = MatchKernelAttr(kernel_attr, GetOpSupport()).first;
  if (!is_match) {
    MS_LOG(EXCEPTION) << kernel_name_ << " does not support this kernel data type: " << kernel_attr;
  }
  auto group = GetValue<std::string>(primitive_->GetAttr(GROUP));
  if (group != kMCCLGlobalGroupName) {
    MS_LOG(EXCEPTION) << kernel_name_ << " only support " << kMCCLGlobalGroupName << " on CPU, but got " << group;
  }
  root_rank_ = LongToUint(GetValue<std::int64_t>(primitive_->GetAttr("root_rank")));
  input_dtype_ = inputs[0]->dtype_id();
#else
  MS_LOG(EXCEPTION) << "The CPU kernel broadcast is only supported on linux platform.";
#endif
  return true;
}

std::vector<KernelAttr> BroadcastCPUKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list = {
    KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
    KernelAttr().AddAllSameAttr(true).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32)};
  return support_list;
}

bool BroadcastCPUKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                   const std::vector<kernel::KernelTensor *> &,
                                   const std::vector<kernel::KernelTensor *> &outputs) {
#if defined(__linux__) && defined(WITH_BACKEND)
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(EXCEPTION) << kernel_name_ << " has at least one input and one output, but got 0.";
  }
  std::size_t data_size = 0;
  for (size_t i = 0; i < inputs.size(); ++i) {
    data_size += inputs[i]->size();
  }
  bool ret = MsCollectiveCommLib::GetInstance().Broadcast(inputs[0]->device_ptr(), outputs[0]->device_ptr(),
                                                          data_size / sizeof(float), input_dtype_, root_rank_,
                                                          kMCCLGlobalGroupName);
  if (!ret) {
    MS_LOG(ERROR) << "BroadcastCPUKernelMod launch failed.";
  }
  return ret;
#else
  MS_LOG(EXCEPTION) << "The CPU kernel broadcast is only supported on linux platform.";
#endif
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Broadcast, BroadcastCPUKernelMod);
}  // namespace kernel
}  // namespace mindspore
