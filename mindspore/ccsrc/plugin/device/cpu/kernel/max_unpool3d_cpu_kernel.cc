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
#include <cmath>
#include <map>
#include <utility>
#include <algorithm>
#include "plugin/device/cpu/kernel/max_unpool3d_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/max_unpool3d.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kMaxUnpool3DInputsNum = 2;
constexpr size_t kMaxUnpool3DOutputsNum = 1;
constexpr size_t kInputIndex0 = 0;
constexpr size_t kInputIndex1 = 1;
constexpr size_t kInputIndex2 = 2;
constexpr size_t kInputIndex3 = 3;
constexpr size_t kInputIndex4 = 4;
}  // namespace

bool MaxUnpool3DCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  data_format_ = GetValue<std::string>(primitive_->GetAttr(ops::kFormat));

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MaxUnpool3DFunc> &pair) { return pair.first; });
  auto [is_match, index] = MatchKernelAttr(kernel_attr, support_list);
  if (!is_match) {
    MS_LOG(ERROR) << "MaxUnpool3D does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int MaxUnpool3DCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  constexpr size_t output_index = 0;
  constexpr size_t input_index = 0;
  constexpr size_t indices_index = 1;
  output_shape_ = outputs[output_index]->GetDeviceShapeVector();
  input_shape_ = inputs[input_index]->GetDeviceShapeVector();
  indices_shape_ = inputs[indices_index]->GetDeviceShapeVector();
  return KRET_OK;
}

template <typename DATA_T>
void MaxUnpool3DCpuKernelMod::OutPutInitKernel(DATA_T *raw_output, size_t length) {
  for (size_t s = 0; s < length; s++) {
    raw_output[s] = (DATA_T)0;
  }
}

template <typename DATA_T, typename INDICES_T>
bool MaxUnpool3DCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMaxUnpool3DInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMaxUnpool3DOutputsNum, kernel_name_);
  if (outputs[kInputIndex0]->size() == 0) {
    MS_LOG(WARNING) << "MaxUnpool3D output memory size should be greater than 0, but got 0.";
    return false;
  }
  auto *raw_input = static_cast<DATA_T *>(inputs[kInputIndex0]->device_ptr());
  auto *raw_indices = static_cast<INDICES_T *>(inputs[kInputIndex1]->device_ptr());
  auto *raw_output = static_cast<DATA_T *>(outputs[kInputIndex0]->device_ptr());
  size_t num_batch = static_cast<size_t>(input_shape_[kInputIndex0]);
  if (data_format_ == "NDHWC") {
    size_t input_depth = LongToSize(input_shape_[kInputIndex1]);
    size_t input_height = LongToSize(input_shape_[kInputIndex2]);
    size_t input_width = LongToSize(input_shape_[kInputIndex3]);
    size_t num_channels = LongToSize(input_shape_[kInputIndex4]);
    size_t odepth = LongToSize(output_shape_[kInputIndex1]);
    size_t oheight = LongToSize(output_shape_[kInputIndex2]);
    size_t owidth = LongToSize(output_shape_[kInputIndex3]);
    size_t length = num_batch * odepth * oheight * owidth * num_channels;
    OutPutInitKernel<DATA_T>(raw_output, length);
    for (size_t n = 0; n < num_batch; n++) {
      size_t noutput_offset = n * num_channels * odepth * owidth * oheight;
      size_t n_input_offset = n * num_channels * input_depth * input_width * input_height;
      DATA_T *output_p_k = raw_output + noutput_offset;
      DATA_T *input_p_k = raw_input + n_input_offset;
      INDICES_T *ind_p_k = raw_indices + n_input_offset;
      INDICES_T maxp;
      size_t ind_p_k_id;
      for (size_t k = 0; k < num_channels; k++) {
        for (size_t t = 0; t < input_depth; t++) {
          for (size_t i = 0; i < input_height; i++) {
            for (size_t j = 0; j < input_width; j++) {
              ind_p_k_id =
                t * input_height * input_width * num_channels + i * input_width * num_channels + j * num_channels + k;
              maxp = ind_p_k[ind_p_k_id];
              if (ind_p_k[ind_p_k_id] < 0 || maxp >= static_cast<INDICES_T>(odepth * owidth * oheight)) {
                MS_LOG(EXCEPTION) << "MaxUnpool3D: internal error, output_size D * H * W should "
                                     "be bigger than some indicis, now D * H * W is "
                                  << odepth * owidth * oheight << " and value of argmax is " << maxp << "."
                                  << std::endl;
              } else {
                output_p_k[static_cast<size_t>(maxp) * num_channels + k] = input_p_k[ind_p_k_id];
              }
            }
          }
        }
      }
    }
  } else {
    size_t input_depth = LongToSize(input_shape_[kInputIndex2]);
    size_t input_height = LongToSize(input_shape_[kInputIndex3]);
    size_t input_width = LongToSize(input_shape_[kInputIndex4]);
    size_t num_channels = LongToSize(input_shape_[kInputIndex1]);
    size_t odepth = LongToSize(output_shape_[kInputIndex2]);
    size_t oheight = LongToSize(output_shape_[kInputIndex3]);
    size_t owidth = LongToSize(output_shape_[kInputIndex4]);
    size_t length = num_batch * odepth * oheight * owidth * num_channels;
    OutPutInitKernel<DATA_T>(raw_output, length);
    for (size_t n = 0; n < num_batch; n++) {
      size_t noutput_offset = n * num_channels * odepth * owidth * oheight;
      size_t n_input_offset = n * num_channels * input_depth * input_width * input_height;
      size_t k = 0;
      for (k = 0; k < num_channels; k++) {
        size_t final_output_offset = noutput_offset + k * odepth * owidth * oheight;
        size_t final_input_offset = n_input_offset + k * input_depth * input_width * input_height;
        DATA_T *output_p_k = raw_output + final_output_offset;
        DATA_T *input_p_k = raw_input + final_input_offset;
        INDICES_T *ind_p_k = raw_indices + final_input_offset;
        INDICES_T maxp;
        size_t ind_p_k_id;
        for (size_t t = 0; t < input_depth; t++) {
          for (size_t i = 0; i < input_height; i++) {
            for (size_t j = 0; j < input_width; j++) {
              ind_p_k_id = t * input_height * input_width + i * input_width + j;
              maxp = static_cast<INDICES_T>(ind_p_k[ind_p_k_id]);
              if (ind_p_k[ind_p_k_id] < 0 || maxp >= static_cast<INDICES_T>(odepth * owidth * oheight)) {
                MS_LOG(EXCEPTION) << "MaxUnpool3D: internal error, output_size D * H * W should "
                                     "be bigger than some indicis, now D * H * W is "
                                  << odepth * owidth * oheight << " and value of argmax is " << maxp << "."
                                  << std::endl;
              } else {
                output_p_k[maxp] = input_p_k[ind_p_k_id];
              }
            }
          }
        }
      }
    }
  }
  return true;
}

std::vector<std::pair<KernelAttr, MaxUnpool3DCpuKernelMod::MaxUnpool3DFunc>> MaxUnpool3DCpuKernelMod::func_list_ = {
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt8),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<uint8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<uint8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt16),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<uint16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<uint16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt32),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<uint32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<uint32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt64),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<uint64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<uint64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt8),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<int8_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<int8_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt16),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<int16_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<int16_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<int32_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<int32_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt64),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<int64_t, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<int64_t, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat16),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<float16, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<float16, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<float, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<float, int64_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat64),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<double, int32_t>},
  {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
   &MaxUnpool3DCpuKernelMod::LaunchKernel<double, int64_t>}};

std::vector<KernelAttr> MaxUnpool3DCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MaxUnpool3DFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, MaxUnpool3D, MaxUnpool3DCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
