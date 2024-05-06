/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_SOFTMAX_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_SOFTMAX_GPU_KERNEL_H_

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/softmax_impl.cuh"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/kernel_constants.h"

namespace mindspore {
namespace kernel {
class SoftmaxGpuKernelMod final : public NativeGpuKernelMod {
 public:
  SoftmaxGpuKernelMod() = default;
  ~SoftmaxGpuKernelMod() = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    return kernel_func_(this, inputs, workspace, outputs, stream_ptr);
  }
  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs, void *stream_ptr) noexcept;

  using SoftmaxGpuLaunchFunc = std::function<bool(SoftmaxGpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                                                  const std::vector<kernel::KernelTensor *> &,
                                                  const std::vector<kernel::KernelTensor *> &, void *)>;

  void ResetResource() noexcept {
    outer_size_ = 1;
    inner_size_ = 1;
    shape_.clear();
  }

  size_t GetAccAxis(KernelTensor *axis_kernel_tensor) const noexcept;

  bool is_null_input_{false};
  size_t shape_size_{0};
  std::vector<size_t> shape_{};
  size_t axis_acc_{0};
  size_t outer_size_{1};
  size_t inner_size_{1};
  bool is_log_softmax_{false};

  SoftmaxGpuLaunchFunc kernel_func_;
  static std::vector<std::pair<KernelAttr, SoftmaxGpuLaunchFunc>> func_list_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_NN_SOFTMAX_GPU_KERNEL_H_
