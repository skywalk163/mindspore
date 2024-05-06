/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/sequence/sequence_less_cpu_kernel.h"
#include <algorithm>
#include <utility>
#include <complex>
#include <unordered_map>
#include "mindspore/core/ops/sequence_ops.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "include/common/thread_pool.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr int kInputsNum = 2;
constexpr int kOutputsNum = 1;
constexpr auto kTupleLe = "tuple_le";
constexpr auto kTupleLt = "tuple_lt";
constexpr auto kListLe = "list_le";
constexpr auto kListLt = "list_lt";
}  // namespace

template <typename T, typename S>
bool LessImpl(const T *in_x, const S *in_y, const size_t in_x_size, const size_t in_y_size,
              const bool is_less_equal = true) {
  size_t max_size = std::max(in_x_size, in_y_size);
  size_t zero_num = 0;
  if (in_x_size == zero_num && in_y_size > zero_num) {
    return true;
  }

  for (size_t i = 0; i < max_size; ++i) {
    if (i >= in_x_size) {
      return true;
    }
    if (i >= in_y_size) {
      return false;
    }
    if (static_cast<double>(in_x[i]) < static_cast<double>(in_y[i])) {
      return true;
    } else if (static_cast<double>(in_x[i]) > static_cast<double>(in_y[i])) {
      return false;
    }
  }
  return is_less_equal;
}

template <typename T, typename S>
void LtImpl(const T *in_x, const S *in_y, bool *out, const size_t in_x_size, const size_t in_y_size) {
  *out = LessImpl(in_x, in_y, in_x_size, in_y_size, false);
}

template <typename T, typename S>
void LeImpl(const T *in_x, const S *in_y, bool *out, const size_t in_x_size, const size_t in_y_size) {
  *out = LessImpl(in_x, in_y, in_x_size, in_y_size, true);
}

bool SequenceLessCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputsNum, kernel_name_);
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int SequenceLessCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputsNum, kernel_name_);
  auto input_0_shape = inputs[0]->GetShapeVector();
  auto input_1_shape = inputs[1]->GetShapeVector();
  if (input_0_shape.empty() || input_1_shape.empty()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the x and y shape can't be 0, but got " << GetShapes(inputs);
  }
  x_size_ = LongToSize(input_0_shape[0]);
  y_size_ = LongToSize(input_1_shape[0]);
  return KRET_OK;
}

template <typename T, typename S>
bool SequenceLessCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &,
                                            const std::vector<KernelTensor *> &outputs) {
  using InequalityImplFunc = std::function<void(const T *, const S *, bool *, const size_t, const size_t)>;
  std::unordered_map<std::string, InequalityImplFunc> func_map = {
    {kTupleLt, LtImpl<T, S>}, {kTupleLe, LeImpl<T, S>}, {kListLt, LtImpl<T, S>}, {kListLe, LeImpl<T, S>}};
  auto iter = func_map.find(kernel_name_);
  if (iter == func_map.end()) {
    MS_EXCEPTION(TypeError) << "For '" << kernel_name_ << "' don't support. Only support [Le, Lt]";
  }
  InequalityImplFunc compute_func = iter->second;

  const auto x_addr = GetDeviceAddress<T>(inputs, 0);
  const auto y_addr = GetDeviceAddress<S>(inputs, 1);
  bool *output_addr = GetDeviceAddress<bool>(outputs, 0);

  compute_func(x_addr, y_addr, output_addr, x_size_, y_size_);
  return true;
}

#define ADD_KERNEL(x_dtype, y_dtype, x_type, y_type)          \
  {                                                           \
    KernelAttr()                                              \
      .AddInputAttr(kObjectTypeTuple, kNumberType##x_dtype)   \
      .AddInputAttr(kObjectTypeTuple, kNumberType##y_dtype)   \
      .AddOutputAttr(kObjectTypeNumber, kNumberTypeBool),     \
      &SequenceLessCpuKernelMod::LaunchKernel<x_type, y_type> \
  }

const std::vector<std::pair<KernelAttr, SequenceLessCpuKernelMod::KernelRunFunc>>
  &SequenceLessCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, SequenceLessCpuKernelMod::KernelRunFunc>> func_list = {
    ADD_KERNEL(Float64, Float32, double, float), ADD_KERNEL(Float64, Float64, double, double),
    ADD_KERNEL(Float64, Int32, double, int),     ADD_KERNEL(Float64, Int64, double, int64_t),
    ADD_KERNEL(Float64, Bool, double, bool),     ADD_KERNEL(Float32, Float32, float, float),
    ADD_KERNEL(Float32, Float64, float, double), ADD_KERNEL(Float32, Int32, float, int),
    ADD_KERNEL(Float32, Int64, float, int64_t),  ADD_KERNEL(Float32, Bool, float, bool),
    ADD_KERNEL(Int32, Int32, int, int),          ADD_KERNEL(Int32, Int64, int, int64_t),
    ADD_KERNEL(Int32, Float32, int, float),      ADD_KERNEL(Int32, Float64, int, double),
    ADD_KERNEL(Int32, Bool, int, bool),          ADD_KERNEL(Int64, Float32, int64_t, float),
    ADD_KERNEL(Int64, Float64, int64_t, double), ADD_KERNEL(Int64, Int32, int64_t, int),
    ADD_KERNEL(Int64, Int64, int64_t, int64_t),  ADD_KERNEL(Int64, Bool, int64_t, bool),
    ADD_KERNEL(Bool, Int32, bool, int),          ADD_KERNEL(Bool, Int64, bool, int64_t),
    ADD_KERNEL(Bool, Float64, bool, double),     ADD_KERNEL(Bool, Float32, bool, float),
    ADD_KERNEL(Bool, Bool, bool, bool)};
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, tuple_le, SequenceLessCpuKernelMod);
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, tuple_lt, SequenceLessCpuKernelMod);
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, list_le, SequenceLessCpuKernelMod);
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, list_lt, SequenceLessCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
