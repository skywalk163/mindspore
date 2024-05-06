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

#include "plugin/device/cpu/kernel/topk_cpu_kernel.h"
#include <algorithm>
#include <map>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "include/common/thread_pool.h"
#include "ops/topk.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kTopKInputsNum = 2;
constexpr size_t kTopKOutputsNum = 2;
}  // namespace

template <typename T>
void TopKCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &workspaces,
                                    const std::vector<KernelTensor *> &outputs) const {
  if (inputs.size() != 2 || outputs.size() != 2) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the operator must have 2 inputs and 2 outputs, but got "
                      << inputs.size() << "input(s) and " << outputs.size() << "output(s)";
  }
  if (inputs[0]->size() != outer_size_ * inner_size_ * sizeof(T)) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', address size of 'input_x' error.";
  }
  if (inputs[1]->size() != sizeof(int)) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the 'k' must be int, but got " << inputs[1];
  }
  auto input = reinterpret_cast<T *>(inputs[0]->device_ptr());
  int k = reinterpret_cast<int *>(inputs[1]->device_ptr())[0];
  size_t *workspace = GetDeviceAddress<size_t>(workspaces, 0);
  auto output = reinterpret_cast<T *>(outputs[0]->device_ptr());
  auto indices = reinterpret_cast<int *>(outputs[1]->device_ptr());
  if (k < 1) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the 'k' must be greater than 0, but got " << k;
  }
  size_t k_num = IntToSize(std::min<int>(inner_size_, k));
  if (outputs[0]->size() != outer_size_ * k_num * sizeof(T)) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', address size of output error.";
  }

  const std::function<bool(size_t, size_t)> comparator = [input](size_t index_1, size_t index_2) {
    return input[index_1] > input[index_2];
  };
  std::vector<common::Task> tasks;
  tasks.reserve(outer_size_);
  for (size_t i = 0; i < outer_size_; ++i) {
    (void)tasks.emplace_back([this, i, k_num, &comparator, input, workspace, indices, output]() {
      size_t *idx = workspace + i * inner_size_;
      auto base_input = i * inner_size_;
      std::iota(idx, idx + inner_size_, base_input);

      if (sorted_) {
        constexpr float fraction = 0.5;
        const size_t threshold = FloatToSize(inner_size_ * fraction);
        // fall back to stable_sort
        if (k_num > threshold) {
          std::stable_sort(idx, idx + inner_size_, comparator);
        } else {
          std::nth_element(idx, idx + SizeToLong(k_num), idx + inner_size_, comparator);
          std::stable_sort(idx, idx + SizeToLong(k_num), comparator);
        }
      } else {
        std::nth_element(idx, idx + SizeToLong(k_num), idx + inner_size_, comparator);
      }

      auto base_output = i * k_num;
      for (size_t j = 0; j < k_num; ++j) {
        indices[base_output + j] = SizeToInt(idx[j]) - SizeToInt(base_input);
        output[base_output + j] = input[idx[j]];
      }
      return common::SUCCESS;
    });
  }
  ParallelLaunch(tasks);
}

bool TopKCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  return true;
}

int TopKCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  auto x_shape_ = Convert2SizeTClipNeg(inputs[0]->GetShapeVector());
  if (x_shape_.empty()) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the dimension of input must be greater than 0, but got empty input.";
  }
  outer_size_ = 1;
  for (size_t i = 0; i < x_shape_.size() - 1; ++i) {
    outer_size_ *= x_shape_[i];
  }
  inner_size_ = x_shape_[x_shape_.size() - 1];

  sorted_ = GetValue<bool>(primitive_->GetAttr(ops::kSorted));
  dtype_ = inputs[0]->dtype_id();

  size_t element_size = outer_size_ * inner_size_;
  (void)workspace_size_list_.emplace_back((sizeof(size_t) * element_size));

  return KRET_OK;
}

bool TopKCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                              const std::vector<kernel::KernelTensor *> &workspaces,
                              const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kTopKInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kTopKOutputsNum, kernel_name_);
  if (dtype_ == kNumberTypeFloat16) {
    LaunchKernel<float16>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeFloat32) {
    LaunchKernel<float>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeInt32) {
    LaunchKernel<int>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeUInt32) {
    LaunchKernel<uint32_t>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeInt8) {
    LaunchKernel<int8_t>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeUInt8) {
    LaunchKernel<uint8_t>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeInt16) {
    LaunchKernel<int16_t>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeUInt16) {
    LaunchKernel<uint16_t>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeInt64) {
    LaunchKernel<int64_t>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeUInt64) {
    LaunchKernel<uint64_t>(inputs, workspaces, outputs);
  } else if (dtype_ == kNumberTypeFloat64) {
    LaunchKernel<double>(inputs, workspaces, outputs);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dtype of input must be float, int or uint, but got "
                      << TypeIdToType(dtype_)->ToString();
  }
  return true;
}

std::vector<KernelAttr> TopKCpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> kernel_attr_list = {KernelAttr()
                                                       .AddInputAttr(kNumberTypeFloat16)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeFloat16)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeFloat32)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeFloat32)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeFloat64)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeFloat64)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeInt8)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeInt8)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeUInt8)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeUInt8)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeInt16)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeInt16)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeUInt16)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeUInt16)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeUInt32)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeUInt32)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeInt64)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeInt64)
                                                       .AddOutputAttr(kNumberTypeInt32),
                                                     KernelAttr()
                                                       .AddInputAttr(kNumberTypeUInt64)
                                                       .AddInputAttr(kNumberTypeInt32)
                                                       .AddOutputAttr(kNumberTypeUInt64)
                                                       .AddOutputAttr(kNumberTypeInt32)};
  return kernel_attr_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, TopK, TopKCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
