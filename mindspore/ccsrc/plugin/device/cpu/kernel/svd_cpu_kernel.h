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

#ifndef MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_SVD_CPU_KERNEL_H
#define MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_SVD_CPU_KERNEL_H

#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <map>
#include <utility>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class SvdCpuKernelMod : public NativeCpuKernelMod {
 public:
  SvdCpuKernelMod() {}
  ~SvdCpuKernelMod() = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernelFloat(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                         const std::vector<KernelTensor *> &outputs);

  template <typename T>
  bool LaunchKernelComplex(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
                           const std::vector<KernelTensor *> &outputs);

  using SvdFunc = std::function<bool(SvdCpuKernelMod *, const std::vector<KernelTensor *> &,
                                     const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &)>;

 private:
  static std::vector<std::pair<KernelAttr, SvdFunc>> func_list_;
  SvdFunc kernel_func_;
  bool full_matrices_{false};
  bool compute_uv_{true};
  int64_t batch_size_ = 1;
  int64_t num_of_rows_;
  int64_t num_of_cols_;
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_PLUGIN_DEVICE_CPU_KERNEL_SVD_CPU_KERNEL_H
