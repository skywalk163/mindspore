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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EIGEN_SOLVE_TRIANGULAR_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EIGEN_SOLVE_TRIANGULAR_CPU_KERNEL_H_

#include <Eigen/Dense>
#include <vector>
#include <utility>
#include <map>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
class SolveTriangularCpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<SolveTriangularCpuKernelMod> {
 public:
  SolveTriangularCpuKernelMod() = default;
  ~SolveTriangularCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

 private:
  template <typename T_in, typename T_out>
  bool LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                    const std::vector<KernelTensor *> &outputs);

  template <typename Derived_a, typename Derived_b, typename T>
  void solve(const Eigen::MatrixBase<Derived_a> &a, const Eigen::MatrixBase<Derived_b> &b, T *output_addr, bool lower);

  void SolveTriangularCheck(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs);

  size_t m_{0};
  size_t n_{0};
  size_t batch_{1};
  bool lower_{false};
  bool trans_{false};
  bool conj_{false};
  bool unit_diagonal_{false};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_EIGEN_SOLVE_TRIANGULAR_CPU_KERNEL_H_
