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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RESIZE_NEAREST_NEIGHBOR_V2_GRAD_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RESIZE_NEAREST_NEIGHBOR_V2_GRAD_CPU_KERNEL_H_

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include <utility>
#include "kernel/common_utils.h"
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class ResizeNearestNeighborV2GradCpuKernelMod : public NativeCpuKernelMod {
 public:
  ResizeNearestNeighborV2GradCpuKernelMod() = default;
  ~ResizeNearestNeighborV2GradCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T, typename S>
  void RealCompute(T *const input, S *const output);

  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);
  using ResizeNearestNeighborV2GradLaunchFunc =
    std::function<bool(ResizeNearestNeighborV2GradCpuKernelMod *, const std::vector<KernelTensor *> &,
                       const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &)>;
  static std::vector<std::pair<KernelAttr, ResizeNearestNeighborV2GradLaunchFunc>> func_list_;
  ResizeNearestNeighborV2GradLaunchFunc kernel_func_;

  TypeId y_type_{kTypeUnknown};
  size_t y_size_;
  bool align_corners_{false};
  bool half_pixel_centers_{false};
  std::vector<int64_t> grads_shape_;
  std::vector<int64_t> y_shape_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_RESIZE_NEAREST_NEIGHBOR_V2_GRAD_CPU_KERNEL_H_
