/**
 * Copyright 2021 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_IGAMMAGRADA_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_IGAMMAGRADA_CPU_KERNEL_H_

#include <vector>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "mindspore/core/ops/grad/igammagrada.h"

namespace mindspore {
namespace kernel {
class IgammaGradACpuKernelMod : public NativeCpuKernelMod {
 public:
  IgammaGradACpuKernelMod() = default;
  ~IgammaGradACpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override {
    static const std::vector<KernelAttr> support_list = {
      KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
      KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64)};
    return support_list;
  }

 private:
  std::vector<int64_t> a_shape_;
  std::vector<int64_t> x_shape_;
  std::vector<int64_t> z_shape_;
  TypeId dtype_{kTypeUnknown};
  template <typename T>
  void LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  template <typename T>
  void BcastCompute(const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &);

  template <typename T>
  void SpecialCompute(int64_t, int64_t, int64_t, const T *, const T *, T *);

  template <typename T>
  void NoBcastCompute(const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &);
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_IGAMMAGRADA_CPU_KERNEL_H_
