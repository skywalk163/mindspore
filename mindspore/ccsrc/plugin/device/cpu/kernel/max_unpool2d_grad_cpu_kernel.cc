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
#include "plugin/device/cpu/kernel/max_unpool2d_grad_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/grad/max_unpool2d_grad.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kMaxUnpool2DGradInputsNum = 3;
constexpr size_t kMaxUnpool2DGradOutputsNum = 1;
constexpr size_t kInputIndex0 = 0;
constexpr size_t kInputIndex1 = 1;
constexpr size_t kInputIndex2 = 2;
constexpr size_t kInputIndex3 = 3;
}  // namespace

bool MaxUnpool2DGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                       const std::vector<KernelTensor *> &outputs) {
  data_format_ = GetValue<std::string>(primitive_->GetAttr(ops::kFormat));

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MaxUnpool2DGradFunc> &pair) { return pair.first; });
  auto [is_match, index] = MatchKernelAttr(kernel_attr, support_list);
  if (!is_match) {
    MS_LOG(ERROR) << "MaxUnpool2DGrad does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int MaxUnpool2DGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                        const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  input_shape_ = inputs[kIndex0]->GetDeviceShapeVector();
  grads_shape_ = inputs[kIndex1]->GetDeviceShapeVector();
  indices_shape_ = inputs[kIndex2]->GetDeviceShapeVector();
  output_shape_ = outputs[kIndex0]->GetDeviceShapeVector();
  return KRET_OK;
}

template <typename DATA_T>
void MaxUnpool2DGradCpuKernelMod::OutPutInitKernel(DATA_T *raw_output, size_t length) {
  for (size_t s = 0; s < length; s++) {
    raw_output[s] = (DATA_T)0;
  }
}

template <typename DATA_T, typename INDICES_T>
bool MaxUnpool2DGradCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                               const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMaxUnpool2DGradInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMaxUnpool2DGradOutputsNum, kernel_name_);

  if (outputs[kInputIndex0]->size() == 0) {
    MS_LOG(WARNING) << "MaxUnpool2DGrad output memory size should be greater than 0, but got 0.";
    return false;
  }
  auto *raw_grads = static_cast<DATA_T *>(inputs[kInputIndex1]->device_ptr());
  auto *raw_indices = static_cast<INDICES_T *>(inputs[kInputIndex2]->device_ptr());
  auto *raw_output = static_cast<DATA_T *>(outputs[kInputIndex0]->device_ptr());
  if (data_format_ == "NHWC") {
    size_t num_batch = LongToSize(grads_shape_[kInputIndex0]);
    size_t oheight = LongToSize(grads_shape_[kInputIndex1]);
    size_t owidth = LongToSize(grads_shape_[kInputIndex2]);
    size_t num_channels = LongToSize(grads_shape_[kInputIndex3]);
    size_t iheight = LongToSize(output_shape_[kInputIndex1]);
    size_t iwidth = LongToSize(output_shape_[kInputIndex2]);
    size_t length = num_batch * iheight * iwidth * num_channels;
    OutPutInitKernel<DATA_T>(raw_output, length);
    for (size_t n = 0; n < num_batch; n++) {
      size_t noutput_offset = n * num_channels * iwidth * iheight;
      size_t n_grads_offset = n * num_channels * owidth * oheight;
      DATA_T *output_p_k = raw_output + noutput_offset;
      DATA_T *grads_p_k = raw_grads + n_grads_offset;
      INDICES_T *ind_p_k = raw_indices + noutput_offset;
      size_t maxp;
      size_t ind_p_k_id;
      for (size_t k = 0; k < num_channels; k++) {
        for (size_t i = 0; i < iheight; i++) {
          for (size_t j = 0; j < iwidth; j++) {
            ind_p_k_id = i * iwidth * num_channels + j * num_channels + k;
            maxp = static_cast<size_t>(ind_p_k[ind_p_k_id]);
            if (ind_p_k[ind_p_k_id] < 0 || maxp >= owidth * oheight) {
              MS_LOG(EXCEPTION) << "MaxUnpool2DGrad: internal error, output_size H * W should "
                                   "be bigger than some indicis, now H * W is "
                                << owidth * oheight << " and value of argmax is " << maxp << "." << std::endl;
            } else {
              output_p_k[ind_p_k_id] = grads_p_k[maxp * num_channels + k];
            }
          }
        }
      }
    }
  } else {
    size_t num_batch = LongToSize(grads_shape_[kInputIndex0]);
    size_t oheight = LongToSize(grads_shape_[kInputIndex2]);
    size_t owidth = LongToSize(grads_shape_[kInputIndex3]);
    size_t num_channels = LongToSize(grads_shape_[kInputIndex1]);
    size_t iheight = LongToSize(output_shape_[kInputIndex2]);
    size_t iwidth = LongToSize(output_shape_[kInputIndex3]);
    size_t length = num_batch * iheight * iwidth * num_channels;
    OutPutInitKernel<DATA_T>(raw_output, length);
    for (size_t n = 0; n < num_batch; n++) {
      size_t noutput_offset = n * num_channels * iwidth * iheight;
      size_t n_grads_offset = n * num_channels * owidth * oheight;
      size_t k = 0;
      for (k = 0; k < num_channels; k++) {
        size_t final_output_offset = noutput_offset + k * iwidth * iheight;
        size_t final_grads_offset = n_grads_offset + k * owidth * oheight;
        DATA_T *output_p_k = raw_output + final_output_offset;
        DATA_T *grads_p_k = raw_grads + final_grads_offset;
        INDICES_T *ind_p_k = raw_indices + final_output_offset;
        size_t maxp;
        size_t ind_p_k_id;
        for (size_t i = 0; i < iheight; i++) {
          for (size_t j = 0; j < iwidth; j++) {
            ind_p_k_id = i * iwidth + j;
            maxp = static_cast<size_t>(ind_p_k[ind_p_k_id]);
            if (ind_p_k[ind_p_k_id] < 0 || maxp >= owidth * oheight) {
              MS_LOG(EXCEPTION) << "MaxUnpool2DGrad: internal error, output_size H * W should "
                                   "be bigger than some indicis, now H * W is "
                                << owidth * oheight << " and value of argmax is " << maxp << "." << std::endl;
            } else {
              output_p_k[ind_p_k_id] = grads_p_k[maxp];
            }
          }
        }
      }
    }
  }
  return true;
}

std::vector<std::pair<KernelAttr, MaxUnpool2DGradCpuKernelMod::MaxUnpool2DGradFunc>>
  MaxUnpool2DGradCpuKernelMod::func_list_ = {{KernelAttr()
                                                .AddInputAttr(kNumberTypeUInt8)
                                                .AddInputAttr(kNumberTypeUInt8)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeUInt8),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<uint8_t, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeUInt8)
                                                .AddInputAttr(kNumberTypeUInt8)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeUInt8),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<uint8_t, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeUInt16)
                                                .AddInputAttr(kNumberTypeUInt16)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeUInt16),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<uint16_t, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeUInt16)
                                                .AddInputAttr(kNumberTypeUInt16)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeUInt16),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<uint16_t, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeUInt32)
                                                .AddInputAttr(kNumberTypeUInt32)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeUInt32),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<uint32_t, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeUInt32)
                                                .AddInputAttr(kNumberTypeUInt32)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeUInt32),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<uint32_t, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeUInt64)
                                                .AddInputAttr(kNumberTypeUInt64)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeUInt64),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<uint64_t, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeUInt64)
                                                .AddInputAttr(kNumberTypeUInt64)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeUInt64),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<uint64_t, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeInt8)
                                                .AddInputAttr(kNumberTypeInt8)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeInt8),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<int8_t, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeInt8)
                                                .AddInputAttr(kNumberTypeInt8)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeInt8),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<int8_t, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeInt16)
                                                .AddInputAttr(kNumberTypeInt16)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeInt16),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<int16_t, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeInt16)
                                                .AddInputAttr(kNumberTypeInt16)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeInt16),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<int16_t, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeInt32),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<int32_t, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeInt32),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<int32_t, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeInt64),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<int64_t, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeInt64),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<int64_t, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeFloat16)
                                                .AddInputAttr(kNumberTypeFloat16)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeFloat16),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<float16, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeFloat16)
                                                .AddInputAttr(kNumberTypeFloat16)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeFloat16),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<float16, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeFloat32)
                                                .AddInputAttr(kNumberTypeFloat32)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeFloat32),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<float, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeFloat32)
                                                .AddInputAttr(kNumberTypeFloat32)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeFloat32),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<float, int64_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeFloat64)
                                                .AddInputAttr(kNumberTypeFloat64)
                                                .AddInputAttr(kNumberTypeInt32)
                                                .AddOutputAttr(kNumberTypeFloat64),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<double, int32_t>},
                                             {KernelAttr()
                                                .AddInputAttr(kNumberTypeFloat64)
                                                .AddInputAttr(kNumberTypeFloat64)
                                                .AddInputAttr(kNumberTypeInt64)
                                                .AddOutputAttr(kNumberTypeFloat64),
                                              &MaxUnpool2DGradCpuKernelMod::LaunchKernel<double, int64_t>}};

std::vector<KernelAttr> MaxUnpool2DGradCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MaxUnpool2DGradFunc> &pair) { return pair.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, MaxUnpool2DGrad, MaxUnpool2DGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
