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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_APPLY_ADA_MAX_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_APPLY_ADA_MAX_CPU_KERNEL_H_

#include <algorithm>
#include <vector>
#include <memory>
#include <string>
#include <map>

#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class ApplyAdaMaxCpuKernelMod : public NativeCpuKernelMod {
 public:
  ApplyAdaMaxCpuKernelMod() = default;
  ~ApplyAdaMaxCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override {
    static const std::vector<KernelAttr> support_list = {KernelAttr()
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddInputAttr(kNumberTypeFloat32)
                                                           .AddOutputAttr(kNumberTypeFloat32)
                                                           .AddOutputAttr(kNumberTypeFloat32)
                                                           .AddOutputAttr(kNumberTypeFloat32)
                                                           .AddOutInRef(0, 0)
                                                           .AddOutInRef(1, 1)
                                                           .AddOutInRef(2, 2),
                                                         KernelAttr()
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddInputAttr(kNumberTypeFloat16)
                                                           .AddOutputAttr(kNumberTypeFloat16)
                                                           .AddOutputAttr(kNumberTypeFloat16)
                                                           .AddOutputAttr(kNumberTypeFloat16)
                                                           .AddOutInRef(0, 0)
                                                           .AddOutInRef(1, 1)
                                                           .AddOutInRef(2, 2)};
    return support_list;
  }

 private:
  TypeId dtype_{kTypeUnknown};
  int64_t batch_size_{1};
  int64_t batch_rank_{0};
  int64_t input_elements_;
  template <typename T>
  void LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_APPLY_ADA_MAX_CPU_KERNEL_H_
