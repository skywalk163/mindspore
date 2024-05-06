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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_DROPOUT_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_DROPOUT_KERNEL_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/cuda_impl/cuda_ops/dropout_impl.cuh"

namespace mindspore {
namespace kernel {
class DropoutFwdGpuKernelMod : public NativeGpuKernelMod {
 public:
  DropoutFwdGpuKernelMod() = default;
  ~DropoutFwdGpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    MS_EXCEPTION_IF_NULL(kernel_func_);
    return kernel_func_(this, inputs, workspace, outputs, stream_ptr);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  void ResetResource() noexcept;

 protected:
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
  void InitSizeLists();
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                    const std::vector<KernelTensor *> &outputs, void *stream_ptr);
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  using DropoutFunc =
    std::function<bool(DropoutFwdGpuKernelMod *, const std::vector<KernelTensor *> &,
                       const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &, void *)>;
  static std::vector<std::pair<KernelAttr, DropoutFunc>> func_list_;
  DropoutFunc kernel_func_;
  bool is_null_input_{false};
  size_t num_count_;
  float keep_prob_;
  bool states_init_{false};
  uint64_t seed_{0};
  uint64_t seed_offset_{0};
  size_t input_size_{0};
  size_t output_size_{0};
  std::vector<int64_t> input_shape_;
  bool use_fused_dropout_{false};
  bool only_use_first_output_{false};
  bool only_use_second_output_{false};
  curandGenerator_t mask_generator_{nullptr};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_DROPOUT_KERNEL_H_
