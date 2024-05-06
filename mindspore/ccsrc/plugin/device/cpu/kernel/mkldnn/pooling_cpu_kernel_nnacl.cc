/**
 * Copyright 2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/mkldnn/pooling_cpu_kernel_nnacl.h"
#include <string>
#include <algorithm>
#include <functional>
#include "plugin/device/cpu/kernel/utils/cpu_utils.h"
#include "mindspore/ccsrc/kernel/common_utils.h"
#include "mindspore/ccsrc/kernel/format_utils.h"
#include "nnacl/fp32/pack_fp32.h"
#include "nnacl/fp32/pooling_fp32.h"
#include "ops/conv_pool_op_name.h"

namespace mindspore {
namespace kernel {
constexpr size_t kDepthOffset = 2;
constexpr size_t kHeightIdx4D = 2;
constexpr int64_t kMinChannelBlock = 4;
constexpr size_t kPoolingInputsNum = 1;
constexpr size_t kAvgPoolInputsNum = 5;
constexpr size_t kPoolingOutputsNum = 1;
constexpr size_t kPadLen2D = 4;
constexpr size_t kPadLen3D = 6;
enum kAxisIdx : int { kD = 2, kH, kW };

void GetAxisPad(int64_t hw, int64_t kernel, int64_t stride, int64_t *pad_l, int64_t *pad_r) {
  MS_EXCEPTION_IF_ZERO("stride", stride);
  int64_t tail = hw % stride;
  auto pad = std::max((tail > 0 ? kernel - tail : kernel - stride), static_cast<int64_t>(0));
  *pad_l = std::floor(pad >> 1);
  *pad_r = pad - *pad_l;
}

int64_t ComputeStride(const std::vector<int64_t> &shape, size_t index) {
  int64_t result = 1;
  for (size_t i = index + 1; i < shape.size(); ++i) {
    result *= shape[i];
  }
  return result;
}

void PoolingCpuKernelNnaclMod::GetPadList(size_t src_dim, size_t padlist_len) {
  // pad_mode has been capitalized in front end.
  if (pad_mode_ == PadMode::VALID) {
    (void)pad_list_.insert(pad_list_.begin(), padlist_len, 0);
  } else if (pad_mode_ == PadMode::SAME) {
    for (size_t i = kHeightIdx4D; i < src_dim; i++) {
      int64_t pad_l, pad_r;
      GetAxisPad(in_size_[i], kernel_size_[i], stride_size_[i], &pad_l, &pad_r);
      (void)pad_list_.emplace_back(pad_l);
      (void)pad_list_.emplace_back(pad_r);
    }
  }
}

void PoolingCpuKernelNnaclMod::InitPooling3DParams() {
  // Access structure members in declaration order
  pooling_args_.pooling_compute_param_.input_w_ = in_size_[kW];
  pooling_args_.pooling_compute_param_.input_h_ = in_size_[kH];
  pooling_args_.pooling_compute_param_.input_batch_ = batches_;
  pooling_args_.pooling_compute_param_.input_channel_ = channels_;
  pooling_args_.pooling_compute_param_.output_w_ = out_size_[kW];
  pooling_args_.pooling_compute_param_.output_h_ = out_size_[kH];
  pooling_args_.input_d_ = in_size_[kD];
  pooling_args_.output_d_ = out_size_[kD];

  pooling_param_.pooling_parameter_.window_w_ = kernel_size_[kW];
  pooling_param_.pooling_parameter_.window_h_ = kernel_size_[kH];
  pooling_param_.pooling_parameter_.stride_w_ = stride_size_[kW];
  pooling_param_.pooling_parameter_.stride_h_ = stride_size_[kH];
  pooling_param_.pooling_parameter_.pad_u_ = padding_l_[kH - kDepthOffset];
  pooling_param_.pooling_parameter_.pad_d_ = padding_r_[kH - kDepthOffset];
  pooling_param_.pooling_parameter_.pad_l_ = padding_l_[kW - kDepthOffset];
  pooling_param_.pooling_parameter_.pad_r_ = padding_r_[kW - kDepthOffset];
  pooling_param_.window_d_ = kernel_size_[kD];
  pooling_param_.stride_d_ = stride_size_[kD];
  pooling_param_.pad_f_ = padding_l_[kD - kDepthOffset];
  pooling_param_.pad_b_ = padding_r_[kD - kDepthOffset];
  pooling_param_.count_include_pad_ = count_include_pad_;
  pooling_param_.divisor_override_ = divisor_override_;
}

bool PoolingCpuKernelNnaclMod::Init(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  if (kernel_name_ == kAvgPoolOpName) {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kAvgPoolInputsNum, kernel_name_);
  } else {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kPoolingInputsNum, kernel_name_);
  }
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kPoolingInputsNum, kernel_name_);
  kernel_name_ = primitive()->name();
  if (kernel_name_ == kAvgPool3DOpName || kernel_name_ == kAvgPoolOpName) {
    pool_mode_ = MEAN_POOLING;
  } else if (kernel_name_ == kMaxPool3DOpName || kernel_name_ == kMaxPoolOpName) {
    pool_mode_ = MAX_POOLING;
  } else {
    MS_LOG(ERROR) << "Pooling only supports Avg or Max, but got: " << kernel_name_ << ".";
    return false;
  }
  dtype_ = inputs[kIndex0]->dtype_id();
  if (kernel_name_ != kAvgPoolOpName) {
    kernel_size_ = GetValue<std::vector<int64_t>>(primitive()->GetAttr(KERNEL_SIZE));
    stride_size_ = GetValue<std::vector<int64_t>>(primitive()->GetAttr(STRIDES));
    pad_mode_ =
      static_cast<mindspore::PadMode>(ops::PadModeStringToInt(GetValue<std::string>(primitive()->GetAttr(PAD_MODE))));
    format_ = GetFormatFromStrToEnum(GetValue<std::string>(primitive()->GetAttr(FORMAT)));
  }

  if (primitive()->HasAttr(COUNT_INCLUDE_PAD)) {
    count_include_pad_ = GetValue<bool>(primitive()->GetAttr(COUNT_INCLUDE_PAD));
  }
  if (primitive()->HasAttr(DIVISOR_OVERRIDE)) {
    divisor_override_ = GetValue<int64_t>(primitive()->GetAttr(DIVISOR_OVERRIDE));
  }
  return true;
}

static bool inline check_kernel_stride(const std::vector<int64_t> &kernel_size, const std::vector<int64_t> &stride_size,
                                       const size_t rank) {
  // length check
  if (kernel_size.size() != rank) {
    MS_LOG(ERROR) << "The kernel_size length should be equal to " << rank << " but length of kernel_size is "
                  << kernel_size.size() << ".";
    return false;
  }
  if (stride_size.size() != rank) {
    MS_LOG(ERROR) << "The stride_size length should be equal to " << rank << " but length of stride_size is "
                  << stride_size.size() << ".";
    return false;
  }
  return true;
}

int PoolingCpuKernelNnaclMod::Resize(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  in_size_ = inputs[kIndex0]->GetDeviceShapeVector();
  out_size_ = outputs[kIndex0]->GetDeviceShapeVector();
  auto src_dim = in_size_.size();
  if (!(src_dim == SHAPE_4D && out_size_.size() == SHAPE_4D) &&
      !(src_dim == SHAPE_5D && out_size_.size() == SHAPE_5D)) {
    MS_LOG(ERROR) << "Pooling only supports 4D or 5D input/output, but got input " << src_dim << "D, "
                  << "output " << out_size_.size() << "D!";
    return KRET_RESIZE_FAILED;
  }

  // In dynamic shape + PyNative mode scenario, Resize() will be called twice, in addition,
  // kernel_size_/stride_size_/pad_list_ in 4D will be extended to 5D later, so here we need to reset
  // kernel_size_/stride_size_/pad_list_ before the extending.
  if (src_dim == SHAPE_4D) {
    std::vector<int64_t> kernel_size;
    std::vector<int64_t> stride_size;
    if (kernel_name_ == kAvgPoolOpName) {
      kernel_size = inputs[kIndex1]->GetValueWithCheck<std::vector<int64_t>>();
      stride_size = inputs[kIndex2]->GetValueWithCheck<std::vector<int64_t>>();
      pad_mode_ = static_cast<mindspore::PadMode>(inputs[kIndex3]->GetValueWithCheck<int64_t>());
      format_ = static_cast<mindspore::Format>(inputs[kIndex4]->GetValueWithCheck<int64_t>());
      if (format_ != Format::NCHW) {
        MS_LOG(ERROR) << "For '" << kernel_name_ << "', only 'NCHW' format is supported in CPU target, but got '"
                      << GetFormatFromEnumToStr(format_) << "' format.";
        return KRET_RESIZE_FAILED;
      }
    } else {
      kernel_size = GetValue<std::vector<int64_t>>(primitive()->GetAttr(KERNEL_SIZE));
      stride_size = GetValue<std::vector<int64_t>>(primitive()->GetAttr(STRIDES));
    }
    size_t NC_LEN = 0;
    if (kernel_name_ != kAvgPoolOpName) {
      NC_LEN += 2;
    }
    constexpr auto kernel_size_len = 2;
    if (kernel_size.size() != kernel_size_len + NC_LEN) {
      MS_LOG(INTERNAL_EXCEPTION) << "Unexpected kernel size length:" << kernel_size.size();
    }
    constexpr auto stride_size_len = 2;
    if (stride_size.size() != stride_size_len + NC_LEN) {
      MS_LOG(INTERNAL_EXCEPTION) << "Unexpected stride size length:" << stride_size.size();
    }
    // change kernel size and strides from (H, W) to (1, 1, H, W)
    if (kernel_name_ == kAvgPoolOpName) {
      kernel_size_ = {1, 1, kernel_size[kIndex0], kernel_size[kIndex1]};
      stride_size_ = {1, 1, stride_size[kIndex0], stride_size[kIndex1]};
    } else {
      kernel_size_ = kernel_size;
      stride_size_ = stride_size;
    }
    pad_list_.clear();
  } else {
    if (!check_kernel_stride(kernel_size_, stride_size_, src_dim)) {
      return KRET_RESIZE_FAILED;
    }
  }

  size_t padlist_len = src_dim == SHAPE_4D ? kPadLen2D : kPadLen3D;
  if (src_dim == SHAPE_4D) {
    GetPadList(src_dim, padlist_len);
  } else {
    // PAD_LIST has been calculated during infer shape, so it can be directly obtained here
    pad_list_ = GetValue<std::vector<int64_t>>(primitive()->GetAttr(PAD_LIST));
  }

  if (pad_list_.size() != padlist_len) {
    MS_LOG(EXCEPTION) << kernel_name_ << " requires length of pad_list must be " << padlist_len << ", but got "
                      << pad_list_.size() << "!";
  }

  // In order to reuse the 5D computing kernel, 4D is equivalently extended to 5D.
  if (src_dim == SHAPE_4D) {
    (void)in_size_.insert(in_size_.begin() + D_INDEX, 1);
    (void)out_size_.insert(out_size_.begin() + D_INDEX, 1);
    (void)kernel_size_.insert(kernel_size_.begin() + D_INDEX, 1);
    (void)stride_size_.insert(stride_size_.begin() + D_INDEX, 1);
    (void)pad_list_.insert(pad_list_.begin(), 2, 0);
    count_include_pad_ = false;  // must set false in Pooling2D
  }

  auto pad_size = pad_list_.size() >> 1;
  padding_l_.resize(pad_size);
  padding_r_.resize(pad_size);
  for (size_t i = 0; i < padding_l_.size(); ++i) {
    padding_l_[i] = pad_list_[i << 1];
    padding_r_[i] = pad_list_[(i << 1) + 1];
  }

  input_stride_n_ = ComputeStride(in_size_, N_INDEX);
  input_stride_c_ = ComputeStride(in_size_, C_INDEX);
  input_stride_d_ = ComputeStride(in_size_, D_INDEX);
  input_stride_h_ = ComputeStride(in_size_, H_INDEX);
  input_stride_w_ = ComputeStride(in_size_, W_INDEX);
  batches_ = in_size_[kIndex0];
  channels_ = in_size_[kIndex1];
  output_num_ = batches_ * channels_ * out_size_[kD] * out_size_[kH] * out_size_[kW];
  auto in_dtype_size = GetTypeByte(TypeIdToType(inputs[kIndex0]->dtype_id()));
  auto out_dtype_size = GetTypeByte(TypeIdToType(outputs[kIndex0]->dtype_id()));

  use_channel_last_ = src_dim == SHAPE_5D && dtype_ == kNumberTypeFloat32 && channels_ >= kMinChannelBlock;
  if (use_channel_last_) {
    InitPooling3DParams();
    size_t ws_size = batches_ * channels_ * in_size_[kD] * in_size_[kH] * in_size_[kW] * in_dtype_size;
    (void)workspace_size_list_.emplace_back(ws_size);                       // output buffer of transposing of input
    (void)workspace_size_list_.emplace_back(output_num_ * out_dtype_size);  // output buffer of pooling of ndhwc
  }
  return KRET_OK;
}

std::vector<KernelAttr> PoolingCpuKernelNnaclMod::GetOpSupport() {
  static std::map<std::string, std::vector<KernelAttr>> support_list_map = {
    {kMaxPoolOpName,
     {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
      KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
      KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64)}},
    {kAvgPoolOpName,
     {KernelAttr()
        .AddInputAttr(kNumberTypeFloat32)
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)   // kernel_size
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)   // strides
        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)  // pad_mode
        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)  // data_format
        .AddOutputAttr(kNumberTypeFloat32),
      KernelAttr()
        .AddInputAttr(kNumberTypeFloat16)
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)   // kernel_size
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)   // strides
        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)  // pad_mode
        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)  // data_format
        .AddOutputAttr(kNumberTypeFloat16),
      KernelAttr()
        .AddInputAttr(kNumberTypeFloat64)
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)   // kernel_size
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)   // strides
        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)  // pad_mode
        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)  // data_format
        .AddOutputAttr(kNumberTypeFloat64)}},
    {kMaxPool3DOpName,
     {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
      KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
      KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64)}},
    {kAvgPool3DOpName,
     {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32),
      KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16),
      KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64)}},
  };
  auto iter = support_list_map.find(kernel_type_);
  if (iter == support_list_map.end()) {
    MS_LOG(EXCEPTION) << "Does not support " << kernel_type_ << "!";
  }
  return iter->second;
}

template <typename T>
CTask PoolingCpuKernelNnaclMod::KernelAvgPool(T *input_addr, T *output_addr) {
  CTask task = [&, input_addr, output_addr](size_t start, size_t end) {
    const int64_t d_max = in_size_[kD] + padding_r_[kD - kDepthOffset];
    const int64_t h_max = in_size_[kH] + padding_r_[kH - kDepthOffset];
    const int64_t w_max = in_size_[kW] + padding_r_[kW - kDepthOffset];
    const bool count_include_pad = count_include_pad_;
    int64_t divisor = divisor_override_;

    int64_t n = 0, c = 0, d = 0, h = 0, w = 0;
    offset_to_index_init((int64_t)start, &n, batches_, &c, channels_, &d, out_size_[kD], &h, out_size_[kH], &w,
                         out_size_[kW]);

    for (size_t i = start; i < end; i++) {
      int64_t d_start = d * stride_size_[kD] - padding_l_[kD - kDepthOffset];
      int64_t d_end = std::min(d_start + kernel_size_[kD], d_max);
      int64_t d_start2 = std::max(d_start, static_cast<int64_t>(0));
      int64_t d_end2 = std::min(d_end, in_size_[kD]);
      int64_t h_start = h * stride_size_[kH] - padding_l_[kH - kDepthOffset];
      int64_t h_end = std::min(h_start + kernel_size_[kH], h_max);
      int64_t h_start2 = std::max(h_start, static_cast<int64_t>(0));
      int64_t h_end2 = std::min(h_end, in_size_[kH]);
      int64_t w_start = w * stride_size_[kW] - padding_l_[kW - kDepthOffset];
      int64_t w_end = std::min(w_start + kernel_size_[kW], w_max);
      int64_t w_start2 = std::max(w_start, static_cast<int64_t>(0));
      int64_t w_end2 = std::min(w_end, in_size_[kW]);

      if (divisor_override_ == 0) {
        if (count_include_pad) {
          divisor = (d_end - d_start) * (h_end - h_start) * (w_end - w_start);
        } else {
          divisor = (d_end2 - d_start2) * (h_end2 - h_start2) * (w_end2 - w_start2);
        }
      }

      T *input = input_addr + n * input_stride_n_ + c * input_stride_c_;
      float sum = 0.0f;
      for (auto dd = d_start2; dd < d_end2; ++dd) {
        int64_t stride_d = dd * input_stride_d_;
        for (auto hh = h_start2; hh < h_end2; ++hh) {
          int64_t stride_dh = stride_d + hh * input_stride_h_;
          for (auto ww = w_start2; ww < w_end2; ++ww) {
            int64_t index = stride_dh + ww;
            sum += static_cast<float>(input[index]);
          }
        }
      }
      output_addr[i] = static_cast<T>(sum / divisor);
      offset_to_index_step(&n, batches_, &c, channels_, &d, out_size_[kD], &h, out_size_[kH], &w, out_size_[kW]);
    }
  };
  return task;
}

template <typename T>
CTask PoolingCpuKernelNnaclMod::KernelMaxPool(T *input_addr, T *output_addr) {
  CTask task = [&, input_addr, output_addr](size_t start, size_t end) {
    int64_t n = 0, c = 0, d = 0, h = 0, w = 0;
    offset_to_index_init((int64_t)start, &n, batches_, &c, channels_, &d, out_size_[kD], &h, out_size_[kH], &w,
                         out_size_[kW]);

    for (size_t i = start; i < end; i++) {
      int64_t d_start = d * stride_size_[kD] - padding_l_[kD - kDepthOffset];
      int64_t d_end = std::min(d_start + kernel_size_[kD], in_size_[kD]);
      d_start = std::max(d_start, (int64_t)0);
      int64_t h_start = h * stride_size_[kH] - padding_l_[kH - kDepthOffset];
      int64_t h_end = std::min(h_start + kernel_size_[kH], in_size_[kH]);
      h_start = std::max(h_start, (int64_t)0);
      int64_t w_start = w * stride_size_[kW] - padding_l_[kW - kDepthOffset];
      int64_t w_end = std::min(w_start + kernel_size_[kW], in_size_[kW]);
      w_start = std::max(w_start, (int64_t)0);

      T *input = input_addr + n * input_stride_n_ + c * input_stride_c_;
      T tmp_max = static_cast<T>(-FLT_MAX);
      for (auto dd = d_start; dd < d_end; ++dd) {
        int64_t stride_d = dd * input_stride_d_;
        for (auto hh = h_start; hh < h_end; ++hh) {
          int64_t stride_dh = stride_d + hh * input_stride_h_;
          for (auto ww = w_start; ww < w_end; ++ww) {
            int64_t index = stride_dh + ww;
            tmp_max = std::max(input[index], tmp_max);
          }
        }
      }
      output_addr[i] = tmp_max;
      offset_to_index_step(&n, batches_, &c, channels_, &d, out_size_[kD], &h, out_size_[kH], &w, out_size_[kW]);
    }
  };
  return task;
}

void PoolingCpuKernelNnaclMod::LaunchTransposeFp32(float *input_addr, float *output_addr, int plane, int channel) {
  int m = UP_DIV(plane, C8NUM);
  int n = UP_DIV(channel, C8NUM);
  size_t task_num = batches_ * m * n;

  CTask task = [&, input_addr, output_addr](size_t start, size_t end) {
    TransposeFp32(input_addr, output_addr, batches_, plane, channel, start, end);
  };
  ParallelLaunch(task, task_num, 1.0);
}

void PoolingCpuKernelNnaclMod::LaunchPoolingChannelLastFp32(float *input_addr, float *transpose_out, float *pooling_out,
                                                            float *output_addr) {
  size_t task_num = batches_ * out_size_[kD] * out_size_[kH] * out_size_[kW];
  int in_plane = in_size_[kD] * in_size_[kH] * in_size_[kW];
  LaunchTransposeFp32(input_addr, transpose_out, channels_, in_plane);

  CTask task;
  if (pool_mode_ == MEAN_POOLING) {
    task = [&, transpose_out, pooling_out](size_t start, size_t end) {
      AvgPooling3D_NDHWC(transpose_out, pooling_out, &pooling_param_, &pooling_args_, start, end);
    };
  } else {
    task = [&, transpose_out, pooling_out](size_t start, size_t end) {
      MaxPooling3D_NDHWC(transpose_out, pooling_out, &pooling_param_, &pooling_args_, start, end);
    };
  }
  ParallelLaunch(task, task_num, 1.0);

  int out_plane = out_size_[kD] * out_size_[kH] * out_size_[kW];
  LaunchTransposeFp32(pooling_out, output_addr, out_plane, channels_);
}

template <typename T>
bool PoolingCpuKernelNnaclMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                            const std::vector<kernel::KernelTensor *> &,
                                            const std::vector<kernel::KernelTensor *> &outputs) {
  T *input_addr = reinterpret_cast<T *>(inputs[kIndex0]->device_ptr());
  T *output_addr = reinterpret_cast<T *>(outputs[kIndex0]->device_ptr());
  CTask task =
    pool_mode_ == MEAN_POOLING ? KernelAvgPool<T>(input_addr, output_addr) : KernelMaxPool<T>(input_addr, output_addr);
  ParallelLaunch(task, output_num_, 1.0);
  return true;
}

#define POOL3D_KERNEL_CHANNEL_FIRST_CASE(TYPENUM, DTYPE) \
  case (TYPENUM): {                                      \
    LaunchKernel<DTYPE>(inputs, workspaces, outputs);    \
    break;                                               \
  }

bool PoolingCpuKernelNnaclMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                      const std::vector<kernel::KernelTensor *> &workspaces,
                                      const std::vector<kernel::KernelTensor *> &outputs) {
  if (kernel_name_ == kAvgPoolOpName) {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), 5, kernel_name_);
  } else {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), 1, kernel_name_);
  }
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), 1, kernel_name_);

  if (use_channel_last_) {
    float *input_addr = reinterpret_cast<float *>(inputs[kIndex0]->device_ptr());
    float *output_addr = reinterpret_cast<float *>(outputs[kIndex0]->device_ptr());
    float *transpose_out = GetDeviceAddress<float>(workspaces, 0);
    float *pooling_out = GetDeviceAddress<float>(workspaces, 1);
    LaunchPoolingChannelLastFp32(input_addr, transpose_out, pooling_out, output_addr);
    return true;
  }
  switch (dtype_) {
    POOL3D_KERNEL_CHANNEL_FIRST_CASE(kNumberTypeFloat32, float)
    POOL3D_KERNEL_CHANNEL_FIRST_CASE(kNumberTypeFloat16, float16)
    POOL3D_KERNEL_CHANNEL_FIRST_CASE(kNumberTypeFloat64, double)
    default:
      MS_LOG(ERROR) << "For '" << kernel_name_ << "', the type of input should be float16, float32 or float64, but got "
                    << TypeIdToType(dtype_)->ToString();
      return false;
  }
  return true;
}

MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, MaxPool,
                                 []() { return std::make_shared<PoolingCpuKernelNnaclMod>(kMaxPoolOpName); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, AvgPool,
                                 []() { return std::make_shared<PoolingCpuKernelNnaclMod>(kAvgPoolOpName); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, AvgPool3D,
                                 []() { return std::make_shared<PoolingCpuKernelNnaclMod>(kAvgPool3DOpName); });
MS_KERNEL_FACTORY_REG_BY_CREATOR(NativeCpuKernelMod, MaxPool3D,
                                 []() { return std::make_shared<PoolingCpuKernelNnaclMod>(kMaxPool3DOpName); });
}  // namespace kernel
}  // namespace mindspore
