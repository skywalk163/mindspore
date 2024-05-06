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
#include "plugin/device/cpu/kernel/segment_mean_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace {
const size_t kSegmentsThreshold = 2 * 1024;
const size_t kDataSizeThreshold = 2 * 1024;

#define SEGMENTMEAN_COMPUTE_CASE(DTYPE, TYPE1, TYPE2)  \
  case (DTYPE): {                                      \
    ret = LaunchKernel<TYPE1, TYPE2>(inputs, outputs); \
    break;                                             \
  }
}  // namespace

namespace mindspore {
namespace kernel {
template <typename T>
T ComplexDiv(T sum, size_t num) {
  if (num != 0) {
    T res;
    auto real = sum.real();
    auto imag = sum.imag();
    res.real(real / num);
    res.imag(imag / num);
    return res;
  } else {
    MS_EXCEPTION(ValueError) << "For 'SegmentMean', divisor can not be 0.";
  }
}

bool SegmentMeanCPUKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                   const std::vector<KernelTensor *> &outputs) {
  input_x_dtype_ = inputs.at(kIndex0)->dtype_id();
  segment_ids_dtype_ = inputs.at(kIndex1)->dtype_id();
  output_dtype_ = outputs.at(kIndex0)->dtype_id();
  return true;
}

int SegmentMeanCPUKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                    const std::vector<KernelTensor *> &outputs) {
  if (auto ret = NativeCpuKernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  input_x_shape_ = inputs.at(kIndex0)->GetShapeVector();
  segment_ids_shape_ = inputs.at(kIndex1)->GetShapeVector();
  output_shape_ = outputs.at(kIndex0)->GetShapeVector();
  input_x_num_ = SizeOf(input_x_shape_);
  segment_ids_num_ = SizeOf(segment_ids_shape_);
  output_num_ = SizeOf(output_shape_);
  return KRET_OK;
}

std::vector<KernelAttr> SegmentMeanCPUKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list = {
    KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat16),
    KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat32),
    KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeFloat64),
    KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt8),
    KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt16),
    KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt32),
    KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeUInt64),
    KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt8),
    KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt16),
    KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32),
    KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt64),
    KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeComplex64),
    KernelAttr()
      .AddInputAttr(kNumberTypeComplex128)
      .AddInputAttr(kNumberTypeInt32)
      .AddOutputAttr(kNumberTypeComplex128),
    KernelAttr().AddInputAttr(kNumberTypeFloat16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat16),
    KernelAttr().AddInputAttr(kNumberTypeFloat32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat32),
    KernelAttr().AddInputAttr(kNumberTypeFloat64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeFloat64),
    KernelAttr().AddInputAttr(kNumberTypeUInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt8),
    KernelAttr().AddInputAttr(kNumberTypeUInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt16),
    KernelAttr().AddInputAttr(kNumberTypeUInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt32),
    KernelAttr().AddInputAttr(kNumberTypeUInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeUInt64),
    KernelAttr().AddInputAttr(kNumberTypeInt8).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt8),
    KernelAttr().AddInputAttr(kNumberTypeInt16).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt16),
    KernelAttr().AddInputAttr(kNumberTypeInt32).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt32),
    KernelAttr().AddInputAttr(kNumberTypeInt64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64),
    KernelAttr().AddInputAttr(kNumberTypeComplex64).AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeComplex64),
    KernelAttr()
      .AddInputAttr(kNumberTypeComplex128)
      .AddInputAttr(kNumberTypeInt64)
      .AddOutputAttr(kNumberTypeComplex128)};
  return support_list;
}

bool SegmentMeanCPUKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                     const std::vector<kernel::KernelTensor *> &,
                                     const std::vector<kernel::KernelTensor *> &outputs) {
  bool ret = true;
  switch (segment_ids_dtype_) {
    case kNumberTypeInt32: {
      switch (input_x_dtype_) {
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeComplex64, std::complex<float>, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeComplex128, std::complex<double>, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeInt8, int8_t, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeInt16, int16_t, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeInt32, int32_t, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeInt64, int64_t, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeUInt8, uint8_t, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeUInt16, uint16_t, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeUInt32, uint32_t, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeUInt64, uint64_t, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeFloat16, float16, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeFloat32, float, int32_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeFloat64, double, int32_t)
        default:
          MS_EXCEPTION(TypeError) << "For 'SegmentMean', unsupported input_x data type: " << input_x_dtype_;
      }
      break;
    }
    case kNumberTypeInt64: {
      switch (input_x_dtype_) {
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeComplex64, std::complex<float>, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeComplex128, std::complex<double>, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeInt8, int8_t, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeInt16, int16_t, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeInt32, int32_t, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeInt64, int64_t, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeUInt8, uint8_t, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeUInt16, uint16_t, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeUInt32, uint32_t, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeUInt64, uint64_t, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeFloat16, float16, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeFloat32, float, int64_t)
        SEGMENTMEAN_COMPUTE_CASE(kNumberTypeFloat64, double, int64_t)
        default:
          MS_EXCEPTION(TypeError) << "For 'SegmentMean', unsupported input_x data type: " << input_x_dtype_;
      }
      break;
    }
    default:
      MS_EXCEPTION(TypeError) << "For 'SegmentMean', unsupported segment_ids data type: " << segment_ids_dtype_;
  }
  return ret;
}

template <typename T1, typename T2>
bool SegmentMeanCPUKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                           const std::vector<kernel::KernelTensor *> &outputs) {
  auto input_x_data_addr = static_cast<T1 *>(inputs[0]->device_ptr());
  auto segment_ids_data_addr = static_cast<T2 *>(inputs[1]->device_ptr());
  auto output_data_addr = static_cast<T1 *>(outputs[0]->device_ptr());
  std::vector<int64_t> segments = CPUKernelUtils::CalcSegmentIds(segment_ids_data_addr, segment_ids_num_);
  for (size_t i = 0; i < output_num_; ++i) {
    output_data_addr[i] = static_cast<T1>(0);
  }
  const size_t num_compare_per = input_x_num_ / LongToSize(input_x_shape_[0]);
  const size_t num_segments = segments.size();
  if (num_segments < kSegmentsThreshold) {
    for (size_t i = 0; i < num_segments; ++i) {
      const size_t count = static_cast<size_t>(segments[i]);
      size_t count_no = 0;
      for (size_t j = 0; j < i; ++j) {
        count_no += static_cast<size_t>(segments[j]);
      }
      size_t input_addr_base = count_no * num_compare_per;
      auto task = [&](size_t start, size_t end) {
        for (size_t j = start; j < end; ++j) {
          size_t mean_init_addr = input_addr_base + j;
          T1 sum_value = input_x_data_addr[mean_init_addr];
          for (size_t k = 1; k < count; ++k) {
            int tmp_addr = mean_init_addr + k * num_compare_per;
            sum_value += input_x_data_addr[tmp_addr];
          }
          size_t output_index = static_cast<size_t>(segment_ids_data_addr[count_no] * num_compare_per + j);
          if constexpr (std::is_same_v<T1, std::complex<float>>) {
            output_data_addr[output_index] = ComplexDiv(sum_value, count);
          } else if constexpr (std::is_same_v<T1, std::complex<double>>) {
            output_data_addr[output_index] = ComplexDiv(sum_value, count);
          } else {
            output_data_addr[output_index] = sum_value / static_cast<T1>(count);
          }
        }
      };
      if (num_compare_per < kDataSizeThreshold) {
        task(0, num_compare_per);
      } else {
        CPUKernelUtils::ParallelFor(task, num_compare_per);
      }
    }
  } else {
    auto task = [&](size_t start, size_t end) {
      for (size_t i = start; i < end; ++i) {
        const size_t count = static_cast<size_t>(segments[i]);
        size_t count_no = 0;
        for (size_t j = 0; j < i; ++j) {
          count_no += static_cast<size_t>(segments[j]);
        }
        size_t input_addr_base = count_no * num_compare_per;
        for (size_t j = 0; j < num_compare_per; ++j) {
          size_t mean_init_addr = input_addr_base + j;
          T1 sum_value = input_x_data_addr[mean_init_addr];
          for (size_t k = 1; k < count; ++k) {
            int tmp_addr = mean_init_addr + k * num_compare_per;
            sum_value += input_x_data_addr[tmp_addr];
          }
          size_t output_index = static_cast<size_t>(segment_ids_data_addr[count_no] * num_compare_per + j);
          if constexpr (std::is_same_v<T1, std::complex<float>>) {
            output_data_addr[output_index] = ComplexDiv(sum_value, count);
          } else if constexpr (std::is_same_v<T1, std::complex<double>>) {
            output_data_addr[output_index] = ComplexDiv(sum_value, count);
          } else {
            output_data_addr[output_index] = sum_value / static_cast<T1>(count);
          }
        }
      }
    };
    CPUKernelUtils::ParallelFor(task, num_segments);
  }
  return true;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, SegmentMean, SegmentMeanCPUKernelMod);
}  // namespace kernel
}  // namespace mindspore
