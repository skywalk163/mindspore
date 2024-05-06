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

#include "plugin/device/cpu/kernel/eigen/eig_cpu_kernel.h"
#include <algorithm>
#include <type_traits>
#include <utility>
#include "plugin/device/cpu/kernel/eigen/eigen_common_utils.h"
#include "utils/ms_utils.h"
#include "Eigen/Eigenvalues"

namespace mindspore {
namespace kernel {
namespace {
#define EIG_KERNEL_CPU_REGISTER(IN_DT, OUT_DT, IN_T, OUT_T) \
  KernelAttr()                                              \
    .AddInputAttr(IN_DT)                                    \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)       \
    .AddOutputAttr(OUT_DT)                                  \
    .AddOutputAttr(OUT_DT),                                 \
    &EigCpuKernelMod::LaunchKernel<IN_T, OUT_T>
}  // namespace

void EigCpuKernelMod::InitMatrixInfo(const std::vector<size_t> &shape) {
  if (shape.size() < kShape2dDims) {
    MS_LOG_EXCEPTION << "For '" << kernel_name_ << "', the rank of parameter 'a' must be at least 2, but got "
                     << shape.size() << " dimensions.";
  }
  row_size_ = shape[shape.size() - kDim1];
  col_size_ = shape[shape.size() - kDim2];
  if (row_size_ != col_size_) {
    MS_LOG_EXCEPTION << "For '" << kernel_name_
                     << "', the shape of parameter 'a' must be a square matrix, but got last two dimensions is "
                     << row_size_ << " and " << col_size_;
  }
  batch_size_ = 1;
  for (auto i : shape) {
    batch_size_ *= i;
  }
  batch_size_ /= (row_size_ * col_size_);
}

bool EigCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  compute_v_ = inputs[kIndex1]->GetValueWithCheck<bool>();

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(EXCEPTION) << "Eig does not support this kernel data type: " << kernel_attr;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int EigCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }

  auto input_shape = Convert2SizeTClipNeg(inputs[0]->GetShapeVector());
  InitMatrixInfo(input_shape);
  return KRET_OK;
}

template <typename T, typename C>
bool EigCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                   const std::vector<KernelTensor *> &outputs) {
  auto input_addr = reinterpret_cast<T *>(inputs[0]->device_ptr());
  auto output_w_addr = reinterpret_cast<C *>(outputs[0]->device_ptr());
  auto output_v_addr = compute_v_ ? reinterpret_cast<C *>(outputs[1]->device_ptr()) : nullptr;

  for (size_t batch = 0; batch < batch_size_; ++batch) {
    T *a_addr = input_addr + batch * row_size_ * col_size_;
    C *w_addr = output_w_addr + batch * row_size_;
    Map<MatrixSquare<T>> a(a_addr, row_size_, col_size_);
    Map<MatrixSquare<C>> w(w_addr, row_size_, 1);
    auto eigen_option = compute_v_ ? Eigen::ComputeEigenvectors : Eigen::EigenvaluesOnly;
    Eigen::ComplexEigenSolver<MatrixSquare<T>> solver(a, eigen_option);
    w = solver.eigenvalues();
    if (compute_v_) {
      C *v_addr = output_v_addr + batch * row_size_ * col_size_;
      Map<MatrixSquare<C>> v(v_addr, row_size_, col_size_);
      v = solver.eigenvectors();
    }
    if (solver.info() != Eigen::Success) {
      MS_LOG_WARNING << "For '" << kernel_name_
                     << "', the computation was not successful. Eigen::ComplexEigenSolver returns 'NoConvergence'.";
    }
  }
  return true;
}

std::vector<std::pair<KernelAttr, EigCpuKernelMod::EigFunc>> EigCpuKernelMod::func_list_ = {
  {EIG_KERNEL_CPU_REGISTER(kNumberTypeFloat32, kNumberTypeComplex64, float, float_complex)},
  {EIG_KERNEL_CPU_REGISTER(kNumberTypeFloat64, kNumberTypeComplex128, double, double_complex)},
  {EIG_KERNEL_CPU_REGISTER(kNumberTypeComplex64, kNumberTypeComplex64, float_complex, float_complex)},
  {EIG_KERNEL_CPU_REGISTER(kNumberTypeComplex128, kNumberTypeComplex128, double_complex, double_complex)}};

std::vector<KernelAttr> EigCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, EigFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Eig, EigCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
