/**
 * Copyright 2022-2023 Huawei Technologies Co., Ltd
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
#include "plugin/device/cpu/kernel/sparse_matrix_add_cpu_kernel.h"
#include <algorithm>
#include <complex>
#include <functional>
#include <utility>
#include <set>
#include <map>
#include <string>
#include "include/common/thread_pool.h"
#include "mindspore/core/ops/sparse_matrix_add.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kMatrixDims = 2;
constexpr size_t kInputNum = 12;
constexpr size_t kOutputNum = 5;
constexpr size_t kADenseShapeIdx = 0;
constexpr size_t kABatchPtrIdx = 1;
constexpr size_t kAIndptrIdx = 2;
constexpr size_t kAIndicesIdx = 3;
constexpr size_t kAValuesIdx = 4;
constexpr size_t kBDenseShapeIdx = 5;
constexpr size_t kBBatchPtrIdx = 6;
constexpr size_t kBIndptrIdx = 7;
constexpr size_t kBIndicesIdx = 8;
constexpr size_t kBValuesIdx = 9;
constexpr size_t kAlphaIdx = 10;
constexpr size_t kBetaIdx = 11;
constexpr size_t kOutDenseShape = 0;
constexpr size_t kOutBatch = 1;
constexpr size_t kOutIndptr = 2;
constexpr size_t kOutIndices = 3;
constexpr size_t kOutValue = 4;
using KernelRunFunc = SparseMatrixAddCpuKernelMod::KernelRunFunc;
}  // namespace
bool SparseMatrixAddCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  size_t input_num = inputs.size();
  if (input_num != kInputNum) {
    MS_LOG(ERROR) << "For " << kernel_name_
                  << ", input should be A(indptr, indices, values), B(indptr, indeices, values), alpha, beta total "
                  << kInputNum << " tensors, but get " << input_num;
    return false;
  }
  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }
  return true;
}

int SparseMatrixAddCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret == KRET_UNKNOWN_OUT_SHAPE) {
    if (inputs.size() != kInputNum) {
      MS_LOG(ERROR) << "The size of inputs should be " << kInputNum << ", but got " << inputs.size();
      return KRET_RESIZE_FAILED;
    }
    auto indptr_shape = inputs.at(kAIndptrIdx)->GetShapeVector();
    if (indptr_shape[0] < 0) {
      return ret;
    }
    row_ = LongToSize(indptr_shape[0] - 1);
    for (size_t i = 0; i < kOutputNum; i++) {
      auto dtype = inputs[i]->dtype_id();
      (void)types_.emplace_back(dtype);
    }
    output_size_list_.clear();
    (void)output_size_list_.emplace_back(inputs[kADenseShapeIdx]->size());  // dense shape
    (void)output_size_list_.emplace_back(inputs[kABatchPtrIdx]->size());    // batch
    (void)output_size_list_.emplace_back(inputs[kAIndptrIdx]->size());      // indptr
    auto max_out_size = inputs[kAIndicesIdx]->size() + inputs[kBIndicesIdx]->size();
    (void)output_size_list_.emplace_back(max_out_size);  // index
    (void)output_size_list_.emplace_back(max_out_size / GetTypeByte(TypeIdToType(types_[kAIndicesIdx])) *
                                         GetTypeByte(TypeIdToType(types_[kAValuesIdx])));  // value
    // set dense and batch shape for output.
    auto out_dense_shape = inputs[kADenseShapeIdx]->GetShapeVector();
    auto ele_size =
      LongToSize(std::accumulate(out_dense_shape.begin(), out_dense_shape.end(), 1, std::multiplies<int64_t>()));
    outputs[kOutDenseShape]->SetShapeVector(out_dense_shape);
    outputs[kOutDenseShape]->set_size(ele_size * UnitSizeInBytes(outputs[kOutDenseShape]->dtype_id()));
    auto out_batch_shape = inputs[kABatchPtrIdx]->GetShapeVector();
    ele_size =
      LongToSize(std::accumulate(out_batch_shape.begin(), out_batch_shape.end(), 1, std::multiplies<int64_t>()));
    outputs[kOutBatch]->SetShapeVector(out_batch_shape);
    outputs[kOutBatch]->set_size(ele_size * UnitSizeInBytes(outputs[kOutBatch]->dtype_id()));
    ret = KRET_OK;
  }
  return ret;
}

template <typename T>
void CheckInputValid(const T *input, const size_t &size, const std::string &name) {
  for (size_t i = 0; i < size - 1; i++) {
    if (input[i] > input[i + 1] || input[i] < 0 || input[i + 1] < 0) {
      std::vector<T> v(input, input + size);
      MS_LOG_EXCEPTION << "For SparseMatrixAdd, " << name << " must non-negative and increasing, but got " << v;
    }
  }
}

template <typename T, typename S>
bool SparseMatrixAddCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                               const std::vector<KernelTensor *> &,
                                               const std::vector<KernelTensor *> &outputs) {
  if (inputs.size() != kInputNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of inputs should be " << kInputNum << ", but got "
                      << inputs.size() << " input(s).";
  }
  if (outputs.size() != kOutputNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of outputs should be " << kOutputNum << ", but got "
                      << outputs.size() << " output(s).";
  }
  const auto a_batch_size = inputs[kABatchPtrIdx]->size() / sizeof(T);
  const auto a_dense_shape = reinterpret_cast<T *>(inputs[kADenseShapeIdx]->device_ptr());
  const auto a_indptr = reinterpret_cast<T *>(inputs[kAIndptrIdx]->device_ptr());
  const auto a_indices = reinterpret_cast<T *>(inputs[kAIndicesIdx]->device_ptr());
  const auto a_values = reinterpret_cast<S *>(inputs[kAValuesIdx]->device_ptr());
  const auto b_indptr = reinterpret_cast<T *>(inputs[kBIndptrIdx]->device_ptr());
  const auto b_indices = reinterpret_cast<T *>(inputs[kBIndicesIdx]->device_ptr());
  const auto b_values = reinterpret_cast<S *>(inputs[kBValuesIdx]->device_ptr());
  const auto alpha = reinterpret_cast<S *>(inputs[kAlphaIdx]->device_ptr());
  const auto beta = reinterpret_cast<S *>(inputs[kBetaIdx]->device_ptr());
  auto c_indptr = reinterpret_cast<T *>(outputs[kOutIndptr]->device_ptr());
  auto c_indices = reinterpret_cast<T *>(outputs[kOutIndices]->device_ptr());
  auto c_values = reinterpret_cast<S *>(outputs[kOutValue]->device_ptr());
  auto c_dense_shape = reinterpret_cast<T *>(outputs[kOutDenseShape]->device_ptr());
  auto c_batch = reinterpret_cast<T *>(outputs[kOutBatch]->device_ptr());
  // Consider the dense shape of input and output are the same.
  auto ret = memcpy_s(c_dense_shape, outputs[kOutDenseShape]->size(), a_dense_shape, inputs[kADenseShapeIdx]->size());
  if (ret != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', launch kernel error: memcpy failed. Error no: " << ret;
  }
  CheckInputValid(a_indptr, inputs[kAIndptrIdx]->size() / sizeof(T), "a indptr");
  CheckInputValid(b_indptr, inputs[kBIndptrIdx]->size() / sizeof(T), "b indptr");
  auto batch_size = static_cast<size_t>(a_batch_size > 1 ? (a_batch_size - 1) : 1);
  c_batch[0] = 0;
  // Do the compute: C = alpha * A + beta * B.
  c_indptr[0] = 0;
  std::set<T> index_set;
  size_t c_idx = 0;
  S a_v = 0;
  S b_v = 0;
  size_t a_v_idx = 0;
  size_t b_v_idx = 0;
  for (size_t s = 0; s < batch_size; s++) {  // loop for all batches
    auto task = [this, &a_indptr, &a_indices, &a_values, &b_indptr, &b_indices, &b_values, &alpha, &beta, &c_indptr,
                 &c_indices, &c_values, &index_set, &c_idx, &a_v, &b_v, &a_v_idx, &b_v_idx,
                 &s](size_t start, size_t end) {
      for (size_t x = start; x < end; x++) {  // one batch
        auto i = x + s;
        size_t max_mask_len = static_cast<size_t>(a_indptr[i + 1] - a_indptr[i] + b_indptr[i + 1] - b_indptr[i]);
        // Masks for recording the valid location.
        std::vector<bool> mask_a(max_mask_len, false);
        std::vector<bool> mask_b(max_mask_len, false);
        for (size_t j = static_cast<size_t>(a_indptr[i]); j < static_cast<size_t>(a_indptr[i + 1]); j++) {
          (void)index_set.insert(a_indices[j]);
          mask_a[static_cast<size_t>(a_indices[j])] = true;
        }
        for (size_t k = static_cast<size_t>(b_indptr[i]); k < static_cast<size_t>(b_indptr[i + 1]); k++) {
          (void)index_set.insert(b_indices[k]);
          mask_b[static_cast<size_t>(b_indices[k])] = true;
        }
        // index_set.size() are the valid numbers to set indptr.
        c_indptr[i + 1] = c_indptr[i] + static_cast<T>(index_set.size());
        for (auto it = index_set.cbegin(); it != index_set.cend(); it++) {
          if (mask_a[static_cast<size_t>(*it)]) {
            // Independent cursor for indeices to get value. Increase the cursor once used.
            a_v = a_values[a_v_idx];
            a_v_idx++;
          }
          if (mask_b[static_cast<size_t>(*it)]) {
            b_v = b_values[b_v_idx];
            b_v_idx++;
          }
          c_values[c_idx] = alpha[IntToSize(0)] * a_v + beta[IntToSize(0)] * b_v;
          c_indices[c_idx] = *it;
          c_idx++;
          b_v = 0;  // Reset the tmp value, real number or zero.
          a_v = 0;
        }
        index_set.clear();
      }
    };
    ParallelLaunchAutoSearch(task, row_, this, &parallel_search_info_);
  }
  c_batch[1] = static_cast<T>(c_idx);
  // Update output shape and type
  std::vector<int64_t> out_indptr_shape;
  std::vector<int64_t> out_indices_shape;
  std::vector<int64_t> out_values_shape;
  (void)out_indptr_shape.emplace_back(SizeToLong(batch_size * (row_ + 1)));
  (void)out_indices_shape.emplace_back(SizeToLong(c_idx));
  (void)out_values_shape.emplace_back(SizeToLong(c_idx));
  outputs[kOutIndptr]->SetShapeVector(out_indptr_shape);
  outputs[kOutIndptr]->set_size(batch_size * (row_ + 1) * UnitSizeInBytes(outputs[kOutIndptr]->dtype_id()));
  outputs[kOutIndices]->SetShapeVector(out_indices_shape);
  outputs[kOutIndices]->set_size(c_idx * UnitSizeInBytes(outputs[kOutIndices]->dtype_id()));
  outputs[kOutValue]->SetShapeVector(out_values_shape);
  outputs[kOutValue]->set_size(c_idx * UnitSizeInBytes(outputs[kOutValue]->dtype_id()));
  return true;
}

#define CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(ms_index_type, ms_value_type, index_type, value_type) \
  {                                                                                                 \
    KernelAttr()                                                                                    \
      .AddInputAttr(ms_index_type)                                                                  \
      .AddInputAttr(ms_index_type)                                                                  \
      .AddInputAttr(ms_index_type)                                                                  \
      .AddInputAttr(ms_index_type)                                                                  \
      .AddInputAttr(ms_value_type)                                                                  \
      .AddInputAttr(ms_index_type)                                                                  \
      .AddInputAttr(ms_index_type)                                                                  \
      .AddInputAttr(ms_index_type)                                                                  \
      .AddInputAttr(ms_index_type)                                                                  \
      .AddInputAttr(ms_value_type)                                                                  \
      .AddInputAttr(ms_value_type)                                                                  \
      .AddInputAttr(ms_value_type)                                                                  \
      .AddOutputAttr(ms_index_type)                                                                 \
      .AddOutputAttr(ms_index_type)                                                                 \
      .AddOutputAttr(ms_index_type)                                                                 \
      .AddOutputAttr(ms_index_type)                                                                 \
      .AddOutputAttr(ms_value_type),                                                                \
      &SparseMatrixAddCpuKernelMod::LaunchKernel<index_type, value_type>                            \
  }

const std::vector<std::pair<KernelAttr, KernelRunFunc>> &SparseMatrixAddCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, KernelRunFunc>> func_list = {
    // float values
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeFloat32, int, float),
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeFloat32, int64_t, float),
    // double values
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeFloat64, int, double),
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeFloat64, int64_t, double),
    // int values
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeInt32, int, int),
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeInt32, int64_t, int),
    // complex46 values
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeComplex64, int, std::complex<float>),
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeComplex64, int64_t, std::complex<float>),
    // complex46 values
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt32, kNumberTypeComplex128, int, std::complex<double>),
    CPU_SPARSE_MATRIX_ADD_KERNEL_REGISTER(kNumberTypeInt64, kNumberTypeComplex128, int64_t, std::complex<double>),
  };
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, SparseMatrixAdd, SparseMatrixAddCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
