/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#include "plugin/device/cpu/kernel/bias_add_grad_cpu_kernel.h"
#include <complex>
#include "ops/ops_func_impl/bias_add_grad.h"

namespace mindspore {
namespace kernel {
bool BiasAddGradCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  if (!MatchKernelFunc(kernel_name_, inputs, outputs)) {
    return false;
  }
  return true;
}

int BiasAddGradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  int ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }
  data_format_ = inputs[kIndex1]->GetValueWithCheck<int64_t>();
  input_shape_ = Convert2SizeTClipNeg(inputs[kIndex0]->GetShapeVector());
  if (data_format_ == Format::NCDHW && input_shape_.size() != kShape5dDims) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', NCDHW format only supports 5-D input on CPU, but got a "
                             << input_shape_.size() << "-D input.";
  }
  const int64_t error_size = 1;
  if (data_format_ == Format::NCHW && input_shape_.size() == kShape3dDims && input_shape_[kDim2] == error_size) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_
                             << "', input tensor's dimension is 3, when data_format is NCHW "
                                "the last dimension size should greater than 1.";
  }
  return ret;
}

bool BiasAddGradCpuKernelMod::Launch(const std::vector<KernelTensor *> &inputs,
                                     const std::vector<KernelTensor *> &workspace,
                                     const std::vector<KernelTensor *> &outputs) {
  return kernel_func_(this, inputs, workspace, outputs);
}

template <typename T>
bool BiasAddGradCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &,
                                           const std::vector<KernelTensor *> &outputs) {
  const auto *input_addr = reinterpret_cast<T *>(inputs[0]->device_ptr());
  auto *output_addr = reinterpret_cast<T *>(outputs[0]->device_ptr());

  if (data_format_ == Format::NHWC) {
    int64_t input_shape_size = SizeToLong(input_shape_.size());
    size_t kStep = input_shape_[input_shape_size - 1];
    size_t num_value = 1;
    for (size_t i = 0; i < kStep; i++) {
      output_addr[i] = (T)0;
    }
    for (size_t i = 0; i < input_shape_.size(); ++i) {
      num_value *= input_shape_[i];
    }

    for (size_t i = 0; i < num_value; i++) {
      if (kStep == 0) {
        MS_LOG(EXCEPTION) << "For kStep, The value can not be 0 " << kStep;
      } else {
        output_addr[i % kStep] += input_addr[i];
      }
    }
  } else if (input_shape_.size() > kShape2dDims) {
    size_t hw_size = 1;
    for (size_t i = kShape2dDims; i < input_shape_.size(); ++i) {
      hw_size *= input_shape_[i];
    }

    size_t c_size = input_shape_[kIndex1];
    size_t n_size = LongToSize(input_shape_[0]);
    std::vector<T> tmp_res(n_size * c_size, 0);
    for (size_t n = 0; n < n_size; ++n) {
      for (size_t c = 0; c < c_size; ++c) {
        size_t offset = n * c_size * hw_size + c * hw_size;
        size_t nc_offset = n * c_size + c;
        const T *inner_src = input_addr + offset;
        T *inner_dst = tmp_res.data() + nc_offset;
        T tmp = static_cast<T>(0);
        for (size_t i = 0; i < hw_size; i++) {
          tmp += inner_src[i];
        }
        *inner_dst = tmp;
      }
    }
    auto task = [this, input_addr, output_addr, &tmp_res, c_size, n_size](size_t start, size_t end) {
      for (size_t k = 0; k < end - start; k++) {
        const T *inner_src = tmp_res.data() + start + k;
        T *inner_dst = output_addr + start + k;
        T tmp = static_cast<T>(0);
        for (size_t i = 0; i < n_size; i++) {
          tmp += inner_src[i * c_size];
        }
        *inner_dst = tmp;
      }
    };
    ParallelLaunchAutoSearch(task, c_size, this, &parallel_search_info_);
  } else if (input_shape_.size() == kShape2dDims) {
    auto task = [this, input_addr, output_addr](size_t start, size_t end) {
      for (size_t k = 0; k < end - start; k++) {
        const T *inner_src = input_addr + start + k;
        T *inner_dst = output_addr + start + k;
        T tmp = static_cast<T>(0);
        for (size_t i = 0; i < input_shape_[kIndex0]; i++) {
          tmp += inner_src[i * input_shape_[kIndex1]];
        }
        *inner_dst = tmp;
      }
    };
    ParallelLaunchAutoSearch(task, input_shape_[kIndex1], this, &parallel_search_info_);
  }
  return true;
}

const std::vector<std::pair<KernelAttr, BiasAddGradCpuKernelMod::KernelRunFunc>> &BiasAddGradCpuKernelMod::GetFuncList()
  const {
  static const std::vector<std::pair<KernelAttr, BiasAddGradCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat32),
     &BiasAddGradCpuKernelMod::LaunchKernel<float>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeFloat64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeFloat64),
     &BiasAddGradCpuKernelMod::LaunchKernel<double>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeInt8)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt8),
     &BiasAddGradCpuKernelMod::LaunchKernel<int8_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeInt16)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt16),
     &BiasAddGradCpuKernelMod::LaunchKernel<int16_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeInt32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt32),
     &BiasAddGradCpuKernelMod::LaunchKernel<int>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeInt64),
     &BiasAddGradCpuKernelMod::LaunchKernel<int64_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeUInt8)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeUInt8),
     &BiasAddGradCpuKernelMod::LaunchKernel<uint8_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeUInt16)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeUInt16),
     &BiasAddGradCpuKernelMod::LaunchKernel<uint16_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeUInt32)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeUInt32),
     &BiasAddGradCpuKernelMod::LaunchKernel<uint32_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeUInt64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeUInt64),
     &BiasAddGradCpuKernelMod::LaunchKernel<uint64_t>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeComplex64)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeComplex64),
     &BiasAddGradCpuKernelMod::LaunchKernel<std::complex<float>>},
    {KernelAttr()
       .AddInputAttr(kNumberTypeComplex128)
       .AddInputAttr(kObjectTypeNumber, kNumberTypeInt64)
       .AddOutputAttr(kNumberTypeComplex128),
     &BiasAddGradCpuKernelMod::LaunchKernel<std::complex<double>>},
  };
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, BiasAddGrad, BiasAddGradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
