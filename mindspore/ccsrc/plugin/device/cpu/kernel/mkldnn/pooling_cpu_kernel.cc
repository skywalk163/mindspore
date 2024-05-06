/**
 * Copyright 2019-2023 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/mkldnn/pooling_cpu_kernel.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <functional>
#include "ops/conv_pool_op_name.h"
#include "kernel/format_utils.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kPoolingInputsNum = 1;
constexpr size_t kAvgPoolInputsNum = 5;
constexpr size_t kPoolingOutputsNum = 1;
}  // namespace

void PoolingCpuKernelMod::InitPoolingFields(const std::vector<KernelTensor *> &inputs,
                                            const std::vector<KernelTensor *> &outputs) {
  if (kernel_name_ == kAvgPoolOpName) {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kAvgPoolInputsNum, kernel_name_);
  } else {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kPoolingInputsNum, kernel_name_);
  }
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kPoolingOutputsNum, kernel_name_);
  dtype_ = inputs[0]->dtype_id();

  if (kernel_name_ == kAvgPoolOpName) {
    kernel_include_nc = inputs[kIndex1]->GetValueWithCheck<std::vector<int64_t>>();
    strides_include_nc = inputs[kIndex2]->GetValueWithCheck<std::vector<int64_t>>();
    pad_mode_ = static_cast<mindspore::PadMode>(inputs[kIndex3]->GetValueWithCheck<int64_t>());
    format_ = static_cast<mindspore::Format>(inputs[kIndex4]->GetValueWithCheck<int64_t>());
  } else {
    kernel_include_nc = GetValue<std::vector<int64_t>>(KernelMod::primitive_->GetAttr(KERNEL_SIZE));
    strides_include_nc = GetValue<std::vector<int64_t>>(KernelMod::primitive_->GetAttr(STRIDES));
    pad_mode_ = static_cast<mindspore::PadMode>(
      ops::PadModeStringToInt(GetValue<std::string>(KernelMod::primitive_->GetAttr(PAD_MODE))));
    format_ = GetFormatFromStrToEnum(GetValue<std::string>(KernelMod::primitive_->GetAttr(FORMAT)));
  }

  if (KernelMod::primitive_->HasAttr(CEIL_MODE)) {
    ValuePtr ceil_mode = KernelMod::primitive_->GetAttr(CEIL_MODE);
    ceil_mode_ = (ceil_mode->isa<BoolImm>() && GetValue<bool>(ceil_mode)) ||
                 (ceil_mode->isa<Int64Imm>() && GetValue<int64_t>(ceil_mode) == 1);
  }

  if (kernel_name_ == kAvgPool3DOpName && (pad_mode_ == mindspore::PadMode::PAD) &&
      KernelMod::primitive_->HasAttr(DIVISOR_OVERRIDE) &&
      GetValue<int64_t>(KernelMod::primitive_->GetAttr(DIVISOR_OVERRIDE)) != 0 &&
      KernelMod::primitive_->HasAttr(COUNT_INCLUDE_PAD) &&
      !GetValue<bool>(KernelMod::primitive_->GetAttr(COUNT_INCLUDE_PAD))) {
    auto pad = GetValue<std::vector<int64_t>>(KernelMod::primitive_->GetAttr(PAD_LIST));
    if (std::any_of(pad.begin(), pad.end(), [](int64_t pad) { return pad > 0; })) {
      MS_LOG(EXCEPTION) << kernel_name_ << "does not support the scenes while padmode == " << pad_mode_
                        << " && padding > 0 && count_include_pad == False && divisor_override != None";
    }
  }

  if (kernel_name_ == kAvgPoolOpName || kernel_name_ == kAvgPool3DOpName) {
    algorithm_ = dnnl::algorithm::pooling_avg;
    if (KernelMod::primitive_->HasAttr(COUNT_INCLUDE_PAD) &&
        GetValue<bool>(KernelMod::primitive_->GetAttr(COUNT_INCLUDE_PAD))) {
      algorithm_ = dnnl::algorithm::pooling_avg_include_padding;
    }
    if (KernelMod::primitive_->HasAttr(DIVISOR_OVERRIDE) &&
        GetValue<int64_t>(KernelMod::primitive_->GetAttr(DIVISOR_OVERRIDE)) != 0) {
      divisor_override_ = GetValue<int64_t>(KernelMod::primitive_->GetAttr(DIVISOR_OVERRIDE));
    }
  }
}

bool PoolingCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  InitPoolingFields(inputs, outputs);
  return true;
}

int PoolingCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  auto src_shape = inputs[0]->GetDeviceShapeVector();
  dst_shape_ = outputs[0]->GetDeviceShapeVector();
  const size_t src_dim = src_shape.size();
  const dnnl::memory::desc src_desc = GetDefaultMemDesc(src_shape);
  const dnnl::memory::desc dst_desc = GetDefaultMemDesc(dst_shape_);
  if (src_dim != SHAPE_4D && src_dim != SHAPE_5D) {
    MS_LOG(ERROR) << "Pooling only supports 4D/5D input, but got " << src_dim << "D!";
    return KRET_RESIZE_FAILED;
  }
  if (src_dim == SHAPE_4D && format_ != NCHW) {
    MS_LOG(ERROR) << kernel_name_ << " only supports 4D input with NCHW format, but got format " << format_;
    return KRET_RESIZE_FAILED;
  }
  if (src_dim == SHAPE_5D && format_ != NCDHW) {
    MS_LOG(ERROR) << kernel_name_ << " only supports 5D input with NCDHW format, but got format " << format_;
    return KRET_RESIZE_FAILED;
  }
  if (kernel_include_nc.size() != src_dim) {
    MS_LOG(EXCEPTION) << kernel_name_ << " requires kernel_size must be " << src_dim << "D, but got "
                      << kernel_include_nc.size() << "D!";
  }
  if (strides_include_nc.size() != src_dim) {
    MS_LOG(EXCEPTION) << kernel_name_ << " requires strides must be " << src_dim << "D, but got "
                      << strides_include_nc.size() << "D!";
  }
  const dnnl::memory::dims kernel(kernel_include_nc.begin() + NC_LEN, kernel_include_nc.end());
  const dnnl::memory::dims strides(strides_include_nc.begin() + NC_LEN, strides_include_nc.end());
  const dnnl::memory::dims dilation(kernel.size(), kPoolingDilation);
  dnnl::memory::dims padding_l;
  dnnl::memory::dims padding_r;
  kernel_ = kernel;
  PaddingInfo padding_info{pad_mode_,  kernel_,    strides,           dilation,
                           &padding_l, &padding_r, &padding_invalid_, ceil_mode_};
  GetPadding(src_shape, padding_info);
  const auto desc = CreateDesc<dnnl::pooling_forward::desc>(dnnl::prop_kind::forward_inference, algorithm_, src_desc,
                                                            dst_desc, strides, kernel, padding_l, padding_r);
  const auto prim_desc = CreateDesc<dnnl::pooling_forward::primitive_desc>(desc, engine_);

  primitive_ = CreatePrimitive<dnnl::pooling_forward>(prim_desc);
  AddArgument(DNNL_ARG_SRC, src_desc);
  AddArgument(DNNL_ARG_DST, dst_desc);
  return KRET_OK;
}

template <typename T>
void PoolingCpuKernelMod::EliminateInvalidPadding(T *dst) {
  if (dst_shape_.size() < SHAPE_5D || kernel_.size() + NC_LEN < SHAPE_5D ||
      padding_invalid_.size() + NC_LEN < SHAPE_5D) {
    MS_LOG(EXCEPTION) << "The dst_shape must be 5D, the kernel and the padding_invalid must be 3D!";
  }
  const auto d_max = LongToSize(dst_shape_[D_INDEX] - 1);
  const auto h_max = LongToSize(dst_shape_[H_INDEX] - 1);
  const auto w_max = LongToSize(dst_shape_[W_INDEX] - 1);
  const size_t d_index = D_INDEX - NC_LEN;
  const size_t h_index = H_INDEX - NC_LEN;
  const size_t w_index = W_INDEX - NC_LEN;
  const int64_t valid_d = kernel_[d_index] - padding_invalid_[d_index];
  const int64_t valid_h = kernel_[h_index] - padding_invalid_[h_index];
  const int64_t valid_w = kernel_[w_index] - padding_invalid_[w_index];
  const std::vector<int64_t> valid_kernel_array{kernel_[d_index] * kernel_[h_index] * kernel_[w_index],
                                                kernel_[d_index] * kernel_[h_index] * valid_w,
                                                kernel_[d_index] * valid_h * kernel_[w_index],
                                                kernel_[d_index] * valid_h * valid_w,
                                                valid_d * kernel_[h_index] * kernel_[w_index],
                                                valid_d * kernel_[h_index] * valid_w,
                                                valid_d * valid_h * kernel_[w_index],
                                                valid_d * valid_h * valid_w};
  const int base = 2;
  const int64_t kernel_size = std::accumulate(kernel_.begin(), kernel_.end(), int64_t(1), std::multiplies<int64_t>());
  CTask task = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      for (size_t d = 0; d <= d_max; d++) {
        for (size_t h = 0; h <= h_max; h++) {
          for (size_t w = 0; w <= w_max; w++) {
            const char d_bound = d == d_max ? '1' : '0';
            const char h_bound = h == h_max ? '1' : '0';
            const char w_bound = w == w_max ? '1' : '0';
            const std::string bin{d_bound, h_bound, w_bound};
            int kernel_index = 0;
            try {
              kernel_index = std::stoi(bin, nullptr, base);
            } catch (const std::invalid_argument &e) {
              MS_LOG(EXCEPTION) << "invalid string to be cast to int!";
            } catch (const std::out_of_range &e) {
              MS_LOG(EXCEPTION) << "value is out of the range of int!";
            }
            const int64_t valid_kernel_size = valid_kernel_array[kernel_index];
            if (valid_kernel_size != kernel_size) {
              const size_t index =
                static_cast<size_t>(i * dst_shape_[D_INDEX] * dst_shape_[H_INDEX] * dst_shape_[W_INDEX] +
                                    d * dst_shape_[H_INDEX] * dst_shape_[W_INDEX] + h * dst_shape_[W_INDEX] + w);
              dst[index] =
                dst[index] * static_cast<T>(LongToFloat(kernel_size)) / static_cast<T>(LongToFloat(valid_kernel_size));
            }
          }
        }
      }
    }
  };
  ParallelLaunchAutoSearch(task, static_cast<size_t>(dst_shape_[N_INDEX] * dst_shape_[C_INDEX]), this,
                           &parallel_search_info_);
}

template <typename T>
void PoolingCpuKernelMod::ReComputeDivisor(T *dst) {
  const int64_t kernel_size = std::accumulate(kernel_.begin(), kernel_.end(), int64_t(1), std::multiplies<int64_t>());
  const size_t size = std::accumulate(dst_shape_.begin(), dst_shape_.end(), size_t(1), std::multiplies<size_t>());
  CTask task = [&](size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
      dst[i] = dst[i] * static_cast<T>(LongToFloat(kernel_size)) / static_cast<T>(LongToFloat(divisor_override_));
    }
  };
  ParallelLaunchAutoSearch(task, size, this, &parallel_search_info_);
}

std::vector<KernelAttr> PoolingCpuKernelMod::GetOpSupport() {
  static std::map<std::string, std::vector<KernelAttr>> support_list_map = {
    {kMaxPoolOpName, {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32)}},
    {kAvgPoolOpName,
     {KernelAttr()
        .AddInputAttr(kNumberTypeFloat32)
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
        .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64)
        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
        .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
        .AddOutputAttr(kNumberTypeFloat32)}},
    {kAvgPoolOpName, {KernelAttr().AddInputAttr(kNumberTypeFloat16).AddOutputAttr(kNumberTypeFloat16)}},
    {kAvgPoolOpName, {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64)}}};
  auto iter = support_list_map.find(kernel_type_);
  if (iter == support_list_map.end()) {
    MS_LOG(EXCEPTION) << "Does not support " << kernel_type_ << "!";
  }
  return iter->second;
}

template <typename T>
bool PoolingCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                       const std::vector<kernel::KernelTensor *> &outputs) {
  SetArgumentHandle(DNNL_ARG_SRC, inputs[0]->device_ptr());
  SetArgumentHandle(DNNL_ARG_DST, outputs[0]->device_ptr());
  ExecutePrimitive();

  T *dst = reinterpret_cast<T *>(outputs[0]->device_ptr());
  if (divisor_override_ != 0) {
    ReComputeDivisor(dst);
    return true;
  }

  bool has_invalid_padding =
    std::any_of(padding_invalid_.begin(), padding_invalid_.end(), [](const int64_t &padding) { return padding != 0; });
  if (algorithm_ == dnnl::algorithm::pooling_avg_include_padding && has_invalid_padding) {
    EliminateInvalidPadding(dst);
  }
  return false;
}

bool PoolingCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                 const std::vector<kernel::KernelTensor *> &,
                                 const std::vector<kernel::KernelTensor *> &outputs) {
  if (kernel_name_ == kAvgPoolOpName) {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kAvgPoolInputsNum, kernel_name_);
  } else {
    CHECK_KERNEL_INPUTS_NUM(inputs.size(), kPoolingInputsNum, kernel_name_);
  }
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kPoolingOutputsNum, kernel_name_);
  if (dtype_ == kNumberTypeFloat32) {
    LaunchKernel<float>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat16) {
    LaunchKernel<float16>(inputs, outputs);
  } else if (dtype_ == kNumberTypeFloat64) {
    LaunchKernel<double>(inputs, outputs);
  } else {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the dtype of input should be float16, float32 or float64, but got "
                  << TypeIdToType(dtype_)->ToString();
  }
  return true;
}
}  // namespace kernel
}  // namespace mindspore
