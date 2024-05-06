/**
 * Copyright 2022-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_APPLY_ADAM_WITH_AMSGRAD_V2_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_APPLY_ADAM_WITH_AMSGRAD_V2_CPU_KERNEL_H_

#include <map>
#include <vector>
#include <memory>

#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class ApplyAdamWithAmsgradV2CpuKernelMod : public NativeCpuKernelMod {
 public:
  ApplyAdamWithAmsgradV2CpuKernelMod() = default;
  ~ApplyAdamWithAmsgradV2CpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputsothers) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  void LaunchApplyAdamWithAmsgradV2(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs);

  float beta1_{0.9};
  float beta2_{0.999};
  float epsilon_{1e-8};

  int64_t batch_rank_{0};
  int64_t batch_size_{1};
  size_t input_elements_;
  TypeId dtype_{kTypeUnknown};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_APPLY_ADAM_WITH_AMSGRAD_V2_CPU_KERNEL_H_
