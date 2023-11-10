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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_SMOOTH_L1_LOSS_GRAD_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_SMOOTH_L1_LOSS_GRAD_GPU_KERNEL_H_

#include <vector>
#include <map>
#include <string>
#include <utility>
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/smooth_l1_loss_impl.cuh"
namespace mindspore {
namespace kernel {
constexpr auto kUnKnown = "UnKnown";
constexpr auto kSmoothL1LossGrad = "SmoothL1LossGrad";
class SmoothL1LossGradGpuKernelMod : public NativeGpuKernelMod {
 public:
  SmoothL1LossGradGpuKernelMod() {}
  ~SmoothL1LossGradGpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    return kernel_func_(this, inputs, workspace, outputs, stream_ptr);
  }

  std::vector<KernelAttr> GetOpSupport() override;

 protected:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs, void *stream_ptr);

  using SmoothL1LossGradFunc = std::function<bool(
    SmoothL1LossGradGpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
    const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &, void *)>;

 private:
  SmoothL1LossGradFunc kernel_func_;
  static std::vector<std::pair<KernelAttr, SmoothL1LossGradFunc>> func_list_;

  float beta_{1.0};
  TypeId dtype_{kTypeUnknown};
  int64_t tensor_size_{1};
  SmoothL1LossReductionMode reduction_{SmoothL1LossReductionMode::NONE};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_SMOOTH_L1_LOSS_GRAD_GPU_KERNEL_H_
