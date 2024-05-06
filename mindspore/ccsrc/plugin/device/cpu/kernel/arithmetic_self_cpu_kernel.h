/**
 * Copyright 2020-2023 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_ARITHMETIC_SELF_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_ARITHMETIC_SELF_CPU_KERNEL_H_

#include <complex>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
constexpr auto kUnknown = "Unknown";
using complex64 = std::complex<float>;
using complex128 = std::complex<double>;

class ArithmeticSelfCpuKernelMod : public NativeCpuKernelMod {
 public:
  ArithmeticSelfCpuKernelMod() = default;
  explicit ArithmeticSelfCpuKernelMod(const std::string &kernel_name) : kernel_name_(kernel_name) {}
  ~ArithmeticSelfCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    if (is_null_input_) {
      return true;
    }
    return func_obj_->RunFunc(inputs, workspace, outputs);
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  std::shared_ptr<CpuKernelFunc> func_obj_;
  std::string kernel_name_{kUnknown};
  bool is_null_input_{false};
};

using LaunchFunc = std::function<bool(const std::vector<KernelTensor *> &, const std::vector<KernelTensor *> &)>;
class IdentityCpuKernelMod : public NativeCpuKernelMod {
 public:
  IdentityCpuKernelMod() = default;
  ~IdentityCpuKernelMod() override = default;
  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;
  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    if (is_null_input_) {
      return true;
    }
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), 1, kernel_name_);
    CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), 1, kernel_name_);
    return kernel_func_(inputs, outputs);
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  LaunchFunc kernel_func_;
  bool is_null_input_{false};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_ARITHMETIC_SELF_CPU_KERNEL_H_
