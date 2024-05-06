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

#include "plugin/device/cpu/kernel/sub_and_filter_cpu_kernel.h"
#include <string>
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kSubAndFilterInputsNum = 3;
constexpr size_t kSubAndFilterOutputNum = 2;
}  // namespace

bool SubAndFilterCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  x_dtype_ = inputs.at(kIndex0)->dtype_id();
  x_dtype_size_ = abstract::TypeIdSize(x_dtype_);
  return true;
}

int SubAndFilterCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  for (auto &input : inputs) {
    MS_EXCEPTION_IF_NULL(input);
    auto shape = input->GetShapeVector();
    if (!IsValidShape(shape)) {
      return KRET_UNKNOWN_SHAPE;
    }
  }
  ResetResource();
  auto input_x_shape = inputs.at(kIndex0)->GetShapeVector();
  batch_size_ = SizeOf(input_x_shape);
  MS_LOG(INFO) << "SubAndFilter batch_size:" << batch_size_;
  (void)output_size_list_.emplace_back(batch_size_ * x_dtype_size_);
  (void)output_size_list_.emplace_back(batch_size_ * x_dtype_size_);
  return KRET_OK;
}

bool SubAndFilterCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                      const std::vector<kernel::KernelTensor *> &,
                                      const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kSubAndFilterInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kSubAndFilterOutputNum, kernel_name_);
  if (x_dtype_ == kNumberTypeInt32) {
    LaunchKernel<int>(inputs, outputs);
  } else if (x_dtype_ == kNumberTypeInt64) {
    LaunchKernel<int64_t>(inputs, outputs);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dtype of input must be int32 or int64, but got "
                      << TypeIdToType(x_dtype_)->ToString();
  }
  return true;
}

template <typename T>
void SubAndFilterCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<kernel::KernelTensor *> &outputs) {
  T *input_x = reinterpret_cast<T *>(inputs[0]->device_ptr());
  T max_num = *reinterpret_cast<T *>(inputs[1]->device_ptr());
  T offset = *reinterpret_cast<T *>(inputs[2]->device_ptr());
  T *filter_res = reinterpret_cast<T *>(outputs[0]->device_ptr());
  T *filter_idx = reinterpret_cast<T *>(outputs[1]->device_ptr());

  int64_t count = 0;
  for (size_t i = 0; i < batch_size_; ++i) {
    T temp = input_x[i] - offset;
    if (temp < 0 || temp >= max_num) {
      continue;
    }
    filter_res[count] = temp;
    filter_idx[count] = static_cast<T>(i);
    count++;
  }
  MS_LOG(INFO) << "SubAndFilter output count is " << count;
  out_size_ = count;
}

void SubAndFilterCpuKernelMod::UpdateOutputShapeAndSize(const std::vector<KernelTensor *> &inputs,
                                                        const std::vector<KernelTensor *> &outputs) {
  ShapeVector out_shape = {out_size_};
  outputs[0]->SetShapeVector(out_shape);
  outputs[0]->set_size(LongToSize(out_size_) * UnitSizeInBytes(x_dtype_));
  outputs[1]->SetShapeVector(out_shape);
  outputs[1]->set_size(LongToSize(out_size_) * UnitSizeInBytes(x_dtype_));
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, SubAndFilter, SubAndFilterCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
