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
#include "plugin/device/cpu/kernel/dilation2d_backprop_filter_cpu_kernel.h"
#include <limits>

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kDimSize3 = 3;
constexpr size_t kDimSize4 = 4;
constexpr size_t kInputNum = 3;
constexpr size_t kOutputNum = 1;
constexpr size_t kInputIndexf = 0;
constexpr size_t kFilterIndexf = 1;
constexpr size_t kBackpropIndexf = 2;
constexpr size_t kOutputIndexf = 0;
constexpr size_t kFormatNCHWIndexN = 0;
constexpr size_t kFormatNCHWIndexC = 1;
constexpr size_t kFormatNCHWIndexH = 2;
constexpr size_t kFormatNCHWIndexW = 3;
constexpr size_t kFormatCHWIndexH = 1;
constexpr size_t kFormatCHWIndexW = 2;
constexpr int64_t kValue2 = 2;
}  // namespace

bool Dilation2DBackpropFilterCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &outputs) {
  stride_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kStride));
  dilation_ = GetValue<std::vector<int64_t>>(primitive_->GetAttr(ops::kDilation));
  pad_mode_ = GetValue<int64_t>(primitive_->GetAttr(ops::kPadMode));
  format_ = GetValue<std::string>(primitive_->GetAttr(ops::kFormat));
  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }
  return true;
}

bool Dilation2DBackpropFilterCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                                  const std::vector<KernelTensor *> &workspace,
                                                  const std::vector<KernelTensor *> &outputs) {
  return kernel_func_(this, inputs, workspace, outputs);
}

int Dilation2DBackpropFilterCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                                 const std::vector<KernelTensor *> &outputs) {
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret != 0) {
    return ret;
  }
  f_input_shape_ = inputs[kInputIndexf]->GetShapeVector();
  f_filter_shape_ = inputs[kFilterIndexf]->GetShapeVector();
  f_out_backprop_shape_ = inputs[kBackpropIndexf]->GetShapeVector();
  f_output_shape_ = outputs[kOutputIndexf]->GetShapeVector();
  CheckKernelParam();
  return KRET_OK;
}

template <typename T>
bool Dilation2DBackpropFilterCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                        const std::vector<KernelTensor *> &,
                                                        const std::vector<kernel::KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kInputNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kOutputNum, kernel_name_);
  T *input = static_cast<T *>(inputs[kInputIndexf]->device_ptr());
  T *filter = static_cast<T *>(inputs[kFilterIndexf]->device_ptr());
  T *out_backprop = static_cast<T *>(inputs[kBackpropIndexf]->device_ptr());
  T *output = static_cast<T *>(outputs[kOutputIndexf]->device_ptr());

  size_t num_batch = LongToSize(f_input_shape_[kFormatNCHWIndexN]);
  size_t input_height = LongToSize(f_input_shape_[kFormatNCHWIndexH]);
  size_t input_width = LongToSize(f_input_shape_[kFormatNCHWIndexW]);
  size_t channel = LongToSize(f_input_shape_[kFormatNCHWIndexC]);
  size_t filter_height = LongToSize(f_filter_shape_[kFormatCHWIndexH]);
  size_t filter_width = LongToSize(f_filter_shape_[kFormatCHWIndexW]);
  size_t out_backprop_height = LongToSize(f_out_backprop_shape_[kFormatNCHWIndexH]);
  size_t out_backprop_width = LongToSize(f_out_backprop_shape_[kFormatNCHWIndexW]);
  size_t output_height = LongToSize(f_output_shape_[kFormatCHWIndexH]);
  size_t output_width = LongToSize(f_output_shape_[kFormatCHWIndexW]);
  size_t stride_height = LongToSize(stride_[kFormatNCHWIndexH]);
  size_t stride_width = LongToSize(stride_[kFormatNCHWIndexW]);
  size_t rate_height = LongToSize(dilation_[kFormatNCHWIndexH]);
  size_t rate_width = LongToSize(dilation_[kFormatNCHWIndexW]);
  int64_t pad_top;
  int64_t pad_left;

  if (pad_mode_.compare("VALID") == 0 || pad_mode_.compare("valid") == 0) {
    pad_top = 0;
    pad_left = 0;
  }
  if (pad_mode_.compare("SAME") == 0 || pad_mode_.compare("same") == 0) {
    int64_t pad_height =
      SizeToLong((out_backprop_height - 1) * stride_height + rate_height * (filter_height - 1) + 1 - input_height) > 0
        ? SizeToLong((out_backprop_height - 1) * stride_height + rate_height * (filter_height - 1) + 1 - input_height)
        : 0;
    int64_t pad_width =
      SizeToLong((out_backprop_width - 1) * stride_width + rate_width * (filter_width - 1) + 1 - input_width) > 0
        ? SizeToLong((out_backprop_width - 1) * stride_width + rate_width * (filter_width - 1) + 1 - input_width)
        : 0;
    pad_top = pad_height / kValue2;
    pad_left = pad_width / kValue2;
  }

  size_t outer_size = num_batch * channel * out_backprop_height * out_backprop_width;
  size_t out_bp_plane_size = out_backprop_height * out_backprop_width;
  size_t output_size = channel * output_height * output_width;

  for (size_t pos = 0; pos < output_size; ++pos) {
    output[pos] = static_cast<T>(0);
  }

  for (size_t pos = 0; pos < outer_size; ++pos) {
    size_t pos_n = pos / (out_bp_plane_size * channel);
    size_t pos_c = pos / (out_bp_plane_size) % channel;
    size_t pos_h = pos / out_backprop_width % out_backprop_height;
    size_t pos_w = pos % out_backprop_width;
    int64_t h_start = SizeToLong(pos_h * stride_height) - pad_top;
    int64_t w_start = SizeToLong(pos_w * stride_width) - pad_left;

    T max_val = std::numeric_limits<T>::lowest();
    size_t max_h = 0;
    size_t max_w = 0;
    for (size_t h = 0; h < filter_height; ++h) {
      int64_t h_in = h_start + SizeToLong(h * rate_height);
      if (h_in >= 0 && h_in < SizeToLong(input_height)) {
        for (size_t w = 0; w < filter_width; ++w) {
          int64_t w_in = w_start + SizeToLong(w * rate_width);
          if (w_in >= 0 && w_in < SizeToLong(input_width)) {
            T val =
              input[LongToSize(w_in) + input_width * (LongToSize(h_in) + input_height * (pos_c + channel * pos_n))] +
              filter[w + filter_width * (h + filter_height * pos_c)];
            if (val > max_val) {
              max_val = val;
              max_h = h;
              max_w = w;
            }
          }
        }
      }
    }
    output[max_w + filter_width * (max_h + filter_height * pos_c)] += out_backprop[pos];
  }
  return true;
}

const std::vector<std::pair<KernelAttr, Dilation2DBackpropFilterCpuKernelMod::KernelRunFunc>>
  &Dilation2DBackpropFilterCpuKernelMod::GetFuncList() const {
  static const std::vector<std::pair<KernelAttr, Dilation2DBackpropFilterCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kNumberTypeFloat16)
       .AddInputAttr(kNumberTypeFloat16)
       .AddOutputAttr(kNumberTypeFloat16),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<float16>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kNumberTypeFloat32)
       .AddOutputAttr(kNumberTypeFloat32),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<float>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kNumberTypeFloat64)
       .AddOutputAttr(kNumberTypeFloat64),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<double>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeInt8)
       .AddInputAttr(kNumberTypeInt8)
       .AddInputAttr(kNumberTypeInt8)
       .AddOutputAttr(kNumberTypeInt8),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<int8_t>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeInt16)
       .AddInputAttr(kNumberTypeInt16)
       .AddInputAttr(kNumberTypeInt16)
       .AddOutputAttr(kNumberTypeInt16),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<int16_t>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kNumberTypeInt32)
       .AddOutputAttr(kNumberTypeInt32),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<int32_t>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<int64_t>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeUInt8)
       .AddInputAttr(kNumberTypeUInt8)
       .AddInputAttr(kNumberTypeUInt8)
       .AddOutputAttr(kNumberTypeUInt8),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<uint8_t>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeUInt16)
       .AddInputAttr(kNumberTypeUInt16)
       .AddInputAttr(kNumberTypeUInt16)
       .AddOutputAttr(kNumberTypeUInt16),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<uint16_t>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeUInt32)
       .AddInputAttr(kNumberTypeUInt32)
       .AddInputAttr(kNumberTypeUInt32)
       .AddOutputAttr(kNumberTypeUInt32),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<uint32_t>},

    {KernelAttr()
       .AddInputAttr(kNumberTypeUInt64)
       .AddInputAttr(kNumberTypeUInt64)
       .AddInputAttr(kNumberTypeUInt64)
       .AddOutputAttr(kNumberTypeUInt64),
     &Dilation2DBackpropFilterCpuKernelMod::LaunchKernel<uint64_t>}};
  return func_list;
}

bool Dilation2DBackpropFilterCpuKernelMod::CheckKernelParam() {
  size_t input_shape_dims = f_input_shape_.size();
  size_t filter_shape_dims = f_filter_shape_.size();
  size_t out_backprop_shape_dims = f_out_backprop_shape_.size();
  size_t output_shape_dims = f_output_shape_.size();
  size_t stride_dims = stride_.size();
  size_t dilation_dims = dilation_.size();
  if (input_shape_dims != kDimSize4) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'input_shape' must be equal to 4, but got "
                  << input_shape_dims << ".";
    return false;
  }
  if (filter_shape_dims != kDimSize3) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'filter_shape' must be equal to 3, but got "
                  << filter_shape_dims << ".";
    return false;
  }
  if (out_backprop_shape_dims != kDimSize4) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'out_backprop_shape' must be equal to 4, but got "
                  << out_backprop_shape_dims << ".";
    return false;
  }
  if (output_shape_dims != kDimSize3) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'output_shape' must be equal to 3, but got "
                  << output_shape_dims << ".";
    return false;
  }
  if (stride_dims != kDimSize4) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'stride' must be equal to 4, but got "
                  << stride_dims << ".";
    return false;
  }
  if (dilation_dims != kDimSize4) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dimension of 'dilation' must be equal to 4, but got "
                  << dilation_dims << ".";
    return false;
  }
  if (pad_mode_ != "VALID" && pad_mode_ != "valid" && pad_mode_ != "SAME" && pad_mode_ != "same") {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', pad_mode_ must be VALID, valid, SAME or same, but got " << pad_mode_
                  << ".";
    return false;
  }
  if (format_ != "NCHW") {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', data_format must be NCHW, but got " << format_ << ".";
    return false;
  }
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Dilation2DBackpropFilter, Dilation2DBackpropFilterCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
