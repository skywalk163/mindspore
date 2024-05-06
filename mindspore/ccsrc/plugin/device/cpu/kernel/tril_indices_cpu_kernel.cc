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

#include "plugin/device/cpu/kernel/tril_indices_cpu_kernel.h"
#include <algorithm>
#include <utility>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
bool TrilIndicesCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  row_ = GetValue<int64_t>(primitive_->GetAttr("row"));
  col_ = GetValue<int64_t>(primitive_->GetAttr("col"));
  offset_ = GetValue<int64_t>(primitive_->GetAttr("offset"));
  if (row_ < 0) {
    MS_EXCEPTION(ValueError) << "For TrilIndices, row is " << row_ << ", but row should be greater than or equal to 0.";
  }
  if (col_ < 0) {
    MS_EXCEPTION(ValueError) << "For TrilIndices, col is " << col_ << ", but col should be greater than or equal to 0.";
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(EXCEPTION) << "TrilIndices does not support this kernel data type: " << kernel_attr;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int TrilIndicesCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  return KernelMod::Resize(inputs, outputs);
}

template <typename T>
bool TrilIndicesCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &,
                                           const std::vector<kernel::KernelTensor *> &,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  auto m_first_row = offset_ > 0 ? std::min<int64_t>(col_, 1 + offset_) : row_ + offset_ > 0;
  auto m_last_row = std::max<int64_t>(0, std::min<int64_t>(col_, row_ + offset_));
  auto n_row_all = std::max<int64_t>(0, std::min<int64_t>(row_, row_ + offset_));
  auto n_row_trapezoid = (m_last_row - m_first_row + 1);
  auto tril_size = (static_cast<size_t>((m_first_row + m_last_row) * n_row_trapezoid)) >> 1;
  auto diff_row = n_row_all - n_row_trapezoid;
  if (diff_row > 0) {
    tril_size += diff_row * col_;
  }
  auto *output_addr = static_cast<T *>(outputs[kIndex0]->device_ptr());
  MS_EXCEPTION_IF_NULL(output_addr);
  int64_t i = 0;
  int64_t r = std::max<int64_t>(0, -offset_);
  int64_t c = 0;
  while (i < SizeToLong(tril_size)) {
    output_addr[i] = r;
    output_addr[tril_size + i++] = c;
    c += 1;
    if (c > r + offset_ || c >= col_) {
      r += 1;
      c = 0;
    }
  }
  return true;
}

std::vector<std::pair<KernelAttr, TrilIndicesCpuKernelMod::TrilIndicesFunc>> TrilIndicesCpuKernelMod::func_list_ = {
  {KernelAttr().AddOutputAttr(kNumberTypeInt32), &TrilIndicesCpuKernelMod::LaunchKernel<int32_t>},
  {KernelAttr().AddOutputAttr(kNumberTypeInt64), &TrilIndicesCpuKernelMod::LaunchKernel<int64_t>}};

std::vector<KernelAttr> TrilIndicesCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, TrilIndicesFunc> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, TrilIndices, TrilIndicesCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
