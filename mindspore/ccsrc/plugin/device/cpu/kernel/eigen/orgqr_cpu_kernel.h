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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_ORGQR_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_ORGQR_KERNEL_H_

#include <complex>
#include <iostream>
#include <map>
#include <utility>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
using complex64 = std::complex<float>;
using complex128 = std::complex<double>;

class OrgqrCpuKernelMod : public NativeCpuKernelMod {
 public:
  OrgqrCpuKernelMod() = default;
  ~OrgqrCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, outputs);
  };

 protected:
  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  template <typename T>
  bool LaunchComplexKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);
  using OrgqrLaunchFunc = std::function<bool(OrgqrCpuKernelMod *, const std::vector<kernel::KernelTensor *> &,
                                             const std::vector<kernel::KernelTensor *> &)>;

  static std::vector<std::pair<KernelAttr, OrgqrLaunchFunc>> func_list_;
  OrgqrLaunchFunc kernel_func_;
  int64_t m_{1};
  int64_t n_{1};
  int64_t p_{1};
  int64_t martrix_num_{1};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_ORGQR_KERNEL_H_
