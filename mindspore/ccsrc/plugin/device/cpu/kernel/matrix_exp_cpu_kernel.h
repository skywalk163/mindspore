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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MATRIX_EXP_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MATRIX_EXP_CPU_KERNEL_H_

#include <algorithm>
#include <complex>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/device/cpu/kernel/eigen/eigen_common_utils.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
template <typename T>
using MatrixXd = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

class MatrixExpCpuKernelMod : public NativeCpuKernelMod, public MatchKernelHelper<MatrixExpCpuKernelMod> {
 public:
  MatrixExpCpuKernelMod() = default;
  ~MatrixExpCpuKernelMod() override = default;

  bool Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  int Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) override;

  bool Launch(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &workspace,
              const std::vector<KernelTensor *> &outputs) override {
    return kernel_func_(this, inputs, workspace, outputs);
  }

  const std::vector<std::pair<KernelAttr, KernelRunFunc>> &GetFuncList() const override;

 protected:
  std::vector<KernelAttr> GetOpSupport() override { return OpSupport(); }

 private:
  template <typename Derived>
  void MTaylorApproximantLow(const Eigen::MatrixBase<Derived> &A, const Eigen::MatrixBase<Derived> &I, int order,
                             Eigen::MatrixBase<Derived> *matrix_y) const;
  template <typename Derived, typename Derived1>
  void MTaylorApproximantHigh(const Eigen::MatrixBase<Derived1> &A_scaled, const Eigen::MatrixBase<Derived> &I,
                              Eigen::MatrixBase<Derived> *matrix_y) const;
  template <typename Derived>
  void MexpImpl(const Eigen::MatrixBase<Derived> &A, const Eigen::MatrixBase<Derived> &I,
                Eigen::MatrixBase<Derived> *matrix_y) const;

  template <typename T>
  bool LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                    const std::vector<kernel::KernelTensor *> &workspace,
                    const std::vector<kernel::KernelTensor *> &outputs);

  std::vector<size_t> input_shape_;
  TypeId data_type_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_MATRIX_EXP_CPU_KERNEL_H_
