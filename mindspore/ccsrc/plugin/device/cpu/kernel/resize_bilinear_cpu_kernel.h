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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_RESIZE_BILINEAR_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_RESIZE_BILINEAR_CPU_KERNEL_H_

#include <memory>
#include <map>
#include <utility>
#include <vector>
#include <algorithm>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class ResizeBilinearCpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<ResizeBilinearCpuKernelMod> {
 public:
  ResizeBilinearCpuKernelMod() = default;
  ~ResizeBilinearCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    if (is_null_input_) {
      return true;
    }
    MS_EXCEPTION_IF_NULL(kernel_func_);
    return kernel_func_(this, inputs, workspace, outputs);
  }

  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

  std::vector<size_t> GetLaunchIgnoredInputAddressIdx() const override { return {kIndex1}; }

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                    const std::vector<KernelTensor *> &outputs) const;
  bool LaunchFloat16Kernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                           const std::vector<KernelTensor *> &outputs) const;
  bool align_corners_{false};
  bool half_pixel_centers_{false};
  bool is_null_input_{false};
  float height_scale{1.0};
  float width_scale{1.0};
  std::vector<size_t> output_shape_;
  std::vector<size_t> shape_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_RESIZE_BILINEAR_CPU_KERNEL_H_
