/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/eigen/cholesky_solve_cpu_kernel.h"
#include <Eigen/Dense>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/cholesky_solve.h"

namespace mindspore {
namespace kernel {
constexpr size_t kInputIndex0 = 0;
constexpr size_t kInputIndex1 = 1;
constexpr size_t kOutputIndex = 0;
constexpr size_t kDefalutRank = 2;
constexpr size_t kBatchRank = 3;
constexpr size_t kBatchIndex = 3;
constexpr size_t kRowIndex = 2;
constexpr size_t kColIndex = 1;
constexpr size_t kCholeskySolveInputNum = 2;
constexpr size_t kCholeskySolveOutputNum = 1;

bool CholeskySolveCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kCholeskySolveInputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kCholeskySolveOutputNum, kernel_name_);
  upper = GetValue<bool>(primitive_->GetAttr(ops::kUpper));
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int CholeskySolveCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  auto shape = inputs[kIndex0]->GetShapeVector();
  auto shape2 = inputs[kIndex1]->GetShapeVector();
  dtype_ = inputs[kIndex0]->dtype_id();
  std::vector<size_t> x1_shape = Convert2SizeT(shape);
  std::vector<size_t> x2_shape = Convert2SizeT(shape2);
  if (x1_shape.size() != kDefalutRank && x1_shape.size() != kBatchRank) {
    MS_EXCEPTION(ValueError) << "For CholeskySolve, the rank of x1 must be 2 or 3, but got rank " << x1_shape.size();
  }
  if (x1_shape.size() != x2_shape.size()) {
    MS_EXCEPTION(ValueError) << "For CholeskySolve, ranks of inputs should be equal"
                             << ", while got x1 rank " << x1_shape.size() << ", x2 rank " << x2_shape.size() << ".";
  }
  size_t rank = x1_shape.size();
  if (rank == kDefalutRank) {
    dim = x1_shape[rank - kRowIndex];
    rhs_dim = x1_shape[rank - kColIndex];
  } else {
    batch_size = x1_shape[rank - kBatchIndex];
    dim = x1_shape[rank - kRowIndex];
    rhs_dim = x1_shape[rank - kColIndex];
  }
  return KRET_OK;
}

template <typename T>
bool CholeskySolveCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                             const std::vector<KernelTensor *> &,
                                             const std::vector<KernelTensor *> &outputs) {
  T *rhsptr = reinterpret_cast<T *>(inputs[kInputIndex0]->device_ptr());
  T *lhsptr = reinterpret_cast<T *>(inputs[kInputIndex1]->device_ptr());
  T *outptr = reinterpret_cast<T *>(outputs[kOutputIndex]->device_ptr());
  Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> RHS(dim, rhs_dim);
  Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> LHS(dim, dim);
  for (size_t k = 0; k < batch_size; k++) {
    for (size_t i = 0; i < dim * rhs_dim; i++) {
      RHS.data()[i] = rhsptr[k * dim * rhs_dim + i];
    }
    for (size_t i = 0; i < dim * dim; i++) {
      LHS.data()[i] = lhsptr[k * dim * dim + i];
    }
    if (!upper) {
      LHS.template triangularView<Eigen::Lower>().solveInPlace(RHS);
      LHS.adjoint().template triangularView<Eigen::Upper>().solveInPlace(RHS);
    } else {
      LHS.adjoint().template triangularView<Eigen::Lower>().solveInPlace(RHS);
      LHS.template triangularView<Eigen::Upper>().solveInPlace(RHS);
    }
    for (size_t i = 0; i < dim * rhs_dim; i++) {
      outptr[k * dim * rhs_dim + i] = RHS.data()[i];
    }
  }
  return true;
}

const std::vector<std::pair<KernelAttr, CholeskySolveCpuKernelMod::KernelRunFunc>>
  &CholeskySolveCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, CholeskySolveCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
     &CholeskySolveCpuKernelMod::LaunchKernel<float>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
     &CholeskySolveCpuKernelMod::LaunchKernel<double>},
  };
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, CholeskySolve, CholeskySolveCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
