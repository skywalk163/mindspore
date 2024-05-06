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

#include "plugin/device/cpu/kernel/left_shift_cpu_kernel.h"
#include <vector>
#include <memory>
#include "plugin/device/cpu/kernel/cpu_kernel.h"

namespace mindspore {
namespace kernel {
namespace {
const size_t kLeftShiftInputsNum = 2;
const size_t kLeftShiftOutputsNum = 1;
}  // namespace

bool LeftShiftCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kLeftShiftInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kLeftShiftOutputsNum, kernel_name_);
  input_type_1_ = inputs[0]->dtype_id();
  input_type_2_ = outputs[0]->dtype_id();
  if (input_type_1_ != input_type_2_) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the type of 'x2' must be the same as the type of 'x1', "
                         "but got "
                      << TypeIdLabel(input_type_2_);
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto match = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!match.first) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  return true;
}

int LeftShiftCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }
  input_shape_1_ = inputs[0]->GetShapeVector();
  input_shape_2_ = inputs[1]->GetShapeVector();
  output_shape_ = outputs[0]->GetShapeVector();
  return ret;
}

bool LeftShiftCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> & /* workspace */,
                                   const std::vector<KernelTensor *> &outputs) {
  if (input_type_1_ == kNumberTypeInt8) {
    return IntCompute<int8_t>(inputs, outputs);
  } else if (input_type_1_ == kNumberTypeInt16) {
    return IntCompute<int16_t>(inputs, outputs);
  } else if (input_type_1_ == kNumberTypeInt32) {
    return IntCompute<int32_t>(inputs, outputs);
  } else if (input_type_1_ == kNumberTypeInt64) {
    return IntCompute<int64_t>(inputs, outputs);
  } else if (input_type_1_ == kNumberTypeUInt8) {
    return UIntCompute<uint8_t>(inputs, outputs);
  } else if (input_type_1_ == kNumberTypeUInt16) {
    return UIntCompute<uint16_t>(inputs, outputs);
  } else if (input_type_1_ == kNumberTypeUInt32) {
    return UIntCompute<uint32_t>(inputs, outputs);
  } else if (input_type_1_ == kNumberTypeUInt64) {
    return UIntCompute<uint64_t>(inputs, outputs);
  } else {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_
                      << "', the type of 'x1' should be int8, int16, int32, int64, uint8, uint16, uint32, uint64, "
                         "but got "
                      << TypeIdLabel(input_type_1_);
  }
}

template <typename T>
bool LeftShiftCpuKernelMod::IntCompute(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  auto *input1 = static_cast<T *>(inputs[0]->device_ptr());
  const auto *input2 = static_cast<T *>(inputs[1]->device_ptr());
  auto *output = static_cast<T *>(outputs[0]->device_ptr());
  if (output_shape_.size() == 0) {
    (void)output_shape_.insert(output_shape_.begin(), 1);
  }
  int64_t size_tmp = 1;
  for (size_t i = 0; i < output_shape_.size(); ++i) {
    size_tmp *= output_shape_[i];
  }
  size_t output_size = LongToSize(size_tmp);
  BroadcastIterator base_iter(input_shape_1_, input_shape_2_, output_shape_);
  auto task = [&input1, &input2, &output, &base_iter](size_t start, size_t end) {
    auto iter = base_iter;
    iter.SetPos(start);
    for (size_t i = start; i < end; i++) {
      T y_val = (input2[iter.GetInputPosB()]);
      T bit_val = static_cast<T>(sizeof(T) * 8) > 32 ? static_cast<T>(sizeof(T) * 8) : 32;
      if (abs(y_val) > bit_val) {
        y_val = y_val % bit_val;
      }
      output[i] = static_cast<T>(input1[iter.GetInputPosA()] << y_val);
      iter.GenNextPos();
    }
  };
  ParallelLaunchAutoSearch(task, output_size, this, &parallel_search_info_);
  return true;
}

template <typename T>
bool LeftShiftCpuKernelMod::UIntCompute(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  auto *input1 = static_cast<T *>(inputs[0]->device_ptr());
  const auto *input2 = static_cast<T *>(inputs[1]->device_ptr());
  auto *output = static_cast<T *>(outputs[0]->device_ptr());
  if (output_shape_.size() == 0) {
    (void)output_shape_.insert(output_shape_.begin(), 1);
  }
  int64_t size_tmp = 1;
  for (size_t i = 0; i < output_shape_.size(); ++i) {
    size_tmp *= output_shape_[i];
  }
  size_t output_size = LongToSize(size_tmp);
  BroadcastIterator base_iter(input_shape_1_, input_shape_2_, output_shape_);
  auto task = [&input1, &input2, &output, &base_iter](size_t start, size_t end) {
    auto iter = base_iter;
    iter.SetPos(start);
    for (size_t i = start; i < end; i++) {
      T y_val = (input2[iter.GetInputPosB()]);
      T bit_val = static_cast<T>(sizeof(T) * 8) > 32 ? static_cast<T>(sizeof(T) * 8) : 32;
      if (y_val > bit_val || y_val < -(bit_val)) {
        y_val = y_val % bit_val;
      }
      output[i] = static_cast<T>(input1[iter.GetInputPosA()] << y_val);
      iter.GenNextPos();
    }
  };
  ParallelLaunchAutoSearch(task, output_size, this, &parallel_search_info_);
  return true;
}

std::vector<KernelAttr> LeftShiftCpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list = {
    KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
    KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
    KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
    KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
    KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
    KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
    KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
    KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64)};
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, LeftShift, LeftShiftCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
