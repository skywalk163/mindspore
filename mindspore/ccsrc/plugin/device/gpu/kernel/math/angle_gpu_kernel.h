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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_MATH_ANGLE_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_MATH_ANGLE_GPU_KERNEL_H_

#include <memory>
#include <vector>
#include <utility>
#include <algorithm>
#include <functional>
#include <string>
#include <map>
#include "ops/ops_func_impl/complex.h"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/angle_impl.cuh"
#include "plugin/device/gpu/kernel/kernel_constants.h"

namespace mindspore {
namespace kernel {
constexpr auto kUnknown = "Unknown";
class AngleGpuKernelMod : public NativeGpuKernelMod {
 public:
  AngleGpuKernelMod() = default;
  ~AngleGpuKernelMod() override = default;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    return kernel_func_(this, inputs, workspace, outputs, stream_ptr);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  void ResetResource() noexcept;

  template <typename T, typename S>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs, void *stream_ptr);

  using AngleFunc =
    std::function<bool(AngleGpuKernelMod *, const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &,
                       const std::vector<KernelTensor *> &, void *)>;

 private:
  bool is_null_input_{false};
  std::string kernel_name_{kUnknown};
  TypeId input_dtype_ = kNumberTypeComplex64;
  size_t output_size;
  AngleFunc kernel_func_;
  static std::vector<std::pair<KernelAttr, AngleFunc>> func_list_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_MATH_ANGLE_GPU_KERNEL_H_
