/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_TRUNCATEDNORMAL_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_TRUNCATEDNORMAL_CPU_KERNEL_H_

#include <map>
#include <vector>
#include <random>
#include <utility>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class TruncatedNormalCPUKernelMod : public NativeCpuKernelMod {
 public:
  TruncatedNormalCPUKernelMod() = default;
  ~TruncatedNormalCPUKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T1, typename T2, typename T3>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  using TruncatedNormalFunc =
    std::function<bool(TruncatedNormalCPUKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &)>;
  static std::vector<std::pair<KernelAttr, TruncatedNormalFunc>> func_list_;

  TruncatedNormalFunc kernel_func_;
  TypeId output_type_{kTypeUnknown};
  TypeId input_type_{kTypeUnknown};
  std::default_random_engine rng_;
  bool flag_{true};
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_TRUNCATEDNORMAL_CPU_KERNEL_H_
