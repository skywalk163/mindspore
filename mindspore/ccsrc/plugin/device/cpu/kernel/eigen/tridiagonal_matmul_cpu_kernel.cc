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

#include "plugin/device/cpu/kernel/eigen/tridiagonal_matmul_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "Eigen/Core"
#include "Eigen/LU"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kInputSize = 4;
constexpr size_t kOutputSize = 1;
constexpr size_t row = 2;
constexpr size_t is_matrix = 2;
constexpr size_t col = 1;
constexpr int64_t is_vector = 1;
constexpr size_t kInputShapeIndex0 = 0;
constexpr size_t kInputShapeIndex1 = 1;
constexpr size_t kInputShapeIndex2 = 2;
constexpr size_t kInputShapeIndex3 = 3;
}  // namespace
bool TridiagonalMatMulCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &outputs) {
  MS_EXCEPTION_IF_NULL(inputs[kIndex0]);
  dtype_ = inputs[kIndex0]->dtype_id();
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  bool is_match = MatchKernelAttr(kernel_attr, GetOpSupport()).first;
  if (!is_match) {
    MS_LOG(ERROR) << kernel_name_ << " does not support this kernel data type: " << kernel_attr;
    return false;
  }
  return true;
}

int TridiagonalMatMulCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                          const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }

  MS_EXCEPTION_IF_NULL(inputs[kIndex0]);
  MS_EXCEPTION_IF_NULL(inputs[kIndex1]);
  MS_EXCEPTION_IF_NULL(inputs[kIndex2]);
  MS_EXCEPTION_IF_NULL(inputs[kIndex3]);
  auto input0_shape = inputs[kIndex0]->GetShapeVector();
  auto input1_shape = inputs[kIndex1]->GetShapeVector();
  auto input2_shape = inputs[kIndex2]->GetShapeVector();
  auto input3_shape = inputs[kIndex3]->GetShapeVector();
  rhs_shape_ = input3_shape;
  if ((input0_shape.size() < is_matrix) || (input1_shape.size() < is_matrix) || (input2_shape.size() < is_matrix) ||
      (input3_shape.size() < is_matrix)) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the rank of all inputs must be equal to or greater than 2, "
                     "but got the rank of 'superdiag': "
                  << input0_shape.size() << ", the rank of 'maindiag': " << input1_shape.size()
                  << ", the rank of 'subdiag': " << input2_shape.size()
                  << ", the rank of 'rhs': " << input3_shape.size();
    return KRET_RESIZE_FAILED;
  }
  if ((input0_shape[input0_shape.size() - row] != is_vector) ||
      (input1_shape[input1_shape.size() - row] != is_vector) ||
      (input2_shape[input2_shape.size() - row] != is_vector)) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the row of superdiag, maindiag and subdiag must be 1, "
                     "but got the row of 'superdiag': "
                  << input0_shape[input0_shape.size() - row]
                  << ", the row of 'maindiag': " << input1_shape[input1_shape.size() - row]
                  << ", the row of 'subdiag': " << input2_shape[input2_shape.size() - row];
    return KRET_RESIZE_FAILED;
  }
  if ((input0_shape != input1_shape) || (input0_shape != input2_shape) || (input1_shape != input2_shape)) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the shape of superdiag, maindiag and subdiag must be same, "
                     "but got the shape of 'superdiag': "
                  << input0_shape << ", the shape of 'maindiag': " << input1_shape
                  << ", the shape of 'subdiag': " << input2_shape;
    return KRET_RESIZE_FAILED;
  }
  if ((input0_shape[input0_shape.size() - col] != input3_shape[input3_shape.size() - row]) ||
      (input1_shape[input1_shape.size() - col] != input3_shape[input3_shape.size() - row]) ||
      (input2_shape[input2_shape.size() - col] != input3_shape[input3_shape.size() - row])) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the col of superdiag, maindiag and subdiag must be equal to the row of rhs, "
                     "but got the col of 'superdiag': "
                  << input0_shape[input0_shape.size() - col]
                  << ", the col of 'maindiag': " << input1_shape[input1_shape.size() - col]
                  << ", the col of 'subdiag': " << input2_shape[input2_shape.size() - col]
                  << ", the row of 'rhs': " << input3_shape[input2_shape.size() - row];
    return KRET_RESIZE_FAILED;
  }
  size_t rhs_shape_num = input3_shape.size() - row;
  for (size_t i = 0; i < rhs_shape_num; i++) {
    if ((input0_shape[i] != input3_shape[i]) || (input1_shape[i] != input3_shape[i]) ||
        (input2_shape[i] != input3_shape[i])) {
      MS_LOG(ERROR) << "For '" << kernel_name_
                    << "', the shape of all inputs ignoring the last two elements must be same, "
                       "but got the shape of 'superdiag': "
                    << input0_shape << ", the shape of 'maindiag': " << input1_shape
                    << ", the shape of 'subdiag': " << input2_shape << ", the shape of 'rhs': " << input3_shape;
      return KRET_RESIZE_FAILED;
    }
  }
  return KRET_OK;
}

bool TridiagonalMatMulCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                           const std::vector<kernel::KernelTensor *> & /* workspace */,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputSize, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputSize, kernel_name_);
  if (dtype_ == kNumberTypeFloat16) {
    LaunchTridiagonalMatMul<Eigen::half>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat32) {
    LaunchTridiagonalMatMul<float>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat64) {
    LaunchTridiagonalMatMul<double>(inputs, outputs);
  } else if (dtype_ == kNumberTypeComplex64) {
    LaunchTridiagonalMatMul<std::complex<float>>(inputs, outputs);
  } else if (dtype_ == kNumberTypeComplex128) {
    LaunchTridiagonalMatMul<std::complex<double>>(inputs, outputs);
  } else {
    MS_LOG(EXCEPTION) << "TridiagonalMatMul kernel data type " << TypeIdLabel(dtype_) << " not support.";
  }
  return true;
}

template <typename T>
void TridiagonalMatMulCpuKernelMod::LaunchTridiagonalMatMul(const std::vector<KernelTensor *> &inputs,
                                                            const std::vector<KernelTensor *> &outputs) {
  T *superdiag_ptr = reinterpret_cast<T *>(inputs[0]->device_ptr());
  MS_EXCEPTION_IF_NULL(superdiag_ptr);
  T *maindiag_ptr = reinterpret_cast<T *>(inputs[1]->device_ptr());
  MS_EXCEPTION_IF_NULL(maindiag_ptr);
  T *subdiag_ptr = reinterpret_cast<T *>(inputs[2]->device_ptr());
  MS_EXCEPTION_IF_NULL(subdiag_ptr);
  T *rhs_ptr = reinterpret_cast<T *>(inputs[3]->device_ptr());
  MS_EXCEPTION_IF_NULL(rhs_ptr);
  T *y_ptr = reinterpret_cast<T *>(outputs[0]->device_ptr());
  MS_EXCEPTION_IF_NULL(y_ptr);
  size_t m = static_cast<size_t>(rhs_shape_[rhs_shape_.size() - row]);
  size_t n = static_cast<size_t>(rhs_shape_[rhs_shape_.size() - col]);
  size_t size_mn = m * n;
  using VectorMap = Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, 1>>;
  using MatrixMap = Eigen::Map<Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>;
  size_t rhs_num = 1;
  for (size_t i = 0; i < rhs_shape_.size(); i++) {
    rhs_num *= static_cast<size_t>(rhs_shape_[i]);
  }
  size_t rhs_matrix_num = rhs_num / size_mn;
  for (size_t i = 0; i < rhs_matrix_num; i++) {
    VectorMap superdiag(superdiag_ptr + i * m, m, 1);
    VectorMap maindiag(maindiag_ptr + i * m, m, 1);
    VectorMap subdiag(subdiag_ptr + i * m, m, 1);
    MatrixMap rhs(rhs_ptr + i * m * n, m, n);
    MatrixMap y(y_ptr + i * m * n, m, n);
    y.array() = rhs.array().colwise() * maindiag.array();
    for (size_t j = 0; j < m - 1; j++) {
      y.array().row(j) += rhs.array().row(j + 1) * superdiag(j);
      y.array().row(j + 1) += rhs.array().row(j) * subdiag(j + 1);
    }
  }
}

std::vector<KernelAttr> TridiagonalMatMulCpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list = {KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddOutputAttr(kNumberTypeFloat16),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeFloat32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat64)
                                                   .AddInputAttr(kNumberTypeFloat64)
                                                   .AddInputAttr(kNumberTypeFloat64)
                                                   .AddInputAttr(kNumberTypeFloat64)
                                                   .AddOutputAttr(kNumberTypeFloat64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeComplex64)
                                                   .AddInputAttr(kNumberTypeComplex64)
                                                   .AddInputAttr(kNumberTypeComplex64)
                                                   .AddInputAttr(kNumberTypeComplex64)
                                                   .AddOutputAttr(kNumberTypeComplex64),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeComplex128)
                                                   .AddInputAttr(kNumberTypeComplex128)
                                                   .AddInputAttr(kNumberTypeComplex128)
                                                   .AddInputAttr(kNumberTypeComplex128)
                                                   .AddOutputAttr(kNumberTypeComplex128)};

  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, TridiagonalMatMul, TridiagonalMatMulCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
