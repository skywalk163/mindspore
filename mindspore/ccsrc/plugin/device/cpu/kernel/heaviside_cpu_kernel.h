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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_HEAVISIDE_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_HEAVISIDE_CPU_KERNEL_H_

#include <functional>
#include <vector>
#include <utility>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/device/cpu/kernel/nnacl/arithmetic_parameter.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class HeavisideCpuKernelMod : public NativeCpuKernelMod {
 public:
  HeavisideCpuKernelMod() = default;
  ~HeavisideCpuKernelMod() override = default;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &outputs);

  using HeavisideLaunchFunc = std::function<bool(HeavisideCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                                                 const std::vector<kernel::KernelTensor *> &)>;

  static std::vector<std::pair<KernelAttr, HeavisideLaunchFunc>> func_list_;
  HeavisideLaunchFunc kernel_func_;
  TypeId input0_dtype_{kTypeUnknown};
  TypeId input1_dtype_{kTypeUnknown};
  ShapeVector input0_shape;
  ShapeVector input1_shape;
  ShapeVector output_shape;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_HEAVISIDE_CPU_KERNEL_H_
