/**
 * Copyright 2020-2022 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License">},;
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

#include "plugin/device/gpu/kernel/arrays/cast_gpu_kernel.h"
#include <vector>
#include <utility>
#include <map>
#include <algorithm>

namespace mindspore {
namespace kernel {
bool CastGpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  ResetKernelFunc(inputs, outputs);
  return true;
}

int CastGpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  input_size_ = 0;
  MS_EXCEPTION_IF_NULL(inputs[kIndex0]);
  MS_EXCEPTION_IF_NULL(outputs[kIndex0]);
  ResetKernelFunc(inputs, outputs);
  auto input_shape = inputs[kIndex0]->GetShapeVector();
  auto output_shape = outputs[kIndex0]->GetShapeVector();
  is_null_input_ =
    CHECK_SHAPE_NULL(input_shape, kernel_name_, "input") || CHECK_SHAPE_NULL(output_shape, kernel_name_, "output");
  if (is_null_input_) {
    output_size_list_ = {0};
    return KRET_OK;
  }
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }
  input_size_ = SizeOf(inputs[kIndex0]->GetShapeVector());
  return KRET_OK;
}

void CastGpuKernelMod::ResetKernelFunc(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
  }
  kernel_func_ = func_list_[index].second;
}

template <typename S, typename T>
bool CastGpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                    const std::vector<KernelTensor *> &outputs, void *stream_ptr) {
  if (is_null_input_) {
    return true;
  }
  S *input_addr = GetPossiblyNullDeviceAddress<S>(inputs, kIndex0);
  T *output_addr = GetPossiblyNullDeviceAddress<T>(outputs, kIndex0);

  if (input_addr == nullptr && output_addr == nullptr) {
    return true;
  } else if (input_addr != nullptr && output_addr != nullptr) {
    auto status = Cast(input_size_, input_addr, output_addr, reinterpret_cast<cudaStream_t>(stream_ptr));
    CHECK_CUDA_STATUS(status, kernel_name_);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the input and output device addresses must be both null or both not null";
  }

  return true;
}

template <typename T>
using Complex = mindspore::utils::Complex<T>;
std::vector<std::pair<KernelAttr, CastGpuKernelMod::CastFunc>> CastGpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int8_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int8_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int8_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int8_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int8_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int8_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int8_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int8_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int16_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int16_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int16_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int16_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int16_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int16_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int16_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int16_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int32_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int32_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int32_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int32_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int32_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int32_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int32_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int32_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int64_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int64_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int64_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int64_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int64_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int64_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int64_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int64_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint8_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint8_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint16_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint16_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint32_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint32_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint64_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint64_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<half, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<half, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<half, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<half, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<half, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<half, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<half, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<half, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<half, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<half, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<half, half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<half, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<half, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<half, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<float, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<float, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<float, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<float, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<float, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<float, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<float, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<float, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<float, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<float, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<float, half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<float, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<float, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<float, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<double, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<double, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<double, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<double, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<double, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<double, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<double, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<double, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<double, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<double, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<double, half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<double, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<double, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<double, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<bool, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<bool, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<bool, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<bool, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<bool, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<bool, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<bool, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<bool, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<bool, float>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<bool, double>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<bool, half>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<bool, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<bool, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<bool, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, float>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, double>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, half>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, float>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, double>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, half>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, Complex<float>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int8_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int8_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int8_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int8_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int8_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int8_t, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int8_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int8_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt8).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int8_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int16_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int16_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int16_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int16_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int16_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int16_t, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int16_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int16_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt16).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int16_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int32_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int32_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int32_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int32_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int32_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int32_t, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int32_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int32_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int32_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int64_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int64_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int64_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int64_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int64_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int64_t, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int64_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int64_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int64_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint8_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint8_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint16_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint16_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint32_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint32_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint64_t, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint64_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<half, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<half, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<half, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<half, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<half, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<half, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<half, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<half, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<half, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<half, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<half, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<half, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<half, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<half, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<float, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<float, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<float, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<float, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<float, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<float, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<float, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<float, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<float, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<float, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<float, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<float, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<float, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<float, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<double, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<double, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<double, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<double, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<double, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<double, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<double, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<double, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<double, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<double, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<double, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<double, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<double, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<double, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<bool, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<bool, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<bool, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<bool, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<bool, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<bool, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<bool, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<bool, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<bool, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<bool, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<bool, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<bool, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<bool, Complex<float>>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeBool).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<bool, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, Complex<double>>},

  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint8_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint16_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint32_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint64_t>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, float>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, double>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, half>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, bool>},
  {KernelAttr().AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int8_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int8_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int8_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int8_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int8_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int8_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int8_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int8_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int16_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int16_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int16_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int16_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int16_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int16_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int16_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int16_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int32_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int32_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int32_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int32_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int32_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int32_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int32_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int32_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int64_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int64_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int64_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int64_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int64_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int64_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int64_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int64_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint8_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint8_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint16_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint16_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint32_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint32_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, float>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, double>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, half>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint64_t, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint64_t, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<half, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<half, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<half, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<half, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<half, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<half, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<half, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<half, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<half, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<half, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<half, half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<half, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<half, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<half, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<float, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<float, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<float, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<float, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<float, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<float, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<float, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<float, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<float, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<float, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<float, half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<float, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<float, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<float, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<double, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<double, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<double, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<double, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<double, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<double, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<double, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<double, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<double, float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<double, double>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<double, half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<double, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<double, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<double, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<bool, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<bool, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<bool, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<bool, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<bool, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<bool, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<bool, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<bool, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<bool, float>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<bool, double>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<bool, half>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<bool, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<bool, Complex<float>>},
  {KernelAttr().AddInputAttr(kNumberTypeBool).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<bool, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, float>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, double>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, half>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, Complex<double>>},

  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, float>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, double>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, half>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, bool>},
  {KernelAttr().AddInputAttr(kNumberTypeComplex128).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, Complex<float>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int8_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int8_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int8_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int8_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int8_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int8_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int8_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int8_t, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int8_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int8_t, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int8_t, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int16_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int16_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int16_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int16_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int16_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int16_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int16_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int16_t, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int16_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int16_t, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int16_t, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int32_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int32_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int32_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int32_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int32_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int32_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int32_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int32_t, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int32_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int32_t, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int32_t, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<int64_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<int64_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<int64_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<int64_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<int64_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<int64_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<int64_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<int64_t, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<int64_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<int64_t, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<int64_t, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint8_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint8_t, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint8_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint8_t, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt8)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint8_t, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint16_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint16_t, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint16_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint16_t, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint16_t, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint32_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint32_t, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint32_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint32_t, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint32_t, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<uint64_t, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<uint64_t, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<uint64_t, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<uint64_t, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeUInt64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<uint64_t, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<half, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<half, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<half, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<half, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<half, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<half, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<half, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<half, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<half, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<half, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<half, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<half, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<half, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat16)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<half, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<float, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<float, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<float, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<float, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<float, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<float, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<float, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<float, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<float, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<float, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<float, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<float, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<float, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<float, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<double, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<double, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<double, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<double, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<double, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<double, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<double, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<double, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<double, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<double, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<double, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<double, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<double, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<double, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<bool, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<bool, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<bool, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<bool, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<bool, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<bool, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<bool, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<bool, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<bool, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<bool, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<bool, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<bool, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<bool, Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeBool)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<bool, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex128),
   &CastGpuKernelMod::LaunchKernel<Complex<float>, Complex<double>>},

  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, int64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt8),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint8_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint16_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint32_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeUInt64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, uint64_t>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat32),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, float>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, double>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeFloat16),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, half>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeBool),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, bool>},
  {KernelAttr()
     .AddInputAttr(kObjectTypeNumber, kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeInt64)
     .AddOutputAttr(kNumberTypeComplex64),
   &CastGpuKernelMod::LaunchKernel<Complex<double>, Complex<float>>}};

std::vector<KernelAttr> CastGpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list;
  if (support_list.empty()) {
    (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                         [](const std::pair<KernelAttr, CastFunc> &pair) { return pair.first; });
  }
  return support_list;
}
MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, Cast, CastGpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
