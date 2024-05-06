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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DROPOUT_ND_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DROPOUT_ND_CPU_KERNEL_H_

#include <memory>
#include <vector>
#include <utility>
#include <map>
#include <random>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class DropoutNdCpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<DropoutNdCpuKernelMod> {
 public:
  DropoutNdCpuKernelMod() = default;
  ~DropoutNdCpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

 private:
  bool CheckDropOutNdShape();
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);

  std::vector<size_t> input_shape_;
  std::vector<size_t> output_shape_;
  size_t channels_{1};
  size_t input_elements_{1};
  float keep_prob_{0.0f};
  float scale_{1.0f};
  std::default_random_engine generator_;
  std::bernoulli_distribution distribution_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DROPOUT_ND_CPU_KERNEL_H_
