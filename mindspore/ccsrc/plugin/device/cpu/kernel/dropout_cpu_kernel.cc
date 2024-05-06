/**
 * Copyright 2020-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/dropout_cpu_kernel.h"

#include <algorithm>
#include <map>
#include <functional>
#include <utility>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/ops_func_impl/dropout.h"
#include "kernel/philox_random.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kDropoutInputsNum = 4;
constexpr size_t kDropoutOutputsNum = 2;
}  // namespace

using FuncVec = const std::vector<std::pair<KernelAttr, DropoutCpuKernelMod::KernelRunFunc>>;

bool DropoutCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (inputs.size() != kDropoutInputsNum || outputs.size() != kDropoutOutputsNum) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', input and output tensor number must be " << kDropoutInputsNum
                  << " and " << kDropoutOutputsNum << ", but got " << inputs.size() << " and " << outputs.size();
    return false;
  }
  keep_prob_ = inputs[kIndex1]->GetValueWithCheck<float>();
  if (keep_prob_ <= 0.0 || keep_prob_ > 1.0) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << ", the 'keep_prob' must be in (0.0, 1.0], but got " << keep_prob_;
  }
  uint64_t seed0 = static_cast<uint64_t>(inputs[kIndex2]->GetValueWithCheck<int64_t>());
  uint64_t seed1 = static_cast<uint64_t>(inputs[kIndex3]->GetValueWithCheck<int64_t>());
  uint64_t init_seed = random::GetSeed(seed0, seed1);
  rng_.seed(init_seed);
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int DropoutCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  tensor_size_ = 1;
  input_shape_ = inputs[0]->GetShapeVector();
  for (const auto &d : input_shape_) {
    tensor_size_ *= LongToSize(d);
  }
  return static_cast<int>(KRET_OK);
}

template <typename T>
bool DropoutCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &,
                                       const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kDropoutInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kDropoutOutputsNum, kernel_name_);

  const auto *input_addr = reinterpret_cast<T *>(inputs[0]->device_ptr());
  auto *output_addr = reinterpret_cast<T *>(outputs[0]->device_ptr());
  auto mask_addr = reinterpret_cast<T *>(outputs[1]->device_ptr());
  T scale = static_cast<T>(1.f / keep_prob_);
  std::uniform_real_distribution<float> uniform(0.f, 1.f);
  auto task = [input_addr, output_addr, mask_addr, scale, &uniform, this](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      mask_addr[i] = static_cast<T>(uniform(this->rng_) < keep_prob_);
      output_addr[i] = mask_addr[i] * input_addr[i] * scale;
    }
  };
  ParallelLaunchAutoSearch(task, tensor_size_, this, &parallel_search_info_);
  return true;
}

FuncVec &DropoutCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, DropoutCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat16)
       .AddOutputAttr(kNumberTypeFloat16),
     &DropoutCpuKernelMod::LaunchKernel<float16>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat32)
       .AddOutputAttr(kNumberTypeFloat32),
     &DropoutCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat64)
       .AddOutputAttr(kNumberTypeFloat64),
     &DropoutCpuKernelMod::LaunchKernel<double>},
  };
  return func_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Dropout, DropoutCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
