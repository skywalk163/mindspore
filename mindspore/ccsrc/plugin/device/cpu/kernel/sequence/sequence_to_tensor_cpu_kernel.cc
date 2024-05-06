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

#include "plugin/device/cpu/kernel/sequence/sequence_to_tensor_cpu_kernel.h"
#include <algorithm>
#include <utility>
#include <complex>
#include "mindspore/core/ops/sequence_ops.h"
#include "mindspore/core/ops/arithmetic_ops.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "include/common/thread_pool.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr auto kTupleToTensor = "TupleToTensor";
constexpr auto kListToTensor = "ListToTensor";
constexpr auto kScalarToTensor = "ScalarToTensor";
}  // namespace

template <typename T, typename S>
void Cast(const T *in, S *out, size_t length) {
  for (size_t i = 0; i < length; i++) {
    out[i] = static_cast<S>(in[i]);
  }
}

bool SeqToTensorCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  if (kernel_type_ == kScalarToTensor) {
    kernel_func_ = scalar_func_list_[index].second;
  } else {
    kernel_func_ = seq_func_list_[index].second;
  }
  return true;
}

int SeqToTensorCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  auto tuple_shape = inputs[0]->GetShapeVector();
  if (tuple_shape.size() != 1 && !tuple_shape.empty()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the input_x element must be scalar or noshape tensor, but get shape:" << tuple_shape;
  }
  return KRET_OK;
}

template <typename T, typename S>
bool SeqToTensorCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &,
                                           const std::vector<KernelTensor *> &outputs) {
  const auto input_addr = GetDeviceAddress<T>(inputs, 0);
  auto output_addr = GetDeviceAddress<S>(outputs, 0);
  auto input_size = inputs[0]->size() / sizeof(T);
  auto output_size = outputs[0]->size() / sizeof(S);
  if (input_size != output_size) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the size of 'input_x': {" << input_size
                      << "} is not equal to the size of output: {" << output_size << "}";
  }
  Cast<T, S>(input_addr, output_addr, input_size);
  return true;
}

#define ADD_TUPLE_KERNEL(x_dtype, out_dtype, in_type, out_type)  \
  {                                                              \
    KernelAttr()                                                 \
      .AddInputAttr(kObjectTypeTuple, kNumberType##x_dtype)      \
      .AddOptionalInputAttr(kObjectTypeNumber, kNumberTypeInt64) \
      .AddOutputAttr(kNumberType##out_dtype),                    \
      &SeqToTensorCpuKernelMod::LaunchKernel<in_type, out_type>  \
  }

#define ADD_SCALAR_KERNEL(x_dtype, out_dtype, in_type, out_type) \
  {                                                              \
    KernelAttr()                                                 \
      .AddInputAttr(kObjectTypeNumber, kNumberType##x_dtype)     \
      .AddOptionalInputAttr(kObjectTypeNumber, kNumberTypeInt64) \
      .AddOutputAttr(kNumberType##out_dtype),                    \
      &SeqToTensorCpuKernelMod::LaunchKernel<in_type, out_type>  \
  }

std::vector<std::pair<KernelAttr, SeqToTensorCpuKernelMod::SeqToTensorFunc>> SeqToTensorCpuKernelMod::seq_func_list_ = {
  ADD_TUPLE_KERNEL(Float32, Float32, float, float),  ADD_TUPLE_KERNEL(Float32, Float64, float, double),
  ADD_TUPLE_KERNEL(Float32, Int32, float, int32_t),  ADD_TUPLE_KERNEL(Float32, Int64, float, int64_t),
  ADD_TUPLE_KERNEL(Float64, Float32, double, float), ADD_TUPLE_KERNEL(Float64, Float64, double, double),
  ADD_TUPLE_KERNEL(Float64, Int32, double, int32_t), ADD_TUPLE_KERNEL(Float64, Int64, double, int64_t),
  ADD_TUPLE_KERNEL(Int32, Float32, int32_t, float),  ADD_TUPLE_KERNEL(Int32, Float64, int32_t, double),
  ADD_TUPLE_KERNEL(Int32, Int32, int32_t, int32_t),  ADD_TUPLE_KERNEL(Int32, Int64, int32_t, int64_t),
  ADD_TUPLE_KERNEL(Int64, Float32, int64_t, float),  ADD_TUPLE_KERNEL(Int64, Float64, int64_t, double),
  ADD_TUPLE_KERNEL(Int64, Int32, int64_t, int32_t),  ADD_TUPLE_KERNEL(Int64, Int64, int64_t, int64_t),
  ADD_TUPLE_KERNEL(Bool, Bool, bool, bool)};

std::vector<std::pair<KernelAttr, SeqToTensorCpuKernelMod::SeqToTensorFunc>>
  SeqToTensorCpuKernelMod::scalar_func_list_ = {
    ADD_SCALAR_KERNEL(Float32, Float32, float, float),  ADD_SCALAR_KERNEL(Float32, Float64, float, double),
    ADD_SCALAR_KERNEL(Float32, Int32, float, int32_t),  ADD_SCALAR_KERNEL(Float32, Int64, float, int64_t),
    ADD_SCALAR_KERNEL(Float32, Bool, float, bool),      ADD_SCALAR_KERNEL(Float64, Bool, double, bool),
    ADD_SCALAR_KERNEL(Float64, Float32, double, float), ADD_SCALAR_KERNEL(Float64, Float64, double, double),
    ADD_SCALAR_KERNEL(Float64, Int32, double, int32_t), ADD_SCALAR_KERNEL(Float64, Int64, double, int64_t),
    ADD_SCALAR_KERNEL(Int32, Float32, int32_t, float),  ADD_SCALAR_KERNEL(Int32, Float64, int32_t, double),
    ADD_SCALAR_KERNEL(Int32, Int32, int32_t, int32_t),  ADD_SCALAR_KERNEL(Int32, Int64, int32_t, int64_t),
    ADD_SCALAR_KERNEL(Int32, Bool, int32_t, bool),      ADD_SCALAR_KERNEL(Int64, Bool, int64_t, bool),
    ADD_SCALAR_KERNEL(Int64, Float32, int64_t, float),  ADD_SCALAR_KERNEL(Int64, Float64, int64_t, double),
    ADD_SCALAR_KERNEL(Int64, Int32, int64_t, int32_t),  ADD_SCALAR_KERNEL(Int64, Int64, int64_t, int64_t),
    ADD_SCALAR_KERNEL(Bool, Float32, bool, float),      ADD_SCALAR_KERNEL(Bool, Float64, bool, double),
    ADD_SCALAR_KERNEL(Bool, Int32, bool, int32_t),      ADD_SCALAR_KERNEL(Bool, Int64, bool, int64_t),
    ADD_SCALAR_KERNEL(Bool, Bool, bool, bool)};

std::vector<KernelAttr> SeqToTensorCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  if (kernel_type_ == kScalarToTensor) {
    (void)std::transform(scalar_func_list_.begin(), scalar_func_list_.end(), std::back_inserter(support_list),
                         [](const std::pair<KernelAttr, SeqToTensorFunc> &item) { return item.first; });
  } else {
    (void)std::transform(seq_func_list_.begin(), seq_func_list_.end(), std::back_inserter(support_list),
                         [](const std::pair<KernelAttr, SeqToTensorFunc> &item) { return item.first; });
  }
  return support_list;
}

MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, TupleToTensor,
                                 []() { return std::make_shared<SeqToTensorCpuKernelMod>(kTupleToTensor); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, ScalarToTensor,
                                 []() { return std::make_shared<SeqToTensorCpuKernelMod>(kScalarToTensor); });
}  // namespace kernel
}  // namespace mindspore
