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

#include "plugin/device/cpu/kernel/truncate_mod_cpu_kernel.h"

#include <limits>
#include <algorithm>
#include <functional>
#include <utility>
#include <vector>
#include <map>

namespace mindspore {
namespace kernel {
namespace {
const size_t kZero = 0;
const size_t kOne = 1;
constexpr size_t kTruncateModInputsNum = 2;
constexpr size_t kTruncateModOutputsNum = 1;
}  // namespace

template <typename T>
T GetTruncModDivZeroVal(const T &v) {
  auto zero = static_cast<T>(0.0);
  if (std::numeric_limits<T>::has_infinity) {
    return v > zero ? std::numeric_limits<T>::infinity() : -std::numeric_limits<T>::infinity();
  } else {
    return v > zero ? std::numeric_limits<T>::max() : std::numeric_limits<T>::min();
  }
}

float16 GetTruncModDivZeroVal(const float16 &v) {
  auto zero = static_cast<float16>(0.0);
  if (std::numeric_limits<float16>::has_infinity) {
    return v > zero ? std::numeric_limits<float16>::infinity() : -std::numeric_limits<float16>::infinity();
  } else {
    return v > zero ? std::numeric_limits<float16>::max() : std::numeric_limits<float16>::min();
  }
}

template <typename T>
T SafeModOp(const T &a, const T &b) {
  if (b != 0) {
    return std::fmod(a, b);
  } else {
    return 0;
  }
}

float16 SafeModOp(const float16 &a, const float16 &b) {
  float ret_float = std::fmod(static_cast<float>(a), static_cast<float>(b));
  return static_cast<float16>(ret_float);
}

float SafeModOp(const float &a, const float &b) { return std::fmod(a, b); }

double SafeModOp(const double &a, const double &b) { return std::fmod(a, b); }

bool TruncateModCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(EXCEPTION) << "TruncateMod does not support this kernel data type: " << kernel_attr;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int TruncateModCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  input_shape_1_ = inputs[0]->GetShapeVector();
  input_shape_2_ = inputs[1]->GetShapeVector();
  output_shape_ = outputs[0]->GetShapeVector();

  return KRET_OK;
}

template <typename T>
bool TruncateModCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                           const std::vector<kernel::KernelTensor *> &,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kTruncateModInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kTruncateModOutputsNum, kernel_name_);
  auto *input_addr_a = reinterpret_cast<T *>(inputs[kZero]->device_ptr());
  auto *input_addr_b = reinterpret_cast<T *>(inputs[kOne]->device_ptr());
  auto *output_addr = reinterpret_cast<T *>(outputs[kZero]->device_ptr());
  size_t output_size = outputs[0]->size() / sizeof(T);
  if (input_shape_1_ == input_shape_2_) {
    auto task = [output_addr, input_addr_a, input_addr_b](size_t start, size_t end) {
      for (size_t i = start; i < end; ++i) {
        auto dividend = input_addr_a[i];
        auto divisor = input_addr_b[i];
        output_addr[i] = SafeModOp(dividend, divisor);
      }
    };
    ParallelLaunchAutoSearch(task, output_size, this, &parallel_search_info_);
  } else {  // For Broadcast
    BroadcastIterator base_iter(input_shape_1_, input_shape_2_, output_shape_);
    auto task = [&base_iter, output_addr, input_addr_a, input_addr_b](size_t start, size_t end) {
      auto iter = base_iter;
      iter.SetPos(start);
      for (size_t i = start; i < end; ++i) {
        auto dividend = input_addr_a[iter.GetInputPosA()];
        auto divisor = input_addr_b[iter.GetInputPosB()];
        output_addr[i] = SafeModOp(dividend, divisor);
        iter.GenNextPos();
      }
    };
    ParallelLaunchAutoSearch(task, output_size, this, &parallel_search_info_);
  }
  return true;
}

std::vector<std::pair<KernelAttr, TruncateModCpuKernelMod::TruncateModFunc>> TruncateModCpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &TruncateModCpuKernelMod::LaunchKernel<int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &TruncateModCpuKernelMod::LaunchKernel<int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
   &TruncateModCpuKernelMod::LaunchKernel<int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
   &TruncateModCpuKernelMod::LaunchKernel<int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
   &TruncateModCpuKernelMod::LaunchKernel<uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
   &TruncateModCpuKernelMod::LaunchKernel<uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
   &TruncateModCpuKernelMod::LaunchKernel<uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
   &TruncateModCpuKernelMod::LaunchKernel<uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &TruncateModCpuKernelMod::LaunchKernel<double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &TruncateModCpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &TruncateModCpuKernelMod::LaunchKernel<float16>}};

std::vector<KernelAttr> TruncateModCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, TruncateModFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, TruncateMod, TruncateModCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
