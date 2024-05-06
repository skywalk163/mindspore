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

#include "plugin/device/cpu/kernel/resize_nearest_neighbor_v2_cpu_kernel.h"
#include <string>
#include "kernel/ops_utils.h"
#include "mindspore/core/ops/ops_func_impl/resize_nearest_neighbor_v2.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "plugin/device/cpu/kernel/eigen/eigen_common_utils.h"

namespace mindspore {
namespace kernel {
namespace {
constexpr size_t kResizeNearestNeighborV2InputsNum = 4;
constexpr size_t kResizeNearestNeighborV2OutputNum = 1;
}  // namespace

bool ResizeNearestNeighborV2CpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                               const std::vector<KernelTensor *> &outputs) {
  auto kernel_attr = GetKernelAttrFromTensors(inputs, outputs);
  auto [is_match, index] = MatchKernelAttr(kernel_attr, GetOpSupport());
  if (!is_match) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it does not support this kernel data type: " << kernel_attr;
    return false;
  }
  kernel_func_ = func_list_[index].second;
  return true;
}

int ResizeNearestNeighborV2CpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                                const std::vector<KernelTensor *> &outputs) {
  CHECK_KERNEL_INPUTS_NUM(inputs.size(), kResizeNearestNeighborV2InputsNum, kernel_name_);
  CHECK_KERNEL_OUTPUTS_NUM(outputs.size(), kResizeNearestNeighborV2OutputNum, kernel_name_);
  auto ret = KernelMod::Resize(inputs, outputs);
  if (ret != KRET_OK) {
    return ret;
  }
  x_shape_ = inputs[kIndex0]->GetDeviceShapeVector();
  y_shape_ = outputs[kIndex0]->GetDeviceShapeVector();
  align_corners_ = inputs.at(kIndex2)->GetValueWithCheck<bool>();
  half_pixel_centers_ = inputs.at(kIndex3)->GetValueWithCheck<bool>();
  return KRET_OK;
}

template <typename T>
bool ResizeNearestNeighborV2CpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                       const std::vector<kernel::KernelTensor *> &outputs) {
  const int64_t batch_size = x_shape_[kIndex0];
  const int64_t channels = x_shape_[kIndex1];
  const int64_t in_height = x_shape_[kIndex2];
  const int64_t in_width = x_shape_[kIndex3];

  const int64_t out_height = y_shape_[kIndex2];
  const int64_t out_width = y_shape_[kIndex3];

  const float height_scale = Scaling(static_cast<size_t>(in_height), static_cast<size_t>(out_height), align_corners_);
  const float width_scale = Scaling(static_cast<size_t>(in_width), static_cast<size_t>(out_width), align_corners_);

  auto x_4d = EigenTensor(x_shape_, inputs[kIndex0]->device_ptr()).tensor<T, kDim4>();
  auto y_4d = EigenTensor(y_shape_, outputs[kIndex0]->device_ptr()).tensor<T, kDim4>();
  for (int64_t b = 0; b < batch_size; ++b) {
    for (int64_t y = 0; y < out_height; ++y) {
      int64_t in_y =
        std::min((align_corners_)
                   ? static_cast<int64_t>(roundf(Scaler(static_cast<size_t>(y), height_scale, half_pixel_centers_)))
                   : static_cast<int64_t>(floorf(Scaler(static_cast<size_t>(y), height_scale, half_pixel_centers_))),
                 in_height - 1);
      if (half_pixel_centers_) {
        in_y = std::max(static_cast<int64_t>(0), in_y);
      }
      for (int64_t x = 0; x < out_width; ++x) {
        int64_t in_x =
          std::min((align_corners_)
                     ? static_cast<int64_t>(roundf(Scaler(static_cast<size_t>(x), width_scale, half_pixel_centers_)))
                     : static_cast<int64_t>(floorf(Scaler(static_cast<size_t>(x), width_scale, half_pixel_centers_))),
                   in_width - 1);
        if (half_pixel_centers_) {
          in_x = std::max(static_cast<int64_t>(0), in_x);
        }
        for (int64_t c = 0; c < channels; ++c) {
          y_4d(b, c, y, x) = x_4d(b, c, in_y, in_x);
        }
      }
    }
  }
  return true;
}

#define RESIZE_NEAREST_NEIGHBOR_V2_CPU_REG(MS_T, T)   \
  KernelAttr()                                        \
    .AddInputAttr(MS_T)                               \
    .AddInputAttr(kObjectTypeTuple, kNumberTypeInt64) \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool) \
    .AddInputAttr(kObjectTypeNumber, kNumberTypeBool) \
    .AddOutputAttr(MS_T),                             \
    &ResizeNearestNeighborV2CpuKernelMod::LaunchKernel<T>

std::vector<std::pair<KernelAttr, ResizeNearestNeighborV2CpuKernelMod::ResizeNearestNeighborV2LaunchFunc>>
  ResizeNearestNeighborV2CpuKernelMod::func_list_ = {{RESIZE_NEAREST_NEIGHBOR_V2_CPU_REG(kNumberTypeUInt8, uint8_t)},
                                                     {RESIZE_NEAREST_NEIGHBOR_V2_CPU_REG(kNumberTypeFloat16, float16)},
                                                     {RESIZE_NEAREST_NEIGHBOR_V2_CPU_REG(kNumberTypeFloat32, float)},
                                                     {RESIZE_NEAREST_NEIGHBOR_V2_CPU_REG(kNumberTypeFloat64, double)}};

std::vector<KernelAttr> ResizeNearestNeighborV2CpuKernelMod::GetOpSupport() {
  std::vector<KernelAttr> support_list;
  (void)std::transform(
    func_list_.begin(), func_list_.end(), std::back_inserter(support_list),
    [](const std::pair<KernelAttr, ResizeNearestNeighborV2CpuKernelMod::ResizeNearestNeighborV2LaunchFunc> &pair) {
      return pair.first;
    });
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, ResizeNearestNeighborV2, ResizeNearestNeighborV2CpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
