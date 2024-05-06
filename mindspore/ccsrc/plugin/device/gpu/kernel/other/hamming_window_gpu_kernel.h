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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_OTHER_Hamming_WINDOW_GPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_OTHER_Hamming_WINDOW_GPU_KERNEL_H_
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <map>
#include "mindspore/core/ops/hamming_window.h"
#include "abstract/utils.h"
#include "plugin/factory/ms_factory.h"
#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/hamming_window_impl.cuh"

namespace mindspore {
namespace kernel {
class HammingWindowGpuKernelMod : public NativeGpuKernelMod {
 public:
  HammingWindowGpuKernelMod() { ResetResource(); }
  ~HammingWindowGpuKernelMod() override = default;

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
    output_elements_ = 0;
    is_null_input_ = false;
    output_size_list_.clear();
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T, typename S>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs);
  using Hamming_Func =
    std::function<bool(HammingWindowGpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                       const std::vector<kernel::KernelTensor *> &, const std::vector<kernel::KernelTensor *> &)>;

 private:
  bool periodic_{true};
  float alpha_{0.54};
  float beta_{0.46};
  size_t unit_input_size_{1};
  size_t unit_output_size_{1};
  size_t output_elements_;
  Hamming_Func kernel_func_{};
  bool is_null_input_{false};
  void *cuda_stream_{nullptr};
  static std::vector<std::pair<KernelAttr, Hamming_Func>> func_list_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_GPU_OTHER_Hamming_GPU_KERNEL_H_
