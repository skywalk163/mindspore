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
 * once_compute_thread_sizetributed under the License is
 * once_compute_thread_sizetributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

#include "plugin/device/cpu/kernel/max_pool3d_with_argmax_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/max_pool3d_with_argmax.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kMaxPool3DWithArgmaxInputNum = 1;
constexpr size_t kMaxPool3DWithArgmaxOutputsNum = 2;
const int64_t kZero = 0;
const int64_t kOne = 1;
const int64_t kTwo = 2;
const int64_t kThree = 3;
const int64_t kFour = 4;
const size_t DIM_SIZE_1 = 1;
const size_t DIM_SIZE_3 = 3;
const size_t DIM_SIZE_5 = 5;
}  // namespace

bool MaxPool3DWithArgmaxCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &outputs) {
  x_dtype_ = inputs[kZero]->dtype_id();
  argmax_dtype_ = outputs[kOne]->dtype_id();

  MS_EXCEPTION_IF_CHECK_FAIL(primitive_->HasAttr(ops::kKSize), "no attribute ksize");
  ksize_list_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kKSize));
  MS_EXCEPTION_IF_CHECK_FAIL(primitive_->HasAttr(ops::kStrides), "no attribute strides");
  strides_list_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kStrides));
  MS_EXCEPTION_IF_CHECK_FAIL(primitive_->HasAttr(ops::kPads), "no attribute pads");
  pads_list_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kPads));
  MS_EXCEPTION_IF_CHECK_FAIL(primitive_->HasAttr(ops::kDilation), "no attribute dilation");
  dilation_list_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kDilation));

  if (ksize_list_.size() != DIM_SIZE_1 && ksize_list_.size() != DIM_SIZE_3) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', the ksize size must be 1 or 3, but got " << ksize_list_;
  }
  if (strides_list_.size() != DIM_SIZE_1 && strides_list_.size() != DIM_SIZE_3) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', the strides size must be 1 or 3, but got "
                             << strides_list_;
  }
  if (pads_list_.size() != DIM_SIZE_1 && pads_list_.size() != DIM_SIZE_3) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', the strides size must be 1 or 3, but got " << pads_list_;
  }
  if (dilation_list_.size() != DIM_SIZE_1 && dilation_list_.size() != DIM_SIZE_3 &&
      dilation_list_.size() != DIM_SIZE_5) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', the strides size must be 1 or 3 or 5, but got "
                             << dilation_list_;
  }

  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int MaxPool3DWithArgmaxCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }

  x_shape_ = inputs[kZero]->GetDeviceShapeVector();
  y_shape_ = outputs[kZero]->GetDeviceShapeVector();
  argmax_shape_ = outputs[kOne]->GetDeviceShapeVector();
  return KRET_OK;
}

template <typename DATA_T, typename INDICES_T>
void MaxPool3DWithArgmaxCpuKernelMod::MaxPool3DWithArgmaxSingleCompute(
  DATA_T *input, DATA_T *output_y, INDICES_T *output_argmax, int64_t iD, int64_t iH, int64_t iW, int64_t oD, int64_t oH,
  int64_t oW, int64_t kD, int64_t kH, int64_t kW, int64_t sD, int64_t sH, int64_t sW, int64_t pD, int64_t pH,
  int64_t pW, int64_t dD, int64_t dH, int64_t dW) const {
  int64_t i;
  int64_t j;
  int64_t ti;
  DATA_T *ip = input;
  for (ti = 0; ti < oD; ti++) {
    for (i = 0; i < oH; i++) {
      for (j = 0; j < oW; j++) {
        int64_t start_t = ti * sD - pD;
        int64_t start_h = i * sH - pH;
        int64_t start_w = j * sW - pW;

        int64_t end_t = std::min(start_t + (kD - kOne) * dD + kOne, iD);
        int64_t end_h = std::min(start_h + (kH - kOne) * dH + kOne, iH);
        int64_t end_w = std::min(start_w + (kW - kOne) * dW + kOne, iW);

        while (start_t < 0) {
          start_t += dD;
        }
        while (start_h < 0) {
          start_h += dH;
        }
        while (start_w < 0) {
          start_w += dW;
        }

        DATA_T *op = output_y + ti * oW * oH + i * oW + j;
        INDICES_T *indzp = output_argmax + ti * oW * oH + i * oW + j;

        INDICES_T maxindex = start_t * iH * iW + start_h * iW + start_w;
        DATA_T maxval = -std::numeric_limits<DATA_T>::infinity();

        for (int64_t z = start_t; z < end_t; z += dD) {
          for (int64_t y = start_h; y < end_h; y += dH) {
            for (int64_t x = start_w; x < end_w; x += dW) {
              INDICES_T index = z * iH * iW + y * iW + x;
              DATA_T val = ip[index];
              if ((val > maxval) || std::isnan(static_cast<double>(val))) {
                maxval = (DATA_T)val;
                maxindex = index;
              }
            }
          }
        }

        // store location of max
        *indzp = maxindex;

        /* set output to local max */
        *op = maxval;
      }
    }
  }
}

template <typename DATA_T>
bool MaxPool3DWithArgmaxCpuKernelMod::CheckIfLessOne(const std::vector<DATA_T> &inputs) const {
  const int64_t ksize = static_cast<int64_t>(inputs[kZero]);
  const int64_t strides = static_cast<int64_t>(inputs[kOne]);
  const int64_t dilation = static_cast<int64_t>(inputs[kTwo]);
  if (ksize < kOne || strides < kOne || dilation < kOne) {
    MS_EXCEPTION(ValueError)
      << "for MaxPool3DWithArgmax, ksize, strides or dilation should be no less than one, but get ksize " << ksize
      << " , strides " << strides << ", dilation " << dilation << ".";
  } else {
    return true;
  }
}

template <typename DATA_T>
bool MaxPool3DWithArgmaxCpuKernelMod::CheckIfLessZero(const std::vector<DATA_T> &inputs) const {
  const int64_t width = static_cast<int64_t>(inputs[kZero]);
  const int64_t height = static_cast<int64_t>(inputs[kOne]);
  const int64_t depth = static_cast<int64_t>(inputs[kTwo]);
  if (width < kZero || height < kZero || depth < kZero) {
    MS_EXCEPTION(ValueError) << "for MaxPool3DWithArgmax, pads should be no less than zero, but get pads [" << width
                             << ", " << height << ", " << depth << "].";
  } else {
    return true;
  }
}

void MaxPool3DWithArgmaxCpuKernelMod::CheckPadsValue(size_t k_width, size_t p_width, size_t k_height, size_t p_height,
                                                     size_t k_depth, size_t p_depth) const {
  if (k_width / kTwo < p_width || k_height / kTwo < p_height || k_depth / kTwo < p_depth) {
    MS_EXCEPTION(ValueError) << "for " << kernel_name_
                             << ", pads should be smaller than or equal to half of kernel size, but the pads is ["
                             << p_depth << ", " << p_height << ", " << p_width << "], the kernel size is [" << k_depth
                             << ", " << k_height << ", " << k_width << "].";
  }
}

template <typename DATA_T, typename INDICES_T>
bool MaxPool3DWithArgmaxCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                                   const std::vector<KernelTensor *> &,
                                                   const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kMaxPool3DWithArgmaxInputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kMaxPool3DWithArgmaxOutputsNum, kernel_name_);
  auto input_x = static_cast<DATA_T *>(inputs[kZero]->device_ptr());
  auto output_y = static_cast<DATA_T *>(outputs[kZero]->device_ptr());
  auto output_argmax = static_cast<INDICES_T *>(outputs[kOne]->device_ptr());
  auto input_shape_vec = x_shape_;
  auto output_shape_vec = y_shape_;
  const int64_t in_width = input_shape_vec[kFour];
  const int64_t in_height = input_shape_vec[kThree];
  const int64_t in_depth = input_shape_vec[kTwo];
  const int64_t in_channel = input_shape_vec[kOne];
  const int64_t in_batch = input_shape_vec[kZero];
  const int64_t out_width = output_shape_vec[kFour];
  const int64_t out_height = output_shape_vec[kThree];
  const int64_t out_depth = output_shape_vec[kTwo];
  const int64_t batch = in_batch * in_channel;
  const int64_t in_stride = in_width * in_height * in_depth;
  const int64_t out_stride = out_width * out_height * out_depth;
  std::vector<int64_t> ksize_temp_list;
  if (ksize_list_.size() == DIM_SIZE_1) {
    ksize_temp_list.push_back(ksize_list_[kZero]);
    ksize_temp_list.push_back(ksize_list_[kZero]);
    ksize_temp_list.push_back(ksize_list_[kZero]);
  } else {
    ksize_temp_list.push_back(ksize_list_[kZero]);
    ksize_temp_list.push_back(ksize_list_[kOne]);
    ksize_temp_list.push_back(ksize_list_[kTwo]);
  }
  std::vector<int64_t> strides_temp_list;
  if (strides_list_.size() == DIM_SIZE_1) {
    strides_temp_list.push_back(strides_list_[kZero]);
    strides_temp_list.push_back(strides_list_[kZero]);
    strides_temp_list.push_back(strides_list_[kZero]);
  } else {
    strides_temp_list.push_back(strides_list_[kZero]);
    strides_temp_list.push_back(strides_list_[kOne]);
    strides_temp_list.push_back(strides_list_[kTwo]);
  }
  std::vector<int64_t> pads_temp_list;
  if (pads_list_.size() == DIM_SIZE_1) {
    pads_temp_list.push_back(pads_list_[kZero]);
    pads_temp_list.push_back(pads_list_[kZero]);
    pads_temp_list.push_back(pads_list_[kZero]);
  } else {
    pads_temp_list.push_back(pads_list_[kZero]);
    pads_temp_list.push_back(pads_list_[kOne]);
    pads_temp_list.push_back(pads_list_[kTwo]);
  }
  std::vector<int64_t> dilation_temp_list;
  if (dilation_list_.size() == DIM_SIZE_1) {
    dilation_temp_list.push_back(dilation_list_[kZero]);
    dilation_temp_list.push_back(dilation_list_[kZero]);
    dilation_temp_list.push_back(dilation_list_[kZero]);
  } else if (dilation_list_.size() == DIM_SIZE_5) {
    dilation_temp_list.push_back(dilation_list_[kTwo]);
    dilation_temp_list.push_back(dilation_list_[kThree]);
    dilation_temp_list.push_back(dilation_list_[kFour]);
  } else {
    dilation_temp_list.push_back(dilation_list_[kZero]);
    dilation_temp_list.push_back(dilation_list_[kOne]);
    dilation_temp_list.push_back(dilation_list_[kTwo]);
  }
  const int64_t k_width = ksize_temp_list[kTwo];
  const int64_t k_height = ksize_temp_list[kOne];
  const int64_t k_depth = ksize_temp_list[kZero];
  const int64_t s_width = strides_temp_list[kTwo];
  const int64_t s_height = strides_temp_list[kOne];
  const int64_t s_depth = strides_temp_list[kZero];
  const int64_t p_width = pads_temp_list[kTwo];
  const int64_t p_height = pads_temp_list[kOne];
  const int64_t p_depth = pads_temp_list[kZero];
  const int64_t d_width = dilation_temp_list[kTwo];
  const int64_t d_height = dilation_temp_list[kOne];
  const int64_t d_depth = dilation_temp_list[kZero];
  (void)CheckIfLessOne(strides_temp_list);
  (void)CheckIfLessOne(dilation_temp_list);
  (void)CheckIfLessOne(ksize_temp_list);
  (void)CheckIfLessZero(pads_temp_list);
  CheckPadsValue(k_width, p_width, k_height, p_height, k_depth, p_depth);
  // attributes limitations
  for (int64_t i = 0; i < batch; i++) {
    MaxPool3DWithArgmaxSingleCompute(input_x + i * in_stride, output_y + i * out_stride, output_argmax + i * out_stride,
                                     in_depth, in_height, in_width, out_depth, out_height, out_width, k_depth, k_height,
                                     k_width, s_depth, s_height, s_width, p_depth, p_height, p_width, d_depth, d_height,
                                     d_width);
  }
  return true;
}

#define ADD_KERNEL(x_dtype, shape_dtype, x_type, shape_type)             \
  {                                                                      \
    KernelAttr()                                                         \
      .AddInputAttr(kNumberType##x_dtype)                                \
      .AddOutputAttr(kNumberType##x_dtype)                               \
      .AddOutputAttr(kNumberType##shape_dtype),                          \
      &MaxPool3DWithArgmaxCpuKernelMod::LaunchKernel<x_type, shape_type> \
  }

std::vector<std::pair<KernelAttr, MaxPool3DWithArgmaxCpuKernelMod::MaxPool3DWithArgmaxFunc>>
  MaxPool3DWithArgmaxCpuKernelMod::func_list_ = {
    ADD_KERNEL(Float16, Int32, float16, int32_t), ADD_KERNEL(Float32, Int32, float, int32_t),
    ADD_KERNEL(Float64, Int32, double, int32_t),  ADD_KERNEL(Int8, Int32, int8_t, int32_t),
    ADD_KERNEL(Int16, Int32, int16_t, int32_t),   ADD_KERNEL(Int32, Int32, int32_t, int32_t),
    ADD_KERNEL(Int64, Int32, int64_t, int32_t),   ADD_KERNEL(UInt8, Int32, uint8_t, int32_t),
    ADD_KERNEL(UInt16, Int32, uint16_t, int32_t), ADD_KERNEL(UInt32, Int32, uint32_t, int32_t),
    ADD_KERNEL(UInt64, Int32, uint64_t, int32_t), ADD_KERNEL(Float16, Int64, float16, int64_t),
    ADD_KERNEL(Float32, Int32, float, int64_t),   ADD_KERNEL(Float64, Int64, double, int64_t),
    ADD_KERNEL(Int8, Int64, int8_t, int64_t),     ADD_KERNEL(Int16, Int64, int16_t, int64_t),
    ADD_KERNEL(Int32, Int64, int32_t, int64_t),   ADD_KERNEL(Int64, Int64, int64_t, int64_t),
    ADD_KERNEL(UInt8, Int64, uint8_t, int64_t),   ADD_KERNEL(UInt16, Int64, uint16_t, int64_t),
    ADD_KERNEL(UInt32, Int64, uint32_t, int64_t), ADD_KERNEL(UInt64, Int64, uint64_t, int64_t)};

std::vector<KernelAttr> MaxPool3DWithArgmaxCpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
                       [](const std::pair<KernelAttr, MaxPool3DWithArgmaxFunc> &item) { return item.first; });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, MaxPool3DWithArgmax, MaxPool3DWithArgmaxCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
