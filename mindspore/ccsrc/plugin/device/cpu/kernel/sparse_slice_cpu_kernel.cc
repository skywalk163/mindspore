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

#include "plugin/device/cpu/kernel/sparse_slice_cpu_kernel.h"
#include <algorithm>
#include <complex>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "plugin/device/cpu/kernel/eigen/eigen_common_utils.h"

namespace mindspore {
namespace kernel {
namespace {
using complex64 = std::complex<float>;
using complex128 = std::complex<double>;
constexpr int64_t kSparseSliceInputsNum = 5;
constexpr int64_t kSparseSliceOutputsNum = 3;
constexpr int64_t kDim0Num = 1;
constexpr int64_t kDim1Num = 2;

#define ADD_KERNEL(dtype, type)                    \
  {                                                \
    KernelAttr()                                   \
      .AddInputAttr(kNumberTypeInt64)              \
      .AddInputAttr(kNumberType##dtype)            \
      .AddInputAttr(kNumberTypeInt64)              \
      .AddInputAttr(kNumberTypeInt64)              \
      .AddInputAttr(kNumberTypeInt64)              \
      .AddOutputAttr(kNumberTypeInt64)             \
      .AddOutputAttr(kNumberType##dtype)           \
      .AddOutputAttr(kNumberTypeInt64),            \
      &SparseSliceCpuKernelMod::LaunchKernel<type> \
  }
}  // namespace

bool SparseSliceCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }
  return true;
}

int SparseSliceCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  const auto input_indices_shape = inputs[kIndex0]->GetShapeVector();
  const auto input_values_shape = inputs[kIndex1]->GetShapeVector();
  const auto input_shape_shape = inputs[kIndex2]->GetShapeVector();
  const auto input_start_shape = inputs[kIndex3]->GetShapeVector();
  const auto input_size_shape = inputs[kIndex4]->GetShapeVector();

  if (input_indices_shape.size() != kDim1Num) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it requires 'input_indices_shape' must be 2D Tensor "
                      << ", but got " << input_indices_shape.size() << "-D";
  }
  if (input_values_shape.size() != kDim0Num) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it requires 'input_values_shape' must be 1D Tensor "
                      << ", but got " << input_values_shape.size() << "-D";
  }
  if (input_shape_shape.size() != kDim0Num) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it requires 'input_shape_shape' must be 1D Tensor "
                      << ", but got " << input_shape_shape.size() << "-D";
  }
  if (input_start_shape.size() != kDim0Num) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it requires 'input_start_shape' must be 1D Tensor "
                      << ", but got " << input_start_shape.size() << "-D";
  }
  if (input_size_shape.size() != kDim0Num) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it requires 'input_size_shape' must be 1D Tensor "
                      << ", but got " << input_size_shape.size() << "-D";
  }
  if (input_indices_shape[0] != input_values_shape[0]) {
    MS_LOG(ERROR)
      << "For '" << kernel_name_
      << "', the dim of 'input_indices' must be the same as 'input_values', but got the dim of 'input_indices': "
      << input_indices_shape[0] << " and the dim of 'input_values': " << input_values_shape[0];
    return KRET_RESIZE_FAILED;
  }
  if (!IsSameShape(input_shape_shape, input_start_shape)) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the shape of 'input_shape' must be the same as the shape of 'input_start', but got the "
                     "shape of 'input_shape': "
                  << input_shape_shape << " and the shape of 'input_start': " << input_start_shape;
    return KRET_RESIZE_FAILED;
  }
  if (!IsSameShape(input_shape_shape, input_size_shape)) {
    MS_LOG(ERROR) << "For '" << kernel_name_
                  << "', the shape of 'input_shape' must be the same as the shape of 'input_size', but got the shape "
                     "of 'input_shape': "
                  << input_shape_shape << " and the shape of 'input_size': " << input_size_shape;
    return KRET_RESIZE_FAILED;
  }
  nnz_ = input_indices_shape[0];
  rank_ = input_indices_shape[1];

  return KRET_OK;
}

void SparseSliceCpuKernelMod::UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                                       const std::vector<KernelTensor *> &outputs) {
  outputs[kIndex0]->SetShapeVector(ShapeVector({slice_nnz_, rank_}));
  outputs[kIndex1]->SetShapeVector(ShapeVector({slice_nnz_}));
  outputs[kIndex2]->SetShapeVector(ShapeVector({rank_}));
  outputs[kIndex0]->set_size(LongToSize(slice_nnz_ * rank_) * UnitSizeInBytes(outputs[kIndex0]->dtype_id()));
  outputs[kIndex1]->set_size(LongToSize(slice_nnz_) * UnitSizeInBytes(outputs[kIndex1]->dtype_id()));
  outputs[kIndex2]->set_size(LongToSize(rank_) * UnitSizeInBytes(outputs[kIndex2]->dtype_id()));
}
template <typename T>
bool SparseSliceCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                           const std::vector<kernel::KernelTensor *> &workspace,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kSparseSliceInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kSparseSliceOutputsNum, kernel_name_);
  auto input_indices = static_cast<int64_t *>(inputs[kIndex0]->device_ptr());
  auto input_values = static_cast<T *>(inputs[kIndex1]->device_ptr());
  auto input_shape = static_cast<int64_t *>(inputs[kIndex2]->device_ptr());
  auto input_start = static_cast<int64_t *>(inputs[kIndex3]->device_ptr());
  auto input_size = static_cast<int64_t *>(inputs[kIndex4]->device_ptr());
  auto output_indices = static_cast<int64_t *>(outputs[kIndex0]->device_ptr());
  auto output_values = static_cast<T *>(outputs[kIndex1]->device_ptr());
  auto output_shape = static_cast<int64_t *>(outputs[kIndex2]->device_ptr());

  SliceCompute<T>(input_indices, input_values, input_shape, input_start, input_size, output_indices, output_values,
                  output_shape);

  return true;
}

template <typename T>
void SparseSliceCpuKernelMod::SliceCompute(int64_t *input_indices, T *input_values, int64_t *input_shape,
                                           int64_t *start, int64_t *size, int64_t *output_indices, T *output_values,
                                           int64_t *output_shape) {
  for (size_t dim = 0; dim < LongToSize(rank_); dim++) {
    const auto input_size = input_shape[dim];
    const auto start_index = start[dim];
    const auto slice_size = size[dim];

    if (start_index + slice_size < input_size) {
      output_shape[dim] = slice_size;
    } else if (start_index < input_size) {
      output_shape[dim] = input_size - start_index;
    } else {
      output_shape[dim] = 0;
    }
  }

  ShapeVector indices_shape = ShapeVector({nnz_, rank_});
  ShapeVector values_shape = ShapeVector({nnz_});
  const auto input_indices_t = (EigenTensor(indices_shape, input_indices)).matrix<int64_t>();
  const auto input_values_t = (EigenTensor(values_shape, input_values)).vec<T>();
  auto output_indices_t = (EigenTensor(indices_shape, output_indices)).matrix<int64_t>();
  auto output_values_t = (EigenTensor(values_shape, output_values)).vec<T>();

  int64_t count = 0;
  for (size_t i = 0; i < LongToSize(nnz_); i++) {
    bool hit = true;
    for (size_t dim = 0; dim < LongToSize(rank_); dim++) {
      if (!(start[dim] <= input_indices_t(i, dim) && input_indices_t(i, dim) < start[dim] + size[dim])) {
        hit = false;
        break;
      }
    }
    if (!hit) {
      continue;
    }
    output_values_t(count) = static_cast<T>(input_values_t(i));
    for (size_t dim = 0; dim < LongToSize(rank_); dim++) {
      output_indices_t(count, dim) = input_indices_t(i, dim) - start[dim];
    }
    count++;
  }
  slice_nnz_ = count;
}

const std::vector<std::pair<KernelAttr, SparseSliceCpuKernelMod::KernelRunFunc>> &SparseSliceCpuKernelMod::GetFuncList()
  const {
  static const std::vector<std::pair<KernelAttr, SparseSliceCpuKernelMod::KernelRunFunc>> func_list = {
    ADD_KERNEL(Bool, bool),           ADD_KERNEL(UInt8, uint8_t),         ADD_KERNEL(UInt16, uint16_t),
    ADD_KERNEL(Int8, int8_t),         ADD_KERNEL(Int16, int16_t),         ADD_KERNEL(Int32, int),
    ADD_KERNEL(UInt32, uint32_t),     ADD_KERNEL(UInt64, uint64_t),       ADD_KERNEL(Int64, int64_t),
    ADD_KERNEL(Float16, float16),     ADD_KERNEL(Float32, float),         ADD_KERNEL(Float64, double),
    ADD_KERNEL(Complex64, complex64), ADD_KERNEL(Complex128, complex128),
  };
  return func_list;
}  // namespace kernel
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, SparseSlice, SparseSliceCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
