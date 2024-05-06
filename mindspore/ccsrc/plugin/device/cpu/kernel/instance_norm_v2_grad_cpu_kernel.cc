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

#include "plugin/device/cpu/kernel/instance_norm_v2_grad_cpu_kernel.h"
#include <algorithm>
#include <string>
#include <vector>
#include "kernel/common_utils.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "plugin/device/cpu/kernel/eigen/eigen_common_utils.h"

namespace mindspore {
namespace kernel {
namespace {
// GRAIN_SIZE for Parallel
constexpr size_t kGrainSize = 4 * 1024;
constexpr float float_init_zero = 0.0;
constexpr float float_init_one = 1.0;
constexpr double double_init_zero = 0.0;
}  // namespace

int InstanceNormV2GradCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                           const std::vector<KernelTensor *> &outputs) {
  if (auto ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  in_type_ = inputs[kIndex0]->dtype_id();
  std::vector<int64_t> dy_shape_ = inputs[kIndex0]->GetShapeVector();
  std::vector<int64_t> batch_channels_ = inputs[kIndex2]->GetShapeVector();
  if (dy_shape_.size() != kDim4 && dy_shape_.size() != kDim5) {
    MS_EXCEPTION(ValueError) << "For '" << kernel_name_ << "', the dimension of 'dy' should be 4D or 5D, but got "
                             << dy_shape_.size() << "D.";
  }
  is_training_ = GetValue<bool>(primitive_->GetAttr(kAttrIsTraining));
  epsilon_ = GetValue<float>(primitive_->GetAttr(kAttrEpsilon));
  dy_is_4d_ = (dy_shape_.size() == kDim4);
  // Format NCHW could be considered as a situation of format NC1HWC0 when C0 = 1.
  if (dy_is_4d_) {
    // extern (N, C, H, W) to (N, C, H, W, 1)
    dy_shape_.push_back(SizeToLong(kDim1));
    // extern (N, C, 1, 1) to (N, C1=C, 1, 1, C0=1)
    batch_channels_.push_back(SizeToLong(kDim1));
  }
  // consider (N, C1, H, W, C0) as (N*C1, H, W, C0), similar to (N, H, W, C)
  dy_shape_4d_ = {dy_shape_[kIndex0] * dy_shape_[kIndex1], dy_shape_[kIndex2], dy_shape_[kIndex3], dy_shape_[kIndex4]};
  // consider (N, C1, C0) as (N*C1, C0), similar to (N, C)
  batch_channels_2d_ = {batch_channels_[kIndex0] * batch_channels_[kIndex1], batch_channels_[kIndex4]};
  instance_num = CPUKernelUtils::CalcElementNum(batch_channels_2d_);
  return KRET_OK;
}

bool InstanceNormV2GradCpuKernelMod::Launch(const std::vector<kernel::KernelTensor *> &inputs,
                                            const std::vector<kernel::KernelTensor *> &,
                                            const std::vector<kernel::KernelTensor *> &outputs) {
  bool res = false;
  switch (in_type_) {
    case kNumberTypeFloat16:
      res = LaunchKernel<float16>(inputs, outputs);
      break;
    case kNumberTypeFloat32:
      res = LaunchKernel<float>(inputs, outputs);
      break;
    default:
      MS_EXCEPTION(TypeError) << "For '" << kernel_name_ << "', the dtype of 'x' should be float16, float32, but got "
                              << TypeIdLabel(in_type_);
  }
  return res;
}

template <typename T>
bool InstanceNormV2GradCpuKernelMod::LaunchKernel(const std::vector<kernel::KernelTensor *> &inputs,
                                                  const std::vector<kernel::KernelTensor *> &outputs) {
  const int64_t batch = dy_shape_4d_[kIndex0];
  const int64_t channel = dy_shape_4d_[kIndex3];
  const int64_t image_size = dy_shape_4d_[kIndex1] * dy_shape_4d_[kIndex2];
  std::vector<int64_t> dy_shape_3d_ = {batch, image_size, channel};
  auto dy_3d = EigenTensor(dy_shape_3d_, inputs[kIndex0]->device_ptr()).tensor<T, kDim3>();
  auto in_x_3d = EigenTensor(dy_shape_3d_, inputs[kIndex1]->device_ptr()).tensor<T, kDim3>();
  auto weight_matrix = EigenTensor(batch_channels_2d_, inputs[kIndex2]->device_ptr()).matrix<float>();
  auto running_mean_matrix = EigenTensor(batch_channels_2d_, inputs[kIndex3]->device_ptr()).matrix<float>();
  auto running_var_matrix = EigenTensor(batch_channels_2d_, inputs[kIndex4]->device_ptr()).matrix<float>();
  auto save_mean_matrix = EigenTensor(batch_channels_2d_, inputs[kIndex5]->device_ptr()).matrix<float>();
  auto save_invstd_matrix = EigenTensor(batch_channels_2d_, inputs[kIndex6]->device_ptr()).matrix<float>();

  auto dx_3d = EigenTensor(dy_shape_3d_, outputs[kIndex0]->device_ptr()).tensor<T, kDim3>();
  auto grad_weight_matrix = EigenTensor(batch_channels_2d_, outputs[kIndex1]->device_ptr()).matrix<float>();
  auto grad_bias_matrix = EigenTensor(batch_channels_2d_, outputs[kIndex2]->device_ptr()).matrix<float>();

  auto loop_batch = [&](int64_t begin, int64_t end) {
    for (int64_t idx = begin; idx < end; ++idx) {
      for (int64_t c_idx = 0; c_idx < channel; ++c_idx) {
        float w = weight_matrix(idx, c_idx);
        float mean = float_init_zero;
        float invstd = float_init_zero;
        mean = is_training_ ? save_mean_matrix(idx, c_idx) : running_mean_matrix(idx, c_idx);
        float _invstd_ = std::sqrt(running_var_matrix(idx, c_idx) + epsilon_);
        do {
          if ((_invstd_) == static_cast<float>(0)) {
            MS_LOG(EXCEPTION) << "The " << ("_invstd_") << " is zero.";
          }
        } while (0);
        invstd = is_training_ ? save_invstd_matrix(idx, c_idx) : float_init_one / _invstd_;

        double sum = double_init_zero;
        double dotp = double_init_zero;
        for (int64_t img_idx = 0; img_idx < image_size; ++img_idx) {
          sum += static_cast<double>(dy_3d(idx, img_idx, c_idx));
          dotp += (static_cast<double>(in_x_3d(idx, img_idx, c_idx)) - FloatToDouble(mean)) *
                  static_cast<double>(dy_3d(idx, img_idx, c_idx));
        }
        if (is_training_) {
          float k = static_cast<float>(dotp * FloatToDouble(invstd) * FloatToDouble(invstd) / LongToDouble(image_size));
          float grad_mean = static_cast<float>(sum / LongToDouble(image_size));
          for (int64_t img_idx = 0; img_idx < image_size; ++img_idx) {
            float _dx_ = (static_cast<float>(in_x_3d(idx, img_idx, c_idx)) - mean) * k;
            dx_3d(idx, img_idx, c_idx) =
              static_cast<T>((static_cast<float>(dy_3d(idx, img_idx, c_idx)) - grad_mean - _dx_) * invstd * w);
          }
        } else {
          for (int64_t img_idx = 0; img_idx < image_size; ++img_idx) {
            dx_3d(idx, img_idx, c_idx) = static_cast<T>(static_cast<float>(dy_3d(idx, img_idx, c_idx)) * invstd * w);
          }
        }
        grad_weight_matrix(idx, c_idx) = static_cast<float>(dotp * FloatToDouble(invstd));
        grad_bias_matrix(idx, c_idx) = static_cast<float>(sum);
      }
    }
  };

  float block_size = std::max(float_init_one, static_cast<float>(kGrainSize / (channel * image_size)));
  CPUKernelUtils::ParallelFor(loop_batch, batch, block_size);
  return true;
}

std::vector<KernelAttr> InstanceNormV2GradCpuKernelMod::GetOpSupport() {
  static std::vector<KernelAttr> support_list = {KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddInputAttr(kNumberTypeFloat16)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeFloat16)
                                                   .AddOutputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeFloat32),
                                                 KernelAttr()
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddInputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeFloat32)
                                                   .AddOutputAttr(kNumberTypeFloat32)};
  return support_list;
}

MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, InstanceNormV2Grad, InstanceNormV2GradCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
