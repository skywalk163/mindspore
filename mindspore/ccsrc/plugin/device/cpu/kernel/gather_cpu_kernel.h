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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_GATHER_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_GATHER_CPU_KERNEL_H_

#include <utility>
#include <vector>
#include <map>
#include <memory>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "plugin/device/cpu/kernel/nnacl/base/gather_base.h"

namespace mindspore {
namespace kernel {
class GatherCpuKernelMod : public NativeCpuKernelMod {
 public:
  GatherCpuKernelMod() = default;
  ~GatherCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, outputs);
  }

  std::vector<KernelAttr> GetOpSupport() override;

  void ResetResource() noexcept {
    input_shape_.clear();
    indices_shape_.clear();
    output_shape_.clear();
    is_null_input_ = false;
    output_size_list_.clear();
    workspace_size_list_.clear();
  }

  void InitSizeLists() {
    auto output_size =
      std::accumulate(output_shape_.begin(), output_shape_.end(), static_cast<size_t>(1), std::multiplies{});
    output_size_list_.push_back(output_size * input_type_size_);
  }

 private:
  template <typename T, typename S>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &outputs);
  using GatherFunc = std::function<bool(GatherCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                                        const std::vector<kernel::KernelTensor *> &)>;
  static std::vector<std::pair<KernelAttr, GatherFunc>> func_list_;
  GatherFunc kernel_func_;

  ShapeVector input_shape_;
  ShapeVector indices_shape_;
  ShapeVector output_shape_;
  int64_t batch_dims_{0};
  size_t input_type_size_ = 0;
  size_t indices_type_size_ = 0;
  bool is_null_input_ = false;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_GATHER_CPU_KERNEL_H_
