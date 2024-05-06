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
#include "plugin/device/cpu/kernel/sparse_add_cpu_kernel.h"
#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <complex>
#include <functional>
#include "include/common/thread_pool.h"
#include "mindspore/core/ops/sparse_add.h"

namespace mindspore {
namespace kernel {
// Value check constant
constexpr size_t kInputNum = 7;
constexpr size_t kOutputNum = 3;
// Input idx constant
constexpr size_t kAIndicesIdx = 0;
constexpr size_t kAValuesIdx = 1;
constexpr size_t kAShapeIdx = 2;
constexpr size_t kBIndicesIdx = 3;
constexpr size_t kBValuesIdx = 4;
constexpr size_t kBShapeIdx = 5;
constexpr size_t kThreshIdx = 6;
// Output idx constant
constexpr size_t kSumIndicesIdx = 0;
constexpr size_t kSumValuesIdx = 1;
constexpr size_t kSumShapeIdx = 2;

bool SparseAddCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  size_t input_num = inputs.size();
  if (input_num != kInputNum) {
    MS_LOG(ERROR) << "For " << kernel_name_
                  << ", input should be a_indices, a_values, a_shape, b_indices, b_values, b_shape and thresh total "
                  << kInputNum << " tensors, but get " << input_num;
    return false;
  }
  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }

  for (size_t i = 0; i < kOutputNum; i++) {
    auto dtype = inputs[i]->dtype_id();
    (void)types_.emplace_back(dtype);
  }
  return true;
}

int SparseAddCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret == KRET_UNKNOWN_OUT_SHAPE) {
    MS_LOG(EXCEPTION) << "Resize failed for op: " << kernel_name();
  }
  auto dims = inputs.at(0)->GetShapeVector()[1];
  if (dims >= 0) {
    indices_column_ = LongToSize(dims);
  }
  return ret;
}

template <typename T>
int SparseAddCpuKernelMod::CompareTwoIndices(const T &a_indices, const T &b_indices, const int64_t a_row,
                                             const int64_t b_row, const size_t dims) const {
  for (int64_t dim = 0; dim < SizeToLong(dims); dim++) {
    auto a_idx = a_indices[a_row * dims + dim];
    auto b_idx = b_indices[b_row * dims + dim];
    if (a_idx < b_idx) {
      return -1;
    } else if (a_idx > b_idx) {
      return 1;
    }
  }
  return 0;
}

template <typename T, typename S, typename K>
bool SparseAddCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                         const std::vector<KernelTensor *> &,
                                         const std::vector<kernel::KernelTensor *> &outputs) {
  if (inputs.size() != kInputNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs should be " << kInputNum << ", but got "
                      << inputs.size() << " input(s).";
  }
  if (outputs.size() != kOutputNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of outputs should be " << kOutputNum << ", but got "
                      << outputs.size() << " output(s).";
  }
  // Inputs
  const auto a_indices = static_cast<T *>(inputs[kAIndicesIdx]->device_ptr());
  const auto a_values = static_cast<S *>(inputs[kAValuesIdx]->device_ptr());
  const auto a_shape = static_cast<T *>(inputs[kAShapeIdx]->device_ptr());
  const auto b_indices = static_cast<T *>(inputs[kBIndicesIdx]->device_ptr());
  const auto b_values = static_cast<S *>(inputs[kBValuesIdx]->device_ptr());
  const auto thresh = static_cast<K *>(inputs[kThreshIdx]->device_ptr());
  // Outputs
  auto sum_indices = static_cast<T *>(outputs[kSumIndicesIdx]->device_ptr());
  auto sum_values = static_cast<S *>(outputs[kSumValuesIdx]->device_ptr());
  auto sum_shape = static_cast<T *>(outputs[kSumShapeIdx]->device_ptr());

  const int64_t a_indices_num = SizeToLong(inputs[kAIndicesIdx]->size()) / SizeToLong((sizeof(T)) * indices_column_);
  const int64_t b_indices_num = SizeToLong(inputs[kBIndicesIdx]->size()) / SizeToLong((sizeof(T)) * indices_column_);

  // Use double pointer to calculate the sum of two inputs
  T i = 0;
  T j = 0;
  S sum_ab = 0;
  std::vector<std::pair<bool, T>> whole_indices;
  std::vector<S> whole_values;
  whole_indices.reserve(LongToSize(a_indices_num + b_indices_num));
  while (i < a_indices_num && j < b_indices_num) {
    switch (CompareTwoIndices(a_indices, b_indices, i, j, indices_column_)) {
      case -1:
        (void)whole_indices.emplace_back(true, i);
        whole_values.push_back(a_values[i]);
        i += 1;
        break;
      case 0:
        sum_ab = a_values[i] + b_values[j];
        if ((*thresh) <= std::abs(sum_ab)) {
          (void)whole_indices.emplace_back(true, i);
          whole_values.push_back(sum_ab);
        }
        i += 1;
        j += 1;
        break;
      case 1:
        (void)whole_indices.emplace_back(false, j);
        whole_values.push_back(b_values[j]);
        j += 1;
        break;
    }
  }

  if (i < a_indices_num) {
    while (i < a_indices_num) {
      (void)whole_indices.emplace_back(true, i);
      whole_values.push_back(a_values[i]);
      i += 1;
    }
  } else {
    while (j < b_indices_num) {
      (void)whole_indices.emplace_back(false, j);
      whole_values.push_back(b_values[j]);
      j += 1;
    }
  }

  for (size_t num = 0; num < whole_indices.size(); num++) {
    auto copy_from_a = whole_indices[num].first;
    auto index_from_input = whole_indices[num].second;
    if (copy_from_a) {
      for (size_t column = 0; column < indices_column_; column++) {
        sum_indices[num * indices_column_ + column] =
          a_indices[LongToSize(index_from_input) * indices_column_ + column];
      }
    } else {
      for (size_t column = 0; column < indices_column_; column++) {
        sum_indices[num * indices_column_ + column] =
          b_indices[LongToSize(index_from_input) * indices_column_ + column];
      }
    }
    sum_values[num] = whole_values[num];
  }

  for (size_t num_out = 0; num_out < indices_column_; num_out++) {
    sum_shape[num_out] = a_shape[num_out];
  }

  // Update output shape and type
  std::vector<int64_t> out_indices_shape;
  std::vector<int64_t> out_values_shape;
  (void)out_indices_shape.emplace_back(SizeToLong(whole_indices.size()));
  (void)out_indices_shape.emplace_back(SizeToLong(indices_column_));
  (void)out_values_shape.emplace_back(SizeToLong(whole_values.size()));
  outputs[kSumIndicesIdx]->SetShapeVector(out_indices_shape);
  auto ele_size =
    LongToSize(std::accumulate(out_indices_shape.begin(), out_indices_shape.end(), 1, std::multiplies<int64_t>()));
  outputs[kSumIndicesIdx]->set_size(ele_size * UnitSizeInBytes(outputs[kSumIndicesIdx]->dtype_id()));
  outputs[kSumValuesIdx]->SetShapeVector(out_values_shape);
  outputs[kSumValuesIdx]->set_size(whole_values.size() * UnitSizeInBytes(outputs[kSumValuesIdx]->dtype_id()));
  auto const &dense_shape = inputs.at(kAShapeIdx)->GetShapeVector();
  outputs[kSumShapeIdx]->SetShapeVector(dense_shape);
  ele_size = LongToSize(std::accumulate(dense_shape.begin(), dense_shape.end(), 1, std::multiplies<int64_t>()));
  outputs[kSumShapeIdx]->set_size(ele_size * UnitSizeInBytes(outputs[kSumShapeIdx]->dtype_id()));

  return true;
}

#define CPU_SPARSE_ADD_KERNEL_REGISTER(ms_index_type, ms_value_type, ms_thresh_type, index_type, value_type, \
                                       thresh_type)                                                          \
  {                                                                                                          \
    KernelAttr()                                                                                             \
      .AddInputAttr(ms_index_type)                                                                           \
      .AddInputAttr(ms_value_type)                                                                           \
      .AddInputAttr(ms_index_type)                                                                           \
      .AddInputAttr(ms_index_type)                                                                           \
      .AddInputAttr(ms_value_type)                                                                           \
      .AddInputAttr(ms_index_type)                                                                           \
      .AddInputAttr(ms_thresh_type)                                                                          \
      .AddOutputAttr(ms_index_type)                                                                          \
      .AddOutputAttr(ms_value_type)                                                                          \
      .AddOutputAttr(ms_index_type),                                                                         \
      &SparseAddCpuKernelMod::LaunchKernel<index_type, value_type, thresh_type>                              \
  }

const std::vector<std::pair<KernelAttr, SparseAddCpuKernelMod::KernelRunFunc>> &SparseAddCpuKernelMod::GetFuncList()
  const {
  static const std::vector<std::pair<KernelAttr, SparseAddCpuKernelMod::KernelRunFunc>> func_list = {
    // float values
    CPU_SPARSE_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeFloat32, kNumberTypeFloat32, int64_t, float, float),
    // double values
    CPU_SPARSE_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeFloat64, kNumberTypeFloat64, int64_t, double, double),
    // int8 values
    CPU_SPARSE_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt8, kNumberTypeInt8, int64_t, int8_t, int8_t),
    // int16 values
    CPU_SPARSE_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt16, kNumberTypeInt16, int64_t, int16_t, int16_t),
    // int values
    CPU_SPARSE_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt32, kNumberTypeInt32, int64_t, int, int),
    // int64 values
    CPU_SPARSE_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt64, kNumberTypeInt64, int64_t, int64_t, int64_t),
    // complex64 values
    CPU_SPARSE_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeComplex64, kNumberTypeFloat32, int64_t,
                                   std::complex<float>, float),
    // complex64 values
    CPU_SPARSE_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeComplex128, kNumberTypeFloat64, int64_t,
                                   std::complex<double>, double),
  };
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, SparseAdd, SparseAddCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
