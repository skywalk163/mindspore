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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_FILL_DIAGONAL_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_FILL_DIAGONAL_GPU_KERNEL_H_

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "abstract/utils.h"
#include "mindspore/core/ops/fill_diagonal.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/fill_diagonal_impl.cuh"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class FillDiagonalGpuKernelMod : public NativeGpuKernelMod {
 public:
  FillDiagonalGpuKernelMod() { ResetResource(); }
  ~FillDiagonalGpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *cuda_stream) override {
    if (is_null_input_) {
      return true;
    }
    cuda_stream_ = cuda_stream;
    return kernel_func_(this, inputs, workspace, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

 protected:
  void ResetResource() noexcept {
    is_null_input_ = false;
    input_elements_ = 0;
    step_ = 0;
    output_size_list_.clear();
    workspace_size_list_.clear();
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);
  using FillDiagonalFunc =
    std::function<bool(FillDiagonalGpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;

 private:
  float fill_value_{0.0};
  bool wrap_{false};
  size_t num_diagonal_elements_{0};
  int64_t step_{0};
  int64_t input_dims_{0};
  int64_t matrix_row_{0};
  int64_t matrix_col_{0};
  size_t unit_size_{1};
  size_t input_elements_{0};
  FillDiagonalFunc kernel_func_{};
  bool is_null_input_{false};
  void *cuda_stream_{nullptr};
  static std::vector<std::pair<KernelAttr, FillDiagonalFunc>> func_list_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_MATH_FILL_DIAGONAL_GPU_KERNEL_H_
