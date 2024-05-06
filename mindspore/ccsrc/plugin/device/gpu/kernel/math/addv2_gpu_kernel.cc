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

#include "plugin/device/gpu/kernel/math/addv2_gpu_kernel.h"
#include <utility>
#include <functional>
#include <string>
#include <algorithm>
#include <memory>
#include "abstract/utils.h"

namespace mindspore {
namespace kernel {

constexpr size_t INPUT_NUM = 2;
constexpr size_t OUTPUT_NUM = 1;

bool AddV2GpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), INPUT_NUM, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), OUTPUT_NUM, kernel_name_);

  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it got empty inputs or outputs, which is invalid.";
    return false;
  }
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the kernel type should be in [unit8, uint16, uint32, uint64, int8, "
                  << "int16, int32, int64, float16, float32, float64, complex64, complex128], but got: " << kernel_attr
                  << ".";
    return false;
  }
  kernel_func_ = func_list_[index].second;
  MS_EXCEPTION_IF_NULL(inputs[kIndex0]);
  unit_size_ = abstract::TypeIdSize(inputs[kIndex0]->dtype_id());

  std::vector<size_t> lhs_shape_ = LongVecToSizeVec(inputs.at(kIndex0)->GetShapeVector());
  std::vector<size_t> rhs_shape_ = LongVecToSizeVec(inputs.at(kIndex1)->GetShapeVector());
  std::vector<size_t> output_shape_ = LongVecToSizeVec(outputs.at(kIndex0)->GetShapeVector());
  is_null_input_ = CHECK_SHAPE_NULL(lhs_shape_, kernel_name_, "input_0") ||
                   CHECK_SHAPE_NULL(rhs_shape_, kernel_name_, "input_1") ||
                   CHECK_SHAPE_NULL(output_shape_, kernel_name_, "output_0");
  return true;
}

int AddV2GpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  if (inputs.size() != INPUTS_SIZE) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "' input size must be equal 2.";
    return KRET_RESIZE_FAILED;
  }

  std::vector<size_t> shape1;
  auto shape_1 = inputs.at(kIndex0)->GetShapeVector();
  (void)std::transform(shape_1.begin(), shape_1.end(), std::back_inserter(shape1), LongToSize);
  std::vector<size_t> shape2;
  auto shape_2 = inputs.at(kIndex1)->GetShapeVector();
  (void)std::transform(shape_2.begin(), shape_2.end(), std::back_inserter(shape2), LongToSize);
  auto shape3 = shape1.size() > shape2.size() ? shape1 : shape2;
  need_broadcast_ = common::AnfAlgo::IsTensorBroadcast(shape1, shape2);
  if (need_broadcast_ && shape3.size() > MAX_DIMS) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the dimension of input cannot be greater than " << MAX_DIMS
                      << ", but got " << shape3.size();
  }

  input1_shape_.resize(MAX_DIMS, 1);
  input2_shape_.resize(MAX_DIMS, 1);
  output_shape_.resize(MAX_DIMS, 1);

  for (size_t i = 0; i < shape3.size(); i++) {
    if (need_broadcast_) {
      if (i < MAX_DIMS) {
        output_shape_[i] = shape3[i];
      } else {
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the index of output should be less than " << MAX_DIMS
                          << ", but got " << i;
      }
    }
    output_num_ *= shape3[i];
  }
  int lhs_offset = shape3.size() - shape1.size();
  for (size_t j = 0; j < shape1.size(); j++) {
    if (need_broadcast_) {
      if ((j + lhs_offset) >= MIN_DIMS && (j + lhs_offset) < MAX_DIMS) {
        input1_shape_[j + lhs_offset] = shape1[j];
      } else {
        auto index = j + lhs_offset;
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the index of input cannot be " << index << ", but got "
                          << index;
      }
    }
  }
  int rhs_offset = shape3.size() - shape2.size();
  for (size_t k = 0; k < shape2.size(); k++) {
    if (need_broadcast_) {
      if ((k + rhs_offset) >= MIN_DIMS && (k + rhs_offset) < MAX_DIMS) {
        input2_shape_[k + rhs_offset] = shape2[k];
      } else {
        auto index = k + rhs_offset;
        MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', the index of input cannot be " << index << ", but got "
                          << index;
      }
    }
  }

  input_elements_ = inputs[0]->size() / unit_size_;
  return KRET_OK;
}

template <typename T>
bool AddV2GpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &workspace,
                                     const std::vector<KernelTensor *> &outputs) {
  T *input_addr1 = GetDeviceAddress<T>(inputs, 0);
  T *input_addr2 = GetDeviceAddress<T>(inputs, 1);
  T *output_addr = GetDeviceAddress<T>(outputs, 0);
  if (need_broadcast_) {
    auto status = CalAddV2(input_elements_, input1_shape_, input2_shape_, output_shape_, input_addr1, input_addr2,
                           output_addr, device_id_, reinterpret_cast<cudaStream_t>(stream_ptr_));
    CHECK_CUDA_STATUS(status, kernel_name_);
  } else {
    auto status =
      ElewiseAddV2(output_num_, input_addr1, input_addr2, output_addr, reinterpret_cast<cudaStream_t>(stream_ptr_));
    CHECK_CUDA_STATUS(status, kernel_name_);
  }
  return true;
}

template <typename T>
using Complex = mindspore::utils::Complex<T>;
std::vector<std::pair<KernelAttr, AddV2GpuKernelMod::AddV2Func>> AddV2GpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeUInt8).AddOutputAttr(kNumberTypeUInt8),
   &AddV2GpuKernelMod::LaunchKernel<uint8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeUInt16).AddOutputAttr(kNumberTypeUInt16),
   &AddV2GpuKernelMod::LaunchKernel<uint16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeUInt32).AddOutputAttr(kNumberTypeUInt32),
   &AddV2GpuKernelMod::LaunchKernel<uint32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeUInt64).AddOutputAttr(kNumberTypeUInt64),
   &AddV2GpuKernelMod::LaunchKernel<uint64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8),
   &AddV2GpuKernelMod::LaunchKernel<int8_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16),
   &AddV2GpuKernelMod::LaunchKernel<int16_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &AddV2GpuKernelMod::LaunchKernel<int>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &AddV2GpuKernelMod::LaunchKernel<int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
   &AddV2GpuKernelMod::LaunchKernel<half>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
   &AddV2GpuKernelMod::LaunchKernel<float>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64),
   &AddV2GpuKernelMod::LaunchKernel<double>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeComplex64)
     .AddInputAttr(kNumberTypeComplex64)
     .AddOutputAttr(kNumberTypeComplex64),
   &AddV2GpuKernelMod::LaunchKernel<Complex<float>>},
  {KernelAttr()
     .AddInputAttr(kNumberTypeComplex128)
     .AddInputAttr(kNumberTypeComplex128)
     .AddOutputAttr(kNumberTypeComplex128),
   &AddV2GpuKernelMod::LaunchKernel<Complex<double>>}};

std::vector<KernelAttr> AddV2GpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, AddV2Func> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeGpuKernelMod, AddV2, AddV2GpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
