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
#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_ARRAYS_REVERSE_V2_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_ARRAYS_REVERSE_V2_GPU_KERNEL_H_

#include <algorithm>
#include <cstdint>
#include <vector>
#include <map>
#include <functional>
#include <string>
#include <utility>

#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/reverse_v2_impl.cuh"

namespace mindspore {
namespace kernel {
class ReverseV2GpuKernelMod : public NativeGpuKernelMod {
 public:
  ReverseV2GpuKernelMod() = default;
  ~ReverseV2GpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    return kernel_func_(this, inputs, workspace, outputs, stream_ptr);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override;
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs, void *stream_ptr);

  using ReverseV2LaunchFunc = std::function<bool(ReverseV2GpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                                                 const std::vector<kernel::KernelTensor *> &,
                                                 const std::vector<kernel::KernelTensor *> &, void *)>;

 private:
  ReverseV2LaunchFunc kernel_func_;
  static std::vector<std::pair<KernelAttr, ReverseV2LaunchFunc>> func_list_;

  size_t input_size_;
  size_t input_rank_;
  std::vector<int64_t> input_shape_;
  std::vector<int64_t> strides_;
  std::vector<int64_t> axis_;
  bool is_null_input_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_ARRAYS_REVERSE_V2_GPU_KERNEL_H_
