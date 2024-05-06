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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_IM2COL_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_IM2COL_GPU_KERNEL_H_

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/im2col_impl.cuh"
#include "plugin/device/gpu/kernel/gpu_kernel.h"

namespace mindspore {
namespace kernel {
class Im2ColGpuKernelMod : public NativeGpuKernelMod {
 public:
  Im2ColGpuKernelMod() = default;
  ~Im2ColGpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    return kernel_func_(this, inputs, outputs, stream_ptr);
  }

  int maxBlockSize{-1};

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &outputs, void *stream_ptr);
  using Im2ColFunc = std::function<bool(Im2ColGpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                                        const std::vector<kernel::KernelTensor *> &, void *)>;
  static std::vector<std::pair<KernelAttr, Im2ColFunc>> func_list_;
  Im2ColFunc kernel_func_;

  std::vector<int64_t> x_shape_;
  std::vector<int64_t> y_shape_;
  std::vector<int64_t> ksizes_;
  std::vector<int64_t> strides_ = {1};
  std::vector<int64_t> dilations_ = {1};
  std::vector<int64_t> pads_ = {0};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_IM2COL_GPU_KERNEL_H_
