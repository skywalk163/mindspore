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

#include "plugin/device/cpu/kernel/unique_cpu_kernel.h"
#include <functional>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kBucketSortThreshold = 100000;
constexpr size_t kWorkSpaceNum = 3;
constexpr size_t kOutputNum = 2;
constexpr size_t kWorkSpaceIndex = 2;
}  // namespace

bool UniqueCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                const std::vector<kernel::KernelTensor *> &workspace,
                                const std::vector<kernel::KernelTensor *> &outputs) {
  if (dtype_ == kNumberTypeInt64) {
    LaunchKernel<int64_t, int64_t>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeInt8) {
    LaunchKernel<int8_t, int32_t>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeInt16) {
    LaunchKernel<int16_t, int32_t>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeInt32) {
    LaunchKernel<int32_t, int32_t>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeUInt8) {
    LaunchKernel<uint8_t, int32_t>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeUInt16) {
    LaunchKernel<uint16_t, int32_t>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeFloat16) {
    LaunchKernel<float16, int32_t>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeFloat32) {
    LaunchKernel<float, int32_t>(inputs, workspace, outputs);
  } else if (dtype_ == kNumberTypeFloat64) {
    LaunchKernel<double, int64_t>(inputs, workspace, outputs);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the dtype of input must be float16, 32, 64, (U)Int8, 16, Int 32 or Int 64, but got "
                      << TypeIdToType(dtype_)->ToString();
  }
  return true;
}

template <typename DataType, typename IndexType>
void UniqueCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                      const std::vector<KernelTensor *> &workspace,
                                      const std::vector<KernelTensor *> &outputs) {
  if (input_size_ == 0) {
    MS_LOG(WARNING) << "For '" << kernel_name_ << "', the input size is 0.";
    return;
  }
  if (inputs.size() < 1) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the number of inputs must be greater than 0, but got: " << inputs.size();
  }
  if (workspace.size() < kWorkSpaceNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of workspaces can not be less than " << kWorkSpaceNum
                      << ", but got: " << workspace.size();
  }
  if (outputs.size() < kOutputNum) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the number of outputs can not be less than " << kOutputNum
                      << ", but got: " << outputs.size();
  }
  auto params = std::make_shared<UniqueParam<DataType, IndexType>>();
  params->input_ = reinterpret_cast<DataType *>(inputs[0]->device_ptr());
  params->input_idx_ = reinterpret_cast<IndexType *>(workspace[0]->device_ptr());
  params->workspace_ = reinterpret_cast<DataType *>(workspace[1]->device_ptr());
  params->workspace_idx_ = reinterpret_cast<IndexType *>(workspace[kWorkSpaceIndex]->device_ptr());
  params->output_ = reinterpret_cast<DataType *>(outputs[0]->device_ptr());
  params->inverse_idx_ = reinterpret_cast<IndexType *>(outputs[1]->device_ptr());
  params->input_size_ = input_size_;
  params->output_size_ = 0;

  params->thread_num_ = common::ThreadPool::GetInstance().GetSyncRunThreadNum();
  output_sizes_.clear();
  for (size_t i = 0; i < batch_size_; i++) {
    if (sorted_) {
      params->need_sort_ = true;
      if (input_size_ < kBucketSortThreshold) {
        Unique(params);
      } else {
        BucketUnique(params);
      }
    } else {
      params->need_sort_ = false;
      Unique(params);
    }
    output_sizes_.push_back(static_cast<size_t>(params->output_size_));
    params->output_size_ = 0;
    params->input_ += input_size_;
    params->output_ += input_size_;
    params->inverse_idx_ += input_size_;
  }
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Unique, UniqueCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
