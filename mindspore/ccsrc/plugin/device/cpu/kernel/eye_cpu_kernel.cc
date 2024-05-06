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

#include "plugin/device/cpu/kernel/eye_cpu_kernel.h"
#include <algorithm>
#include <memory>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kEyeInputsNum = 3;
constexpr size_t kEyeOutputsNum = 1;
}  // namespace
bool EyeCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int EyeCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  int ret = 0;
  if ((ret = NativeCpuKernelMod::Resize(inputs, outputs)) != 0) {
    MS_LOG(WARNING) << kernel_name_ << " reinit failed.";
    return ret;
  }
  return 0;
}

template <typename S, typename T>
bool EyeCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &,
                                   const std::vector<kernel::KernelTensor *> &outputs) {
  size_t data_size = outputs[0]->size();
  S tmp_n = static_cast<S *>(inputs[0]->device_ptr())[0];
  S tmp_m = static_cast<S *>(inputs[1]->device_ptr())[0];
  num_n_ = static_cast<int64_t>(tmp_n);
  num_m_ = static_cast<int64_t>(tmp_m);

  int64_t num_min = (num_n_ > num_m_) ? num_m_ : num_n_;
  auto ouput_ptr = outputs[0]->device_ptr();
  auto ret = memset_s(ouput_ptr, data_size, 0, data_size);
  if (ret != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', memset_s failed, ret=" << ret;
  }
  auto *output_addr = reinterpret_cast<T *>(outputs[0]->device_ptr());
  T num = static_cast<T>(1);
  for (int64_t i = 0; i < num_min; i++) {
    *(output_addr + (num_m_ + 1) * i) = static_cast<T>(num);
  }
  return true;
}

// In Kernel, the type of mstype is kNumberTypeInt64;
#define EYE_CPU_REG(MS_T, MS_S, S, T)                                                                    \
  KernelAttr().AddInputAttr(MS_T).AddInputAttr(MS_T).AddInputAttr(kNumberTypeInt64).AddOutputAttr(MS_S), \
    &EyeCpuKernelMod::LaunchKernel<S, T>

const std::vector<std::pair<KernelAttr, EyeCpuKernelMod::KernelRunFunc>> &EyeCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, EyeCpuKernelMod::KernelRunFunc>> func_list = {
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeFloat16, int32_t, float16)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeFloat16, int64_t, float16)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeFloat32, int32_t, float)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeFloat32, int64_t, float)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeFloat64, int32_t, double)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeFloat64, int64_t, double)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeInt8, int32_t, int8_t)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeInt8, int64_t, int8_t)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeInt16, int32_t, int16_t)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeInt16, int64_t, int16_t)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeInt32, int32_t, int32_t)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeInt32, int64_t, int32_t)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeInt64, int32_t, int64_t)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeInt64, int64_t, int64_t)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeUInt8, int32_t, uint8_t)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeUInt8, int64_t, uint8_t)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeUInt16, int32_t, uint16_t)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeUInt16, int64_t, uint16_t)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeUInt32, int32_t, uint32_t)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeUInt32, int64_t, uint32_t)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeUInt64, int32_t, uint64_t)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeUInt64, int64_t, uint64_t)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeComplex64, int32_t, std::complex<float>)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeComplex64, int64_t, std::complex<float>)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeComplex128, int32_t, std::complex<double>)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeComplex128, int64_t, std::complex<double>)},
    {EYE_CPU_REG(kNumberTypeInt32, kNumberTypeBool, int32_t, bool)},
    {EYE_CPU_REG(kNumberTypeInt64, kNumberTypeBool, int64_t, bool)}};
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Eye, EyeCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
