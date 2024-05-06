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

#include "plugin/device/cpu/kernel/dropout_nd_cpu_kernel.h"
#include <algorithm>
#include <random>
#include <utility>
#include <set>
#include <map>
#include <functional>
#include "mindspore/core/ops/nn_ops.h"
#include "plugin/device/cpu/hal/device/cpu_device_address.h"
#include "mindspore/core/ops/dropout_nd.h"
#include "plugin/device/cpu/kernel/nnacl/op_base.h"
#include "plugin/device/cpu/kernel/nnacl//fp32/dropout_fp32.h"

namespace mindspore {
namespace kernel {
bool DropoutNdCpuKernelMod::CheckDropOutNdShape() {
  constexpr size_t k4d = 4;
  constexpr size_t k5d = 5;
  constexpr size_t k4d_remain_dim = 2;
  constexpr size_t k5d_remain_dim = 3;
  size_t nd_dims = input_shape_.size();
  size_t expected_dims;
  size_t last_remain_dim;
  if (kernel_name_ == prim::kPrimDropout2D->name()) {
    // Dropout2D ---> data format NCHW(4 dims)
    expected_dims = k4d;
    last_remain_dim = k4d_remain_dim;
  } else if (kernel_name_ == prim::kPrimDropout3D->name()) {
    // Dropout3D ---> data format NCDHW(5 dims)
    expected_dims = k5d;
    last_remain_dim = k5d_remain_dim;
  } else {
    MS_LOG(ERROR) << "For 'DropoutNd', it only support Dropout2D or Dropout3D, right now, but got " << kernel_name_;
    return false;
  }
  if (nd_dims != expected_dims) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << ", it's input dims must equal to " << expected_dims << "D, but got  "
                  << nd_dims << "D.";
    return false;
  }
  // Flatten input shape to [channels, XHW].
  channels_ = 1;
  for (size_t i = 0; i < nd_dims - last_remain_dim; ++i) {
    channels_ *= input_shape_.at(i);
  }
  return true;
}

bool DropoutNdCpuKernelMod::Init(const std::vector<KernelTensor *> &inputs,
                                 const std::vector<KernelTensor *> &outputs) {
  if (inputs.empty() || outputs.empty()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', it got empty inputs or outputs, which is invalid.";
    return false;
  }
  // Get Self primitive attribute by primitive.
  keep_prob_ = GetValue<float>(primitive_->GetAttr(ops::kKeepProb));
  if ((keep_prob_ < 0.0f) || (keep_prob_ > 1.0f)) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', the value of 'keep_prob' should be in range [0.0, 1.0], "
                  << "but got " << keep_prob_;
    return false;
  }
  std::bernoulli_distribution::param_type dis_param(keep_prob_);
  distribution_.param(dis_param);
  return MatchKernelFunc(kernel_name_, inputs, outputs);
}

int DropoutNdCpuKernelMod::Resize(const std::vector<KernelTensor *> &inputs,
                                  const std::vector<KernelTensor *> &outputs) {
  if (int ret = KernelMod::Resize(inputs, outputs); ret != KRET_OK) {
    return ret;
  }
  input_shape_ = LongVecToSizeVec(inputs.at(kIndex0)->GetShapeVector());
  input_elements_ =
    std::accumulate(input_shape_.begin(), input_shape_.end(), decltype(input_shape_)::value_type(1), std::multiplies{});
  if (!CheckDropOutNdShape()) {
    MS_LOG(ERROR) << "For '" << kernel_name_ << "', input dims is invalid, should be 4D or 5D "
                  << " but got " << input_shape_.size() << "D";
    return KRET_RESIZE_FAILED;
  }
  scale_ = 1.0f / keep_prob_;
  return KRET_OK;
}

template <typename T>
bool DropoutNdCpuKernelMod::LaunchKernel(const std::vector<KernelTensor *> &inputs, const std::vector<KernelTensor *> &,
                                         const std::vector<KernelTensor *> &outputs) {
  auto input = GetDeviceAddress<T>(inputs, kIndex0);
  auto output = GetDeviceAddress<T>(outputs, kIndex0);
  auto mask = GetDeviceAddress<bool>(outputs, kIndex1);
  // When keep_prob equal to 0.0, output default to zero, mask default to false.
  if (std::equal_to<float>()(keep_prob_, 0)) {
    auto ret = memset_s(output, outputs.at(kIndex0)->size(), 0, outputs.at(kIndex0)->size());
    if (ret != EOK) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it does memset_s error.";
    }
    ret = memset_s(mask, outputs.at(kIndex1)->size(), 0, outputs.at(kIndex1)->size());
    if (ret != EOK) {
      MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it does memset_s error.";
    }
    return true;
  }
  size_t inner_size = input_elements_ / channels_;
  int ret_code = EOK;
  for (size_t i = 0; i < channels_; ++i) {
    bool keep = distribution_(generator_);
    if (keep) {
      std::fill(mask, mask + inner_size, keep);
      if constexpr (std::is_same<T, float>::value) {
        DropoutFp32(input, scale_, SizeToInt(inner_size), output);
      } else {
        for (size_t j = 0; j < inner_size; ++j) {
          output[j] = static_cast<T>(scale_ * static_cast<float>(input[j]));
        }
      }
    } else {
      auto temp_code = memset_s(mask, inner_size * sizeof(bool), 0, inner_size * sizeof(bool));
      if (temp_code != EOK) {
        ret_code = temp_code;
        break;
      }
      temp_code = memset_s(output, inner_size * sizeof(T), 0, inner_size * sizeof(T));
      if (temp_code != EOK) {
        ret_code = temp_code;
        break;
      }
    }
    input += inner_size;
    output += inner_size;
    mask += inner_size;
  }
  if (ret_code != EOK) {
    MS_LOG(EXCEPTION) << "For '" << kernel_name_ << "', it launch kernel failed, error code: " << ret_code;
  }
  return true;
}

const std::vector<std::pair<KernelAttr, DropoutNdCpuKernelMod::KernelRunFunc>> &DropoutNdCpuKernelMod::GetFuncList()
  const {
  static const std::vector<std::pair<KernelAttr, DropoutNdCpuKernelMod::KernelRunFunc>> func_list = {
    {KernelAttr().AddInputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeInt8).AddOutputAttr(kNumberTypeBool),
     &DropoutNdCpuKernelMod::LaunchKernel<int8_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeInt16).AddOutputAttr(kNumberTypeBool),
     &DropoutNdCpuKernelMod::LaunchKernel<int16_t>},
    {KernelAttr().AddInputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeInt32).AddOutputAttr(kNumberTypeBool),
     &DropoutNdCpuKernelMod::LaunchKernel<int>},
    {KernelAttr().AddInputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeInt64).AddOutputAttr(kNumberTypeBool),
     &DropoutNdCpuKernelMod::LaunchKernel<int64_t>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeFloat32).AddOutputAttr(kNumberTypeBool),
     &DropoutNdCpuKernelMod::LaunchKernel<float>},
    {KernelAttr().AddInputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeFloat64).AddOutputAttr(kNumberTypeBool),
     &DropoutNdCpuKernelMod::LaunchKernel<double>},
  };
  return func_list;
}
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Dropout2D, DropoutNdCpuKernelMod);
MS_KERNEL_FACTORY_REG(NativeCpuKernelMod, Dropout3D, DropoutNdCpuKernelMod);
}  // namespace kernel
}  // namespace mindspore
