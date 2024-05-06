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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_BIAS_DROPOUT_ADD_KERNEL_H_
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_BIAS_DROPOUT_ADD_KERNEL_H_

#include <string>
#include <vector>
#include <map>
#include <utility>

#include "plugin/device/gpu/kernel/gpu_kernel.h"
#include "plugin/device/gpu/kernel/gpu_kernel_factory.h"
#include "plugin/device/gpu/kernel/kernel_constants.h"

namespace mindspore {
namespace kernel {
class BiasDropoutAddGpuKernelMod : public NativeGpuKernelMod {
 public:
  BiasDropoutAddGpuKernelMod() = default;
  ~BiasDropoutAddGpuKernelMod() override = default;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
              const std::vector<KernelTensor *> &outputs, void *stream_ptr) override {
    cuda_stream_ = reinterpret_cast<cudaStream_t>(stream_ptr);
    return kernel_func_(this, inputs, outputs);
  }

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
  using KernelFunc = std::function<bool(BiasDropoutAddGpuKernelMod *, const std::vector<KernelTensor *> &,
                                        const std::vector<KernelTensor *> &)>;
  cudaStream_t cuda_stream_{nullptr};
  KernelFunc kernel_func_{};
  static std::vector<std::pair<KernelAttr, KernelFunc>> func_list_;

  size_t num_count_{1};
  size_t n_strides_{1};
  size_t channel_strides_{1};
  float keep_prob_{1.0f};
  uint64_t seed_{0};
  uint64_t seed_offset_{0};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_GPU_KERNEL_NN_BIAS_DROPOUT_ADD_KERNEL_H_
