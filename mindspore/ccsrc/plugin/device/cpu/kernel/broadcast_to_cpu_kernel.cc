/**
 * Copyright 2021-2023 Huawei Technologies Co., Ltd
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

#include <algorithm>
#include <utility>
#include <complex>
#include "plugin/device/cpu/kernel/nnacl/errorcode.h"
#include "mindspore/core/ops/array_ops.h"
#include "plugin/device/cpu/kernel/broadcast_to_cpu_kernel.h"

namespace mindspore {
namespace kernel {
namespace {
using complex64 = std::complex<float>;
using complex128 = std::complex<double>;
constexpr size_t kBroadcastToOutputsNum = 1;
}  // namespace

#define BROADCAST_TO_CPU_REG(MS_T, T)                                                                   \
  KernelAttr().AddInputAttr(MS_T).AddInputAttr(kObjectTypeTuple, kNumberTypeInt64).AddOutputAttr(MS_T), \
    &BroadcastToCpuKernelMod::LaunchKernel<T>

std::map<std::string, std::vector<std::pair<KernelAttr, BroadcastToCpuKernelMod::BroadcastToFunc>>>
  BroadcastToCpuKernelMod::func_list_ = {{kBroadcastTo,
                                          {{BROADCAST_TO_CPU_REG(kNumberTypeInt8, int8_t)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeInt16, int16_t)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeInt32, int32_t)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeInt64, int64_t)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeUInt8, uint8_t)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeUInt16, uint16_t)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeUInt32, uint32_t)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeUInt64, uint64_t)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeFloat16, float16)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeFloat32, float)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeFloat64, double)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeComplex64, complex64)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeComplex128, complex128)},
                                           {BROADCAST_TO_CPU_REG(kNumberTypeBool, bool)}}},
                                         {kDynamicBroadcastTo,
                                          {{KernelAttr()
                                              .AddInputAttr(kNumberTypeFloat32)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt32)
                                              .AddOutputAttr(kNumberTypeFloat32),
                                            &BroadcastToCpuKernelMod::LaunchKernel<float>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeFloat32)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeFloat32),
                                            &BroadcastToCpuKernelMod::LaunchKernel<float>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeInt32)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt32)
                                              .AddOutputAttr(kNumberTypeInt32),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeBool)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt32)
                                              .AddOutputAttr(kNumberTypeBool),
                                            &BroadcastToCpuKernelMod::LaunchKernel<bool>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeInt8)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt32)
                                              .AddOutputAttr(kNumberTypeInt8),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeFloat32)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeFloat32),
                                            &BroadcastToCpuKernelMod::LaunchKernel<float>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeInt32)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeInt32),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeBool)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeBool),
                                            &BroadcastToCpuKernelMod::LaunchKernel<bool>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeInt8)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeInt8),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeUInt8)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeUInt8),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeUInt16)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeUInt16),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeUInt32)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeUInt32),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeUInt64)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeUInt64),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int>},
                                           {KernelAttr()
                                              .AddInputAttr(kNumberTypeInt64)
                                              .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
                                              .AddOutputAttr(kNumberTypeUInt64),
                                            &BroadcastToCpuKernelMod::LaunchKernel<int64_t>}}}};

bool BroadcastToCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  kernel_type_ = kernel_name_;
  auto iter = func_list_.find(kernel_type_);
  if (iter == func_list_.end()) {
    MS_LOG(EXCEPTION) << "BroadcastTo cpu does not support " << kernel_type_;
  }

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' does not support this kernel type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[kernel_type_][index].second;
  return true;
}

int BroadcastToCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  input_shape_ = inputs[kIndex0]->GetShapeVector();
  output_shape_ = outputs[kIndex0]->GetShapeVector();

  auto it_x = std::find_if(input_shape_.begin(), input_shape_.end(), [](int64_t sh) { return sh < 0; });
  if (it_x != input_shape_.end()) {
    return KRET_UNKNOWN_SHAPE;
  }

  size_t input_shape_size = input_shape_.size();
  size_t output_shape_size = output_shape_.size();

  for (size_t i = 0; i < input_shape_size; ++i) {
    shape_info_.input_shape_[i] = LongToInt(input_shape_[i]);
  }
  for (size_t i = 0; i < output_shape_size; ++i) {
    shape_info_.output_shape_[i] = LongToInt(output_shape_[i]);
  }
  shape_info_.input_shape_size_ = SizeToInt(input_shape_size);
  shape_info_.output_shape_size_ = SizeToInt(output_shape_size);
  int ret = KernelMod::Resize(inputs, outputs);
  return ret;
}

void BroadcastToCpuKernelMod::CheckArgs() {
  size_t input_shape_size = input_shape_.size();
  size_t output_shape_size = output_shape_.size();
  if (output_shape_size < input_shape_size) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', input tensor 'input_x' and target shape 'shape' can't "
                         "broadcast. The dimension of 'input_x' is "
                      << input_shape_size << ", and the dimension of target shape 'shape' is " << output_shape_size;
  }
  if (output_shape_size > MAX_SHAPE_SIZE) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', input tensor 'input_x' and target shape 'shape' must be "
                         "broadcast, and the dimension of target shape 'shape' must be at most 8. "
                         "But got the dimension of 'input_x': "
                      << input_shape_size << ", and the dimension of target shape 'shape': " << output_shape_size;
  }
  size_t offset = output_shape_size - input_shape_size;
  for (size_t i = 0; i < input_shape_size; ++i) {
    if (input_shape_[i] != output_shape_[i + offset] && input_shape_[i] != 1) {
      MS_LOG(EXCEPTION)
        << "For '" << kernel_name_ << "', when the " << i
        << "'th, the shape of input must be 1 and equal to the shape of output, but got the shape of input: "
        << input_shape_ << ", and the shape of output: " << output_shape_;
    }
  }
}

template <typename T>
bool BroadcastToCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &,
                                           const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kBroadcastToOutputsNum, kernel_name_);
  CheckArgs();

  if (std::find(input_shape_.begin(), input_shape_.end(), 0) != input_shape_.end() &&
      std::find(output_shape_.begin(), output_shape_.end(), 0) != output_shape_.end()) {
    return true;
  }

  const void *input_addr = inputs[0]->device_ptr();
  void *output_addr = outputs[0]->device_ptr();

  if (output_shape_.empty()) {
    *(reinterpret_cast<T *>(output_addr)) = *(reinterpret_cast<const T *>(input_addr));
    return true;
  }

  int status = static_cast<int>(NNACL_OK);
  if constexpr (std::is_same_v<T, bool>) {
    status = BroadcastToSize8(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, int8_t>) {
    status = BroadcastToSize8(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, int16_t>) {
    status = BroadcastToSize16(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, int32_t>) {
    status = BroadcastToSize32(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, int64_t>) {
    status = BroadcastToSize64(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, uint8_t>) {
    status = BroadcastToSize8(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    status = BroadcastToSize16(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    status = BroadcastToSize32(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, uint64_t>) {
    status = BroadcastToSize64(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, float16>) {
    status = BroadcastToSize16(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, float>) {
    status = BroadcastToSize32(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, double>) {
    status = BroadcastToSize64(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, complex64>) {
    status = BroadcastToSize64(input_addr, &shape_info_, output_addr);
  } else if constexpr (std::is_same_v<T, complex128>) {
    status = BroadcastToSize128(input_addr, &shape_info_, output_addr);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', not supported data type, the dtype of input must be bool, int, complex, float or double";
  }

  if (status != static_cast<int>(NNACL_OK)) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', each dimension pair, 'input_x' shape and target shape, "
                         "must be either equal or input is one or the target dimension is -1. "
                         "But got 'input_x' shape: "
                      << input_shape_ << " and target shape: " << output_shape_ << ". Error code: " << status;
  }
  return true;
}

std::vector<KernelAttr> BroadcastToCpuKernelMod::GetOpSupport() {
  auto iter = func_list_.find(kernel_type_);
  if (iter == func_list_.end()) {
    MS_LOG(EXCEPTION) << "not support " << kernel_type_ << "!";
  }

  std::vector<KernelAttr> support_list;
  (void)std::transform(iter->second.begin(), iter->second.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, BroadcastToFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, BroadcastTo,
                                 []() { return std::make_shared<BroadcastToCpuKernelMod>(kBroadcastTo); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, DynamicBroadcastTo,
                                 []() { return std::make_shared<BroadcastToCpuKernelMod>(kDynamicBroadcastTo); });
}  // namespace kernel
}  // namespace mindspore
