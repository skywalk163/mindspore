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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_CUMSUM_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_CUMSUM_GPU_KERNEL_H_

#include <vector>
#include <memory>
#include <map>
#include <utility>
#include <algorithm>
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/cumsum_impl.cuh"
#include "utils/ms_context.h"

namespace mindspore {
namespace kernel {
constexpr int kMaxDimsSize = 3;
class CumSumGpuKernelMod : public NativeGpuKernelMod {
 public:
  CumSumGpuKernelMod() = default;
  ~CumSumGpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    cuda_stream_ = reinterpret_cast<cudaStream_t>(stream_ptr);
    return kernel_func_(this, inputs, workspace, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  void Reshape();
  void ResetResource() noexcept;
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);

  using CumSumLaunchFunc =
    std::function<bool(CumSumGpuKernelMod *, const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &,
                       const std::vector<KernelTensor *> &)>;
  static std::vector<std::pair<KernelAttr, CumSumLaunchFunc>> func_list_;
  CumSumLaunchFunc kernel_func_;
  int axis_{0};
  bool exclusive_{false};
  bool reverse_{false};
  bool is_null_input_{false};
  size_t stride_{0};
  size_t stride2_{0};
  size_t dims_[kMaxDimsSize] = {};
  std::vector<size_t> shape_{};
  bool is_dynamic_shape_{false};
  cudaStream_t cuda_stream_{nullptr};
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_CUMSUM_GPU_KERNEL_H_
