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

#include "plugin/device/cpu/kernel/gcd_cpu_kernel.h"

#include <vector>
#include <algorithm>
#include <utility>
#include <numeric>

#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "plugin/device/cpu/kernel/cpu_kernel.h"

namespace mindspore {
namespace kernel {
namespace {
const size_t kGcdInputsNum = 2;
const size_t kGcdOutputsNum = 1;
}  // namespace

bool GcdCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kGcdInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kGcdOutputsNum, kernel_name_);
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << kernel_name_ << " does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int GcdCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  x1_shape_ = inputs[kIndex0]->GetShapeVector();
  x2_shape_ = inputs[kIndex1]->GetShapeVector();
  y_shape_ = outputs[kIndex0]->GetShapeVector();
  return KRET_OK;
}

template <typename T>
bool GcdCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                   const std::vector<kernel::KernelTensor *> &outputs) {
  const T *x1 = static_cast<const T *>(inputs[kIndex0]->device_ptr());
  MS_EXCEPTION_IF_NULL(x1);
  const T *x2 = static_cast<const T *>(inputs[kIndex1]->device_ptr());
  MS_EXCEPTION_IF_NULL(x2);
  T *y = static_cast<T *>(outputs[kIndex0]->device_ptr());
  MS_EXCEPTION_IF_NULL(y);
  if (y_shape_.size() == 0) {
    (void)y_shape_.insert(y_shape_.begin(), 1);
  }
  int64_t output_size_ = 1;
  for (size_t i = 0; i < y_shape_.size(); ++i) {
    output_size_ *= y_shape_[i];
  }
  BroadcastIterator base_iter(x1_shape_, x2_shape_, y_shape_);
  auto task = [this, &x1, &x2, &y, &base_iter](size_t start, size_t end) {
    auto iter = base_iter;
    iter.SetPos(start);
    for (size_t i = start; i < end; i++) {
      y[i] = std::gcd(x1[iter.GetInputPosA()], x2[iter.GetInputPosB()]);
      iter.GenNextPos();
    }
  };

  ParallelLaunchAutoSearch(task, LongToSize(output_size_), this, &parallel_search_info_);
  return true;
}

std::vector<std::pair<KernelAttr, GcdCpuKernelMod::GcdLaunchFunc>> GcdCpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &GcdCpuKernelMod::LaunchKernel<int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &GcdCpuKernelMod::LaunchKernel<int64_t>}};

std::vector<KernelAttr> GcdCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, GcdLaunchFunc> &pair) { return pair.first; });

  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Gcd, GcdCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
