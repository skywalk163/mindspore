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
#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_LOWER_BOUND_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_LOWER_BOUND_CPU_KERNEL_H_

#include <vector>
#include <utility>
#include <memory>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
namespace mindspore {
namespace kernel {
class LowerBoundCpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<LowerBoundCpuKernelMod> {
 public:
  LowerBoundCpuKernelMod() = default;
  ~LowerBoundCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

 private:
  template <typename I, typename O>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                    const std::vector<kernel::KernelTensor *> &outputs);
  std::vector<int64_t> sorted_x_shape_;
  std::vector<int64_t> values_shape_;
  std::vector<int64_t> output_shape_;
  size_t sorted_x_num_;
  size_t values_num_;
  size_t output_num_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_LOWER_BOUND_CPU_KERNEL_H_
