/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MAP_UNIFORM_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MAP_UNIFORM_CPU_KERNEL_H_

#include <map>
#include <cmath>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/kernel/cpu_kernel.h"
namespace mindspore {
namespace kernel {
class MapUniformCpuKernelMod : public NativeCpuKernelMod {
 public:
  MapUniformCpuKernelMod() = default;
  ~MapUniformCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    MS_EXCEPTION_IF_NULL(kernel_func_);
    return kernel_func_(this, inputs, workspace, outputs);
  }

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs, const std::vector<kernel::KernelTensor *> &,
                    const std::vector<kernel::KernelTensor *> &outputs);

  using MapUniformFunc =
    std::function<bool(MapUniformCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;
  static std::vector<std::pair<KernelAttr, MapUniformFunc>> func_list_;
  MapUniformFunc kernel_func_;

  size_t batch_size_{1};
  TypeId dtype_{kTypeUnknown};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_MAP_UNIFORM_CPU_KERNEL_H_
