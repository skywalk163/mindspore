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
#include "plugin/device/cpu/kernel/rl/batch_assign_cpu_kernel.h"
#include <memory>
#include <functional>
#include "kernel/common_utils.h"
namespace mindspore {
namespace kernel {
constexpr size_t kHalf = 2;
std::shared_mutex BatchAssignCpuBaseMod::rw_mutex_;

BatchAssignCpuKernelMod::BatchAssignCpuKernelMod() : elements_num_(0), lock_(false) {}

int BatchAssignCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  lock_ = GetValue<bool>(primitive_->GetAttr("lock"));
  size_t input_num = inputs.size();
  elements_num_ = input_num / kHalf;

  // Set an output for placeholder.
  output_size_list_.clear();
  output_size_list_.push_back(sizeof(float));
  return KRET_OK;
}

bool BatchAssignCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                     const std::vector<KernelTensor *> &) {
  if (lock_) {
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
  } else {
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
  }
  // Usually, we will get two inputs list, the first half are the weights to be updated, and the last half
  // are the sources. So we just copy the source to overwrite the dst.
  auto task = [this, &inputs](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      auto local_addr = GetDeviceAddress<unsigned char>(inputs, i);
      auto source_addr = GetDeviceAddress<unsigned char>(inputs, i + elements_num_);
      MS_EXCEPTION_IF_NULL(local_addr);
      MS_EXCEPTION_IF_NULL(source_addr);
      auto ret = memcpy_s(local_addr, inputs[i]->size(), source_addr, inputs[i + elements_num_]->size());
      if (ret != EOK) {
        MS_LOG(EXCEPTION) << kernel_name_ << " memcpy failed, errorno(" << ret << ")";
      }
    }
  };
  ParallelLaunchAutoSearch(task, elements_num_, this, &parallel_search_info_);
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, BatchAssign, BatchAssignCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
