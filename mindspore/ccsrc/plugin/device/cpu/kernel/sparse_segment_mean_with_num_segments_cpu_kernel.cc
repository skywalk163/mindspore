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

#include "plugin/device/cpu/kernel/sparse_segment_mean_with_num_segments_cpu_kernel.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kSparseSegmentMeanWithNumSegmentsInputsNum = 4;
constexpr size_t kSparseSegmentMeanWithNumSegmentsOutputsNum = 1;

#define ADD_KERNEL(t1, t2, t3, t4, t5) \
  KernelAttr()                         \
    .AddInputAttr(kNumberType##t1)     \
    .AddInputAttr(kNumberType##t2)     \
    .AddInputAttr(kNumberType##t3)     \
    .AddInputAttr(kNumberType##t4)     \
    .AddOutputAttr(kNumberType##t5)
}  // namespace

bool SparseSegmentMeanWithNumSegmentsCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                                        const std::vector<KernelTensor *> &outputs) {
  x_dtype_ = inputs.at(kIndex0)->dtype_id();
  indices_dtype_ = inputs.at(kIndex1)->dtype_id();
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kSparseSegmentMeanWithNumSegmentsInputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kSparseSegmentMeanWithNumSegmentsOutputsNum, kernel_name_);
  return true;
}

int SparseSegmentMeanWithNumSegmentsCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                                         const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  x_shape_ = inputs.at(kIndex0)->GetDeviceShapeVector();
  segment_ids_shape_ = inputs.at(kIndex2)->GetDeviceShapeVector();
  y_shape_ = outputs.at(kIndex0)->GetDeviceShapeVector();
  return KRET_OK;
}

bool SparseSegmentMeanWithNumSegmentsCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                                          const std::vector<kernel::KernelTensor *> &,
                                                          const std::vector<kernel::KernelTensor *> &outputs) {
  switch (x_dtype_) {
    case (kNumberTypeFloat16):
      if (indices_dtype_ == kNumberTypeInt32) {
        LaunchKernel<float16, int32_t>(inputs, outputs);
        break;
      } else {
        LaunchKernel<float16, int64_t>(inputs, outputs);
        break;
      }
    case (kNumberTypeFloat32):
      if (indices_dtype_ == kNumberTypeInt32) {
        LaunchKernel<float, int32_t>(inputs, outputs);
        break;
      } else {
        LaunchKernel<float, int64_t>(inputs, outputs);
        break;
      }
    case (kNumberTypeFloat64):
      if (indices_dtype_ == kNumberTypeInt32) {
        LaunchKernel<double, int32_t>(inputs, outputs);
        break;
      } else {
        LaunchKernel<double, int64_t>(inputs, outputs);
        break;
      }
    default:
      MS_EXCEPTION(TypeError) << "For '" << kernel_name_ << "', data type of x is " << TypeIdLabel(x_dtype_)
                              << " which is not supported.";
  }
  return true;
}

template <typename T1, typename T2>
void SparseSegmentMeanWithNumSegmentsCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                                const std::vector<kernel::KernelTensor *> &outputs) {
  constexpr size_t kMultiply = 1;
  size_t n = std::accumulate(x_shape_.begin(), x_shape_.end(), kMultiply, std::multiplies<int>()) / x_shape_[kIndex0];
  size_t m = std::accumulate(segment_ids_shape_.begin(), segment_ids_shape_.end(), kMultiply, std::multiplies<int>());
  size_t num_elements = std::accumulate(y_shape_.begin(), y_shape_.end(), kMultiply, std::multiplies<int>());
  auto x_shape_0 = static_cast<T2>(x_shape_[kIndex0]);
  auto x_addr = static_cast<T1 *>(inputs[kIndex0]->device_ptr());
  auto indices_addr = static_cast<T2 *>(inputs[kIndex1]->device_ptr());
  auto segment_ids_addr = static_cast<T2 *>(inputs[kIndex2]->device_ptr());
  auto num_segments_addr = static_cast<T2 *>(inputs[kIndex3]->device_ptr());
  auto y_addr = static_cast<T1 *>(outputs[kIndex0]->device_ptr());
  for (size_t i = 1; i < m; i++) {
    if (segment_ids_addr[i] < segment_ids_addr[i - 1]) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', input segment_ids should be sorted.";
    }
  }
  if (segment_ids_addr[m - 1] >= num_segments_addr[0]) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_
                             << "', num_segments must be bigger than the largest id of segment_ids.";
  }
  for (size_t i = 0; i < m; i++) {
    if (indices_addr[i] >= x_shape_0) {
      MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', input indices is out of range of x's first dimension.";
    }
  }
  for (size_t i = 0; i < num_elements; i++) {
    y_addr[i] = static_cast<T1>(0);
  }
  int oldindex = -1;
  int countnum = 0;
  for (size_t i = 0; i < m; i++) {
    if (oldindex == segment_ids_addr[i]) {
      countnum++;
    } else {
      if (countnum != 0) {
        for (size_t j = 0; j < n; j++) {
          y_addr[j + static_cast<size_t>(oldindex) * n] /= static_cast<T1>(countnum);
        }
      }
      countnum = 1;
      oldindex = segment_ids_addr[i];
    }
    for (size_t j = 0; j < n; j++) {
      y_addr[j + static_cast<size_t>(oldindex) * n] += x_addr[j + static_cast<size_t>(indices_addr[i]) * n];
    }
  }
  if (countnum != 0) {
    for (size_t j = 0; j < n; j++) {
      y_addr[j + static_cast<size_t>(oldindex) * n] /= static_cast<T1>(countnum);
    }
  }
}

std::vector<KernelAttr> SparseSegmentMeanWithNumSegmentsCpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> kernel_attr_list = {
    ADD_KERNEL(Float16, Int32, Int32, Int32, Float16), ADD_KERNEL(Float16, Int64, Int64, Int64, Float16),
    ADD_KERNEL(Float32, Int32, Int32, Int32, Float32), ADD_KERNEL(Float32, Int64, Int64, Int64, Float32),
    ADD_KERNEL(Float64, Int32, Int32, Int32, Float64), ADD_KERNEL(Float64, Int64, Int64, Int64, Float64)};

  return kernel_attr_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, SparseSegmentMeanWithNumSegments,
                      SparseSegmentMeanWithNumSegmentsCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
