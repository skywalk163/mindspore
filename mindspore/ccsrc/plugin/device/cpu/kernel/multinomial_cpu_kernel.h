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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MULTINOMIAL_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MULTINOMIAL_CPU_KERNEL_H_
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <map>
#include <utility>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "nnacl/base/tile_base.h"
#include "mindspore/core/ops/multinomial.h"

namespace mindspore {
namespace kernel {
class MultinomialCpuKernelMod : public NativeCpuKernelMod {
 public:
  MultinomialCpuKernelMod() { ResetResource(); }
  ~MultinomialCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  void ResetResource() noexcept;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T_in, typename T_out>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);

  using MultinomialFunc =
    std::function<bool(MultinomialCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;
  ShapeVector input_shape_;
  std::default_random_engine rng_;
  TypeId input0_dtype_{kTypeUnknown};
  TypeId input1_dtype_{kTypeUnknown};
  TypeId output_dtype_{kTypeUnknown};
  void InitInputOutputSize(const CNodePtr &kernel_node);
  MultinomialFunc kernel_func_{};
  static std::vector<std::pair<KernelAttr, MultinomialFunc>> func_list_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MULTINOMIAL_CPU_KERNEL_H
