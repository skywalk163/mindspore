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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_KL_DIV_LOSS_CPU_KERNEL_H
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_KL_DIV_LOSS_CPU_KERNEL_H

#include <vector>
#include <memory>
#include <utility>
#include <map>
#include <string>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class KLDivLossCpuKernelMod : public NativeCpuKernelMod {
 public:
  KLDivLossCpuKernelMod() {}
  ~KLDivLossCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);

  using KLDivLossFunc = std::function<bool(KLDivLossCpuKernelMod *, const std::vector<KernelTensor *> &,
                                           const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &)>;

 private:
  static std::vector<std::pair<KernelAttr, KLDivLossFunc>> func_list_;
  KLDivLossFunc kernel_func_;
  std::string reductionMode_;
  int64_t batch_size_{0};
  int64_t input_x_shape_size_{1};
  int64_t input_target_shape_size_{1};
  int64_t output_shape_size_{1};
  int64_t output_before_reduction_shape_size_{1};
  std::vector<int64_t> input_x_shape_;
  std::vector<int64_t> input_target_shape_;
  std::vector<int64_t> output_before_reduction_shape_;
  std::mutex mutex_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_KL_DIV_LOSS_CPU_KERNEL_H
